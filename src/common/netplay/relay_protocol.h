/**
 * @file relay_protocol.h
 * @brief Wire codec for the grant relay (ADR 0007). Pure bytes — no sockets.
 *
 * This layer implements the OoTMM `multi-server` wire format
 * (https://github.com/OoTMM/multi-server, MIT, `master` @ 2025-03-27,
 * VERSION 0x00000200). We speak its protocol; we vendor none of its code and
 * we ship no server. See docs/adr/0007-grant-relay-netplay.md §1.
 *
 * WHY WE CAN REUSE IT (ADR 0007 §1.2): the server's ledger entry is
 * `{u64 key; u8 size; opaque payload}` and `multiLedgerWrite` never inspects
 * the payload. There is no game identity, no slot registry, no generated
 * world — so unlike Archipelago (ADR 0006 §1.1) there is NO admissibility
 * check a client can fail. An unmodified multi-server relays RSBS grants
 * today because it cannot tell them from OoTMM's.
 *
 * WIRE FORMAT, transcribed from the server source and pinned by golden byte
 * vectors in test_netplay_relay.c. All multi-byte integers are LITTLE-ENDIAN
 * (the server memcpy's native structs on x86-64 Linux), and the codec below
 * reads/writes them byte-by-byte so a big-endian host produces the same bytes.
 *
 *   Handshake, client -> server (9 bytes):
 *       "OOMM2" + u32 clientVersion
 *   Handshake, server -> client (11 bytes):
 *       "OOMM2" + u32 serverVersion + u16 assignedClientId
 *   Join, client -> server (20 bytes):
 *       u8 roomUuid[16] + u32 ledgerBase
 *   Then a command stream in both directions:
 *       OP_NONE     (0x00)  1 byte. Keepalive NOP — the server emits one every
 *                           few seconds from multiClientEventTimer. MUST be
 *                           tolerated mid-stream or the client desyncs.
 *       OP_TRANSFER (0x01)  server->client: u64 key + u8 size + payload[size]
 *                           client->server: u64 key + u8 size + payload[size]
 *       OP_MSG      (0x02)  server->client: u8 size + u16 senderId + text[size]
 *                           (chat; we decode and discard — see relay_client.c)
 *
 * ECHO ASYMMETRY, and it is load-bearing for the harness: `multiClientCmdTransfer`
 * fans a new entry out to EVERY client on the ledger **including the sender**
 * (it does not skip `client->id`), whereas `multiClientCmdMsg` explicitly does
 * skip the sender. So a client receives its own grants back and MUST filter by
 * targetSlot. A mock that conveniently skipped the sender would hide that —
 * exactly the shortcut ADR 0006 §2(b) caught in the AP mock.
 *
 * The RSBS payload (ADR 0007 §2) is ours and opaque to the server. Foreign or
 * malformed payloads (an OoTMM client sharing the room) are skipped, never
 * fatal: the server's own length prefix keeps the stream framed regardless.
 */

#ifndef RSBS_COMMON_NETPLAY_RELAY_PROTOCOL_H
#define RSBS_COMMON_NETPLAY_RELAY_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "../game.h" // GameId

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Server protocol constants (pinned to multi-server master @ 2025-03-27)
// ============================================================================

#define RSBS_RELAY_MAGIC "OOMM2"
#define RSBS_RELAY_MAGIC_LEN 5u

// The server's VERSION. We refuse a connection whose high 16 bits differ
// rather than proceeding hopefully into a misparse (ADR 0007 §3.4).
#define RSBS_RELAY_PROTOCOL_VERSION 0x00000200u
#define RSBS_RELAY_VERSION_MAJOR(v) ((v) >> 16)

#define RSBS_RELAY_HELLO_LEN 9u  // "OOMM2" + u32 version
#define RSBS_RELAY_SVHELLO_LEN 11u // "OOMM2" + u32 version + u16 clientId
#define RSBS_RELAY_JOIN_LEN 20u  // uuid[16] + u32 ledgerBase
#define RSBS_RELAY_UUID_LEN 16u

// Command opcodes.
#define RSBS_RELAY_OP_NONE 0x00u
#define RSBS_RELAY_OP_TRANSFER 0x01u
#define RSBS_RELAY_OP_MSG 0x02u

// The server disconnects a client whose OP_TRANSFER header.size exceeds this
// (client.c: `if (header.size > 128)`). Our payload is far smaller; this is
// here so we can never emit a frame that gets us dropped.
#define RSBS_RELAY_MAX_PAYLOAD 128u

// OP_TRANSFER wire header: u64 key + u8 size.
#define RSBS_RELAY_ENTRY_HEADER_LEN 9u

// ============================================================================
// The RSBS grant payload (ADR 0007 §2) — ours, opaque to the server
// ============================================================================

#define RSBS_GRANT_PAYLOAD_LEN 16u
#define RSBS_GRANT_PAYLOAD_VERSION 1u
#define RSBS_GRANT_PAYLOAD_MAGIC "RSBS"
#define RSBS_GRANT_PAYLOAD_MAGIC_LEN 4u

