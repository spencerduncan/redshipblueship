/**
 * @file relay_client.h
 * @brief Grant-relay client: state machine, bounded inbox, suspend latch.
 *        ADR 0007. No transport here — see RelayChannel.
 *
 * This object turns a byte stream into calls on ADR 0005's producer seam
 * (`Combo_SubmitSourcedGrant`). It is the ONLY netplay path into the grant
 * model; it opens no second route into `Combo_RecordSharedItem` (#460).
 *
 * ADR 0006 §5's salvage list is carried STRUCTURALLY, not as guidance — each
 * item closes a verified hazard in the vendored Anchor client:
 *
 *   1. NO RECEIVE THREAD (Anchor hazards #1, #2). The socket is serviced only
 *      inside Relay_Tick(), which the caller invokes on the game thread. There
 *      is no background thread to marshal from, so ADR 0005's "game thread
 *      only" seam contract holds by construction rather than by discipline.
 *   2. BOUNDED INBOX WITH RECOVERABLE DROP (Anchor hazard #3). Fixed ring; on
 *      overflow we drop WITHOUT advancing the grant cursor and set a replay
 *      flag. Bounded memory, zero item loss — the dropped grant is re-derived
 *      by the next §3.3 replay-from-zero.
 *   3. EXPLICIT SUSPEND LATCH. Relay_OnSuspend() stops APPLYING; polling and
 *      decoding continue into the inbox. Relay_OnResume() drains in received
 *      order. The lifecycle has no implicit inverse, so the release is a
 *      separate named action.
 *   4. ROOM BINDING, refuse-on-mismatch — and it costs no new state, because
 *      `sourceKey` IS the room fingerprint (ADR 0007 §4.5). A different room
 *      derives a different key, hence a different ADR 0005 source slot, hence
 *      a fresh seq 1. A cursor can never be applied against a room that did
 *      not produce it.
 *
 * THREADING: game thread only, like the seam it feeds.
 *
 * SESSION SCOPE: the inbox is session-scoped RAM. Per the #440 guidance on
 * #460, it is cleared FROM Context_InvalidateSessionState (via
 * Relay_ClearSessionState) rather than at our own call sites, so there stays
 * one list of "things a dead session owns".
 */

#ifndef RSBS_COMMON_NETPLAY_RELAY_CLIENT_H
#define RSBS_COMMON_NETPLAY_RELAY_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "relay_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bounded inbox depth (salvage item 2). Sized well above a realistic burst:
// the whole point is that exceeding it costs latency, never items.
#define RSBS_RELAY_INBOX_CAP 64u

// Receive accumulation buffer. Must exceed the largest single frame
// (1 + 9 + 128 = 138) with room to hold a partial next frame.
#define RSBS_RELAY_RX_CAP 1024u

// Outbound staging depth for locally-produced grants awaiting a writable
// channel.
#define RSBS_RELAY_OUTBOX_CAP 32u

/**
 * Transport abstraction. The loopback harness and a real TCP socket implement
 * the SAME interface, so the harness drives the identical codec and state
 * machine a real peer would — only the syscall layer differs. That is the
 * honest form of the loopback claim (ADR 0007 §8).
 *
 * Both callbacks are non-blocking:
 *   > 0  bytes transferred
 *   == 0 would block / nothing available
 *   < 0  fatal channel error
 */
typedef struct {
    int (*send)(void* userData, const uint8_t* data, uint32_t len);
    int (*recv)(void* userData, uint8_t* data, uint32_t len);
    void* userData;
} RelayChannel;

typedef enum {
    RSBS_RELAY_STATE_IDLE = 0,   // never connected; no outbound anything
    RSBS_RELAY_STATE_HELLO,      // hello sent, awaiting the 11-byte server hello
    RSBS_RELAY_STATE_JOINING,    // join sent, awaiting ledger stream
    RSBS_RELAY_STATE_READY,      // streaming
    RSBS_RELAY_STATE_FAILED,     // terminal: version mismatch or channel error
} RelayState;

