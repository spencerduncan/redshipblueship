/**
 * @file netplay.h
 * @brief Archipelago netplay transport for RedShipBlueShip (increment 1b).
 *
 * Compiled ONLY when the build is configured with -DRSBS_NETPLAY=ON; the
 * default build contains none of these symbols (mirroring how the vendored
 * Anchor client's ENABLE_REMOTE_CONTROL is never declared). Nothing in this
 * module ever connects on its own: a connection exists only between an
 * explicit Netplay_Connect and Netplay_Disconnect/Netplay_Shutdown.
 *
 * Design (docs/adr/0006-netplay-archipelago-transport.md):
 *
 * THREADING — single-threaded by construction. There is NO receive thread.
 * apclientpp's APClient is a poll-driven client: the socket is serviced only
 * inside Netplay_Tick(), and every callback fires on the thread that called
 * it. All Netplay_* APIs must be called from the thread that called
 * Netplay_Init (the game/main thread); each entry point checks this and
 * refuses (with a logged error) off-thread calls. This is the explicit
 * anti-Anchor decision: Anchor hazard #2 (PLAYER_UPDATE mutated shared state
 * on the network thread with no mutex) cannot exist here because there is no
 * network thread and therefore no cross-thread state at all.
 *
 * LIFECYCLE ACROSS A GAME SWITCH — Anchor hazards #1/#3 (receive thread
 * survives Game_Suspend; unbounded queue growth while the consumer stops) are
 * closed structurally:
 *   - No thread to survive: while nothing calls Netplay_Tick (the switch
 *     window), nothing is received in-process; backpressure is TCP's.
 *   - Netplay_OnGameSuspend latches "suspended": Tick keeps servicing the
 *     socket (keepalive, handshake) but never applies a grant.
 *   - The receive inbox is a fixed-capacity buffer (RSBS_NETPLAY_INBOX_CAP).
 *     On overflow the remainder of the batch is DROPPED WITHOUT advancing the
 *     applied cursor and a resync is scheduled — Archipelago's
 *     server-authoritative index makes a later Sync re-deliver exactly the
 *     dropped tail, so bounded memory costs no items.
 *   - Netplay_OnGameResume clears the latch; the next Tick drains the inbox
 *     in received (index) order.
 *
 * ORDERING & IDEMPOTENCY — Archipelago owns the cursor. Every received item
 * carries a monotonic index; this module applies items strictly in index
 * order, exactly once, tracking progress in
 * gComboCtx.netplayApAppliedCount (persisted in every .redsave, see
 * context.h). Retransmits (index < cursor) are discarded; gaps latch a
 * resync. The RSBS_SHARED_ITEM_REDEEMED bit is thereby demoted to a local
 * idempotency guard beneath AP's index, as the increment-1 spike requires.
 *
 * ROOM BINDING — the cursor is only meaningful against the room that
 * produced it. On slot connect the transport fingerprints (seed_name, slot)
 * and compares it to gComboCtx.netplayApRoomFingerprint: unset binds, match
 * resumes, mismatch REFUSES to apply anything until the operator explicitly
 * calls Netplay_RebindRoom (which resets the cursor to 0 — correct for a
 * genuinely new world, catastrophic if done silently, hence explicit).
 *
 * GRANT SINK — item application is delegated to a registered sink so this
 * module never hardwires a path into the shared-item machinery. The sink for
 * the current tree is installed by Netplay_InstallSharedItemSink (see
 * netplay_sink.h for the seam contract and the #460 coordination notes).
 */

#ifndef RSBS_NETPLAY_NETPLAY_H
#define RSBS_NETPLAY_NETPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h" // GameId

#ifdef __cplusplus
extern "C" {
#endif

/** Fixed capacity of the receive inbox (decoded-but-unapplied grants). */
#define RSBS_NETPLAY_INBOX_CAP 128u

typedef enum {
    NETPLAY_STATE_UNINITIALIZED = 0, // Netplay_Init not called (or Shutdown)
    NETPLAY_STATE_IDLE,              // initialized, no connection requested
    NETPLAY_STATE_CONNECTING,        // socket/handshake in progress
    NETPLAY_STATE_ROOM_INFO,         // server hello received, slot not joined
    NETPLAY_STATE_ACTIVE,            // slot connected; grants flow
    NETPLAY_STATE_DISCONNECTED       // was connected, connection lost (Tick retries)
} NetplayConnectionState;

typedef struct {
    NetplayConnectionState state;
    bool suspended;         // Game_Suspend latch: polling yes, applying no
    bool roomMismatch;      // connected room does not match the persisted cursor
    bool resyncScheduled;   // a gap/overflow occurred; Sync will be requested
    uint32_t pendingGrants; // decoded grants waiting in the bounded inbox
    uint32_t appliedCount;  // mirrors gComboCtx.netplayApAppliedCount
    uint32_t droppedForResync; // grants dropped to keep the inbox bounded (recoverable)
    uint32_t decodeFailures;   // received items outside the RSBS id mapping (skipped)
} NetplayStatus;

/**
 * Initialize the transport. Must be called before any other Netplay_* API,
 * on the thread that will drive it (the game/main thread). Does NOT connect.
 * Returns false if already initialized.
 */
bool Netplay_Init(void);

/**
 * Tear down: disconnects (if connected) and releases everything.
 * Safe to call when never initialized.
 */
void Netplay_Shutdown(void);

/**
 * Begin connecting to an Archipelago server. Explicit opt-in only — nothing
 * else in the module (and nothing anywhere in a default build) opens a
 * socket. The connection is serviced by subsequent Netplay_Tick calls.
 *
 * @param uri      e.g. "ws://127.0.0.1:38281". Increment 1 is built with
 *                 WSWRAP_NO_SSL, so the scheme must be explicit "ws://";
 *                 "wss://" requires an OpenSSL-enabled build (deferred).
 * @param slotName the Archipelago slot to join
 * @param password room password ("" for none)
 * @return false on bad args / wrong thread / not initialized.
 */
bool Netplay_Connect(const char* uri, const char* slotName, const char* password);

/** Drop the connection (if any). The transport returns to NETPLAY_STATE_IDLE. */
void Netplay_Disconnect(void);

/**
 * Service the transport: pump the socket, run the handshake, and — when
 * ACTIVE, not suspended, room-matched, and a sink is installed — apply
 * pending grants in index order through the sink. Call once per frame from
 * the owning thread. Cheap when idle.
 */
void Netplay_Tick(void);

/**
 * Game-switch gating (see file header). Call OnGameSuspend before the
 * departing game's Game_Suspend side effects matter, OnGameResume once the
 * arriving game is live. Idempotent.
 */
void Netplay_OnGameSuspend(GameId departing);
void Netplay_OnGameResume(GameId arriving);

/**
 * Adopt the currently connected room as this save's room: stores its
 * fingerprint and RESETS the applied cursor to 0, so the server's full item
 * stream will be (re-)applied from the start. Only meaningful while
 * roomMismatch is set; a deliberate operator action, never automatic.
 * Returns false if not connected or not mismatched.
 */
bool Netplay_RebindRoom(void);

/** Snapshot of the transport state (owning thread only). */
NetplayStatus Netplay_GetStatus(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_NETPLAY_NETPLAY_H