/**
 * The encoded 16-byte layout, which is the authority (the struct below holds
 * only the 12 bytes that are DATA — the magic is a constant, not a field):
 *
 *   off  size  field
 *    0    4    magic  "RSBS"
 *    4    1    version
 *    5    1    senderSlot
 *    6    1    targetSlot
 *    7    1    originGame
 *    8    2    itemId        (LE)
 *   10    2    senderSeq     (LE)
 *   12    4    settingsHash  (LE)
 *   ----------------------------
 *              16 bytes total
 *
 * ADR 0007 §2's table listed a trailing `reserved[2]`, which would have made
 * the payload 18 bytes rather than the 16 it claimed. The layout above is the
 * correct one and the ADR is corrected to match in this change. Growth room is
 * not lost: the server's cap is 128 bytes, so a v2 payload simply gets a new
 * `version` byte and a longer encoding — the decoder already rejects unknown
 * versions by returning false (skip, not fatal), so old clients degrade to
 * "skipped a grant they could not read" rather than misparsing one.
 */

/**
 * One grant on the wire. Decoded/encoded field-by-field (never memcpy'd over
 * the wire bytes) so there is no packing or endianness assumption.
 */
typedef struct {
    uint8_t version;       // RSBS_GRANT_PAYLOAD_VERSION
    uint8_t senderSlot;    // 1..255, self-asserted (trust model: ADR 0007 §6)
    uint8_t targetSlot;    // 1..255, intended recipient
    uint8_t originGame;    // GameId — the id-space owner (ADR 0002)
    uint16_t itemId;       // RG_* if originGame == GAME_OOT, RI_* if GAME_MM
    uint16_t senderSeq;    // sender's own dense 1-based counter
    uint32_t settingsHash; // ADVISORY mismatch warning only (ADR 0007 §5.3)
} RsbsGrantPayload;

/**
 * Encode a grant payload into exactly RSBS_GRANT_PAYLOAD_LEN bytes.
 * @return true on success; false if `out` is NULL or the grant is malformed
 *         (slot 0, seq 0, or originGame not a real game).
 */
bool RelayProto_EncodeGrant(const RsbsGrantPayload* grant, uint8_t* out);

/**
 * Decode a grant payload from exactly `len` bytes.
 *
 * @return true if the bytes are a well-formed RSBS grant payload. Returns
 *         FALSE — not an error, per ADR 0007 §4.3 — for a foreign payload (bad
 *         magic, e.g. an OoTMM client in the same room), an unknown version, a
 *         wrong length, or a malformed field. The caller skips and counts it;
 *         it must never disconnect or desync on this.
 */
bool RelayProto_DecodeGrant(const uint8_t* in, uint32_t len, RsbsGrantPayload* out);

/**
 * The ledger key for a grant (ADR 0007 §2):
 *   (senderSlot << 48) | (senderSeq << 16) | itemId
 *
 * Unique per (sender, sequence), so the server's keySet absorbs a sender's own
 * retransmit — while two DIFFERENT senders gifting the same item produce
 * different keys and both survive. That second half matters: server-side dedup
 * collapsing two legitimate gifts would reintroduce, one layer lower, the exact
 * bug ADR 0005 §1 exists to fix.
 */
uint64_t RelayProto_GrantKey(const RsbsGrantPayload* grant);

// ============================================================================
// Frame encoders (client -> server)
// ============================================================================

/** Write the 9-byte client hello. `out` must hold RSBS_RELAY_HELLO_LEN. */
void RelayProto_EncodeHello(uint8_t* out);

/** Write the 20-byte join. `out` must hold RSBS_RELAY_JOIN_LEN. */
void RelayProto_EncodeJoin(const uint8_t roomUuid[RSBS_RELAY_UUID_LEN], uint32_t ledgerBase, uint8_t* out);

/**
 * Write an OP_TRANSFER frame: op + u64 key + u8 size + payload.
 * `out` must hold 1 + RSBS_RELAY_ENTRY_HEADER_LEN + payloadLen.
 * @return the number of bytes written, or 0 if payloadLen > RSBS_RELAY_MAX_PAYLOAD.
 */
uint32_t RelayProto_EncodeTransfer(uint64_t key, const uint8_t* payload, uint32_t payloadLen, uint8_t* out);

// ============================================================================
// Incremental decoder (server -> client)
// ============================================================================

typedef enum {
    RSBS_RELAY_DECODE_NEED_MORE = 0, // incomplete frame; keep buffering
    RSBS_RELAY_DECODE_NOP,           // OP_NONE keepalive consumed
    RSBS_RELAY_DECODE_ENTRY,         // a ledger entry was decoded
    RSBS_RELAY_DECODE_MSG,           // a chat message was consumed (discarded)
    RSBS_RELAY_DECODE_ERROR,         // unknown opcode — the stream is unusable
} RelayDecodeStatus;

typedef struct {
    uint64_t key;
    uint8_t payload[RSBS_RELAY_MAX_PAYLOAD];
    uint32_t payloadLen;
} RelayLedgerEntry;

/**
 * Try to decode one frame from the front of `buf`.
 *
 * @param consumed set to the number of bytes consumed (0 when NEED_MORE).
 * @param entry    populated only when the result is RSBS_RELAY_DECODE_ENTRY.
 *
 * Tolerating OP_NONE is not optional: the server emits keepalive NOPs from
 * multiClientEventTimer whenever it has been quiet, and a decoder that treated
 * a stray 0x00 as a bad opcode would drop a healthy connection after a few
 * idle seconds.
 */
RelayDecodeStatus RelayProto_DecodeFrame(const uint8_t* buf, uint32_t len, uint32_t* consumed, RelayLedgerEntry* entry);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_NETPLAY_RELAY_PROTOCOL_H