typedef struct {
    RelayState state;

    RelayChannel channel;
    bool channelSet;

    uint8_t roomUuid[RSBS_RELAY_UUID_LEN];
    uint32_t sourceKey; // fnv1a32(roomUuid), forced nonzero — ADR 0007 §3.1
    uint8_t localSlot;  // our slot; entries with targetSlot != this are skipped
    uint32_t settingsHash;
    uint16_t serverClientId;

    // Receive accumulation.
    uint8_t rx[RSBS_RELAY_RX_CAP];
    uint32_t rxLen;

    // Bounded inbox of grants addressed to us, in ledger order.
    RsbsGrantPayload inbox[RSBS_RELAY_INBOX_CAP];
    uint32_t inboxHead;
    uint32_t inboxCount;

    // Outbound staging.
    uint8_t outbox[RSBS_RELAY_OUTBOX_CAP][RSBS_RELAY_MAX_PAYLOAD + 10u];
    uint32_t outboxLen[RSBS_RELAY_OUTBOX_CAP];
    uint32_t outboxHead;
    uint32_t outboxCount;
    // How much of the HEAD outbox frame has already reached the channel. A
    // real socket can accept a prefix and then block; restarting from zero
    // would put those bytes on the wire twice and desync the server's parser.
    uint32_t outboxSent;
    // Same idea for the 20-byte join, which is owed once the server hello has
    // been consumed and so cannot simply be re-derived from the state alone.
    uint32_t joinSent;

    // The dense per-room seq we assign to entries addressed to us (§3.2). This
    // is NOT the ledger index: we skip other players' entries, and a hole would
    // be a permanent RSBS_GRANT_GAP.
    uint32_t nextSeq;

    // Our own outbound counter, dense and 1-based per ADR 0007 §2.
    uint16_t sendSeq;

    bool suspended;    // latch (salvage item 3)
    bool replayNeeded; // inbox overflow or a RETRY_FULL: re-derive by rejoining

    // Diagnostics — all operator-visible, none of it load-bearing.
    uint32_t statEntriesSeen;
    uint32_t statForeignSkipped;
    uint32_t statNotForUs;
    uint32_t statAccepted;
    uint32_t statDuplicate;
    uint32_t statGap;
    uint32_t statRetryFull;
    uint32_t statInboxDropped;
    bool settingsMismatchSeen;
} RelayClient;

/**
 * Derive the ADR 0005 sourceKey from a room UUID (FNV-1a 32, forced nonzero).
 * Exposed because it IS the room-fingerprint binding (§4.5) and tests assert
 * that different rooms cannot share a cursor.
 */
uint32_t Relay_SourceKeyForRoom(const uint8_t roomUuid[RSBS_RELAY_UUID_LEN]);

/**
 * Initialise. Does NOT connect and does NOT touch the channel: an operator
 * action is always required (ADR 0007 §7 — no auto-connect, ever).
 */
void Relay_Init(RelayClient* c, const uint8_t roomUuid[RSBS_RELAY_UUID_LEN], uint8_t localSlot, uint32_t settingsHash);

/**
 * Attach a transport and begin the handshake. Sends the client hello and joins
 * at ledgerBase 0 — always zero, per ADR 0007 §3.3: the grant cursor de-dups
 * the already-delivered prefix, so we never persist a second cursor.
 */
bool Relay_Connect(RelayClient* c, const RelayChannel* channel);

/**
 * Pump the transport. Call once per frame on the GAME THREAD. Drains the
 * socket, decodes frames, files grants addressed to us into the inbox, and —
 * unless suspended — applies them to the ADR 0005 seam in received order.
 *
 * @return the number of grants ACCEPTED by the seam this call.
 */
int Relay_Tick(RelayClient* c);

/**
 * Queue a grant TO a peer. Local-side production: this does not touch our own
 * grant model at all (we are giving, not receiving).
 * @return true if staged for send.
 */
bool Relay_SendGrant(RelayClient* c, uint8_t targetSlot, GameId originGame, uint16_t itemId);

/** Suspend latch (salvage item 3): stop applying; keep polling. */
void Relay_OnSuspend(RelayClient* c);

/** Release the latch and resume applying, in received order. */
void Relay_OnResume(RelayClient* c);

/**
 * Retire session-scoped RAM. Called FROM Context_InvalidateSessionState so a
 * dead session's undelivered grants cannot drain into the next session — the
 * same treatment Combo_ClearSharedItemOutbox() gets (#440 guidance on #460).
 */
void Relay_ClearSessionState(RelayClient* c);

/** True if the inbox overflowed or a grant was refused for capacity. */
bool Relay_ReplayNeeded(const RelayClient* c);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_NETPLAY_RELAY_CLIENT_H
