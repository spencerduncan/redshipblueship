/**
 * @file relay_protocol.c
 * @brief Wire codec for the grant relay (ADR 0007 §2, §3.4). Pure bytes.
 *
 * Every integer is read and written a byte at a time, little-endian, rather
 * than memcpy'd from a packed struct. The server memcpy's native structs on
 * x86-64, so LE is the wire truth; doing it by hand means this file produces
 * identical bytes on a big-endian host and needs no packing pragma.
 *
 * Nothing here allocates, blocks, or touches a socket or gComboCtx. That is
 * what makes the golden-vector tests in test_netplay_relay.c able to pin the
 * exact wire bytes without a transport.
 */

#include "relay_protocol.h"

#include <string.h>

// ============================================================================
// Little-endian scalar helpers
// ============================================================================

static void PutU16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void PutU32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void PutU64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        p[i] = (uint8_t)((v >> (8 * i)) & 0xFFu);
    }
}

static uint16_t GetU16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t GetU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t GetU64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) {
        v = (v << 8) | (uint64_t)p[i];
    }
    return v;
}

static bool IsRealGame(uint8_t g) {
    return g == (uint8_t)GAME_OOT || g == (uint8_t)GAME_MM;
}

// ============================================================================
// Grant payload
// ============================================================================

bool RelayProto_EncodeGrant(const RsbsGrantPayload* grant, uint8_t* out) {
    if (grant == NULL || out == NULL) {
        return false;
    }
    // Refuse to put a malformed grant on the wire. These are exactly the
    // conditions the receiving seam would reject with RSBS_GRANT_REJECTED, so
    // catching them here keeps a local bug from looking like a peer's.
    if (grant->senderSlot == 0 || grant->targetSlot == 0 || grant->senderSeq == 0) {
        return false;
    }
    if (!IsRealGame(grant->originGame)) {
        return false;
    }
    if (grant->version != RSBS_GRANT_PAYLOAD_VERSION) {
        return false;
    }

    memcpy(out, RSBS_GRANT_PAYLOAD_MAGIC, RSBS_GRANT_PAYLOAD_MAGIC_LEN);
    out[4] = grant->version;
    out[5] = grant->senderSlot;
    out[6] = grant->targetSlot;
    out[7] = grant->originGame;
    PutU16(out + 8, grant->itemId);
    PutU16(out + 10, grant->senderSeq);
    PutU32(out + 12, grant->settingsHash);
    return true;
}

bool RelayProto_DecodeGrant(const uint8_t* in, uint32_t len, RsbsGrantPayload* out) {
    if (in == NULL || out == NULL) {
        return false;
    }
    // A foreign payload is NOT an error (ADR 0007 §4.3) — the ledger is
    // shared-format by design, so an OoTMM client's entries land in the same
    // room. Every rejection below is "skip and count", never "disconnect".
    if (len != RSBS_GRANT_PAYLOAD_LEN) {
        return false;
    }
    if (memcmp(in, RSBS_GRANT_PAYLOAD_MAGIC, RSBS_GRANT_PAYLOAD_MAGIC_LEN) != 0) {
        return false;
    }
    if (in[4] != RSBS_GRANT_PAYLOAD_VERSION) {
        return false;
    }

    RsbsGrantPayload g;
    g.version = in[4];
    g.senderSlot = in[5];
    g.targetSlot = in[6];
    g.originGame = in[7];
    g.itemId = GetU16(in + 8);
    g.senderSeq = GetU16(in + 10);
    g.settingsHash = GetU32(in + 12);

    if (g.senderSlot == 0 || g.targetSlot == 0 || g.senderSeq == 0) {
        return false;
    }
    if (!IsRealGame(g.originGame)) {
        return false;
    }

    *out = g;
    return true;
}

uint64_t RelayProto_GrantKey(const RsbsGrantPayload* grant) {
    if (grant == NULL) {
        return 0;
    }
    return ((uint64_t)grant->senderSlot << 48) | ((uint64_t)grant->senderSeq << 16) | (uint64_t)grant->itemId;
}

// ============================================================================
// Frame encoders (client -> server)
// ============================================================================

void RelayProto_EncodeHello(uint8_t* out) {
    memcpy(out, RSBS_RELAY_MAGIC, RSBS_RELAY_MAGIC_LEN);
    PutU32(out + RSBS_RELAY_MAGIC_LEN, RSBS_RELAY_PROTOCOL_VERSION);
}

void RelayProto_EncodeJoin(const uint8_t roomUuid[RSBS_RELAY_UUID_LEN], uint32_t ledgerBase, uint8_t* out) {
    memcpy(out, roomUuid, RSBS_RELAY_UUID_LEN);
    PutU32(out + RSBS_RELAY_UUID_LEN, ledgerBase);
}

uint32_t RelayProto_EncodeTransfer(uint64_t key, const uint8_t* payload, uint32_t payloadLen, uint8_t* out) {
    if (out == NULL || payload == NULL || payloadLen > RSBS_RELAY_MAX_PAYLOAD) {
        return 0;
    }
    out[0] = RSBS_RELAY_OP_TRANSFER;
    PutU64(out + 1, key);
    out[9] = (uint8_t)payloadLen;
    memcpy(out + 10, payload, payloadLen);
    return 10u + payloadLen;
}

// ============================================================================
// Incremental decoder (server -> client)
// ============================================================================

RelayDecodeStatus RelayProto_DecodeFrame(const uint8_t* buf, uint32_t len, uint32_t* consumed, RelayLedgerEntry* entry) {
    if (consumed != NULL) {
        *consumed = 0;
    }
    if (buf == NULL || consumed == NULL || len == 0) {
        return RSBS_RELAY_DECODE_NEED_MORE;
    }

    switch (buf[0]) {
        case RSBS_RELAY_OP_NONE:
            // Keepalive. The server emits these from multiClientEventTimer
            // whenever it has been quiet for a few ticks, so a decoder that
            // treated a stray 0x00 as a bad opcode would drop a perfectly
            // healthy connection after a few idle seconds.
            *consumed = 1;
            return RSBS_RELAY_DECODE_NOP;

        case RSBS_RELAY_OP_TRANSFER: {
            if (len < 1u + RSBS_RELAY_ENTRY_HEADER_LEN) {
                return RSBS_RELAY_DECODE_NEED_MORE;
            }
            // op(1) + key(8) => the size byte is at offset 9, payload at 10.
            const uint32_t payloadLen = buf[9];
            if (payloadLen > RSBS_RELAY_MAX_PAYLOAD) {
                // The server refuses to STORE an oversized entry, so it should
                // never emit one. Treat it as a broken stream rather than
                // guessing a resync point: the length prefix is the only thing
                // keeping us framed, so a bad one means we are already lost.
                return RSBS_RELAY_DECODE_ERROR;
            }
            const uint32_t total = 1u + RSBS_RELAY_ENTRY_HEADER_LEN + payloadLen;
            if (len < total) {
                return RSBS_RELAY_DECODE_NEED_MORE;
            }
            if (entry != NULL) {
                entry->key = GetU64(buf + 1);
                entry->payloadLen = payloadLen;
                if (payloadLen > 0) {
                    memcpy(entry->payload, buf + 10, payloadLen);
                }
            }
            *consumed = total;
            return RSBS_RELAY_DECODE_ENTRY;
        }

        case RSBS_RELAY_OP_MSG: {
            // op + u8 size + u16 senderId + text[size]. We decode it purely to
            // stay framed, then discard: the relay carries grants, and routing
            // peer chat into the game is not in ADR 0007's scope.
            if (len < 4u) {
                return RSBS_RELAY_DECODE_NEED_MORE;
            }
            const uint32_t textLen = buf[1];
            const uint32_t total = 4u + textLen;
            if (len < total) {
                return RSBS_RELAY_DECODE_NEED_MORE;
            }
            *consumed = total;
            return RSBS_RELAY_DECODE_MSG;
        }

        default:
            return RSBS_RELAY_DECODE_ERROR;
    }
}
