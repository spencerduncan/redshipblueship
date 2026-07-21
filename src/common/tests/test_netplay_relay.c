/**
 * @file test_netplay_relay.c
 * @brief Loopback locks for the grant relay (ADR 0007). ROM-free, no sockets.
 *
 * WHAT THIS PROVES, and equally what it does NOT — stated here because ADR
 * 0006 §2(b) caught the previous attempt's harness going green while real
 * integration stayed impossible, and the honest statement is the deliverable:
 *
 * PROVES. Two client objects drive the REAL codec and the REAL state machine
 * over a mock ledger implementing the same semantics as OoTMM's multi-server:
 * append-only, u64-key dedup, `ledgerBase` replay, and — load-bearing —
 * broadcast to EVERY client on the ledger INCLUDING the sender. Real bytes go
 * through RelayProto_* in both directions; only the syscall layer is swapped.
 *   - a retransmitted grant yields exactly ONE item
 *   - two peers gifting the same item yield TWO items (the ADR 0005 §1 regression)
 *   - received-order redemption
 *   - grants survive a cross-game switch
 *   - late-join catch-up delivers the whole backlog
 *   - self-echo is filtered (the sender does not award itself its own gift)
 *   - inbox overflow drops WITHOUT advancing the cursor, and is loud
 *   - a foreign/malformed ledger entry is skipped without desyncing
 *   - golden byte vectors pin the wire format
 *
 * DOES NOT PROVE. The mock is a re-derivation from reading multi-server's
 * client.c/ledger.c, not the real binary — so it cannot prove our framing
 * matches the wire. Nor does it prove real socket behaviour (partial reads,
 * EAGAIN, mid-frame disconnect, TCP segmentation), nor that upstream still
 * behaves as read (pinned: master @ 2025-03-27, VERSION 0x00000200).
 *
 * The distinction from ADR 0006's gap is real and is why this is worth
 * locking: Archipelago's was a WALL — no client of ours could ever be
 * admitted, because a real server validates the announced game against a
 * generated world. multi-server's ledger never inspects the payload, so there
 * is no admissibility check to fail and a framing mismatch is a DEFECT CLASS
 * we can fix unilaterally. Different in kind, not merely in degree.
 *
 * Linkage note: #included into test_runner.cpp inside its own extern "C"
 * block — every symbol it drives is C-linkage (relay_*, Combo_*).
 */

#include "../context.h"
#include "../netplay/relay_client.h"
#include "../netplay/relay_protocol.h"
#include "../shared_items.h"
#include "../test_runner.h"

#include <stdio.h>
#include <string.h>

#define NR_ASSERT(cond)                                                                                                \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

// ============================================================================
// Mock ledger server — the semantics of multi-server, in process
// ============================================================================

#define MOCK_MAX_ENTRIES 128
#define MOCK_MAX_CLIENTS 4
#define MOCK_PIPE_CAP 8192

typedef struct {
    uint64_t key;
    uint8_t payload[RSBS_RELAY_MAX_PAYLOAD];
    uint32_t payloadLen;
} MockEntry;

// A one-directional byte pipe. Deliberately a plain FIFO of bytes, not of
// frames: the client must reassemble, which is what makes the codec's
// incremental decoder actually exercised.
typedef struct {
    uint8_t buf[MOCK_PIPE_CAP];
    uint32_t head;
    uint32_t len;
} MockPipe;

typedef struct {
    bool active;
    bool ready;       // past the handshake
    uint32_t base;    // ledgerBase: how many entries this client has been sent
    MockPipe toClient;
    MockPipe toServer;
    // Server-side parse state for the client's own stream.
    uint8_t rx[512];
    uint32_t rxLen;
    bool sawHello;
    bool sawJoin;
} MockClient;

typedef struct {
    MockEntry entries[MOCK_MAX_ENTRIES];
    uint32_t count;
    MockClient clients[MOCK_MAX_CLIENTS];
    // Set to force the ledger to reject writes, to drive backpressure paths.
    bool full;
} MockServer;

static void MockPipeWrite(MockPipe* p, const uint8_t* data, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i) {
        if (p->len >= MOCK_PIPE_CAP) {
            return; // drop; tests never intentionally overrun this
        }
        p->buf[(p->head + p->len) % MOCK_PIPE_CAP] = data[i];
        p->len++;
    }
}

static uint32_t MockPipeRead(MockPipe* p, uint8_t* out, uint32_t max) {
    uint32_t n = 0;
    while (n < max && p->len > 0) {
        out[n++] = p->buf[p->head];
        p->head = (p->head + 1u) % MOCK_PIPE_CAP;
        p->len--;
    }
    return n;
}

static MockServer gMockServer;

// --- Ledger ----------------------------------------------------------------

// Append with dedup on `key`, exactly like multiLedgerWrite: an existing key
// is silently ignored, which is the server-side half of idempotency.
static void MockLedgerWrite(MockServer* s, uint64_t key, const uint8_t* payload, uint32_t len) {
    if (s->full || s->count >= MOCK_MAX_ENTRIES) {
        return;
    }
    for (uint32_t i = 0; i < s->count; ++i) {
        if (s->entries[i].key == key) {
            return; // dedup — the retransmit never fans out
        }
    }
    MockEntry* e = &s->entries[s->count++];
    e->key = key;
    e->payloadLen = len;
    memcpy(e->payload, payload, len);
}

// Stream every entry the client has not yet been sent. This is
// multiClientTransferLedger: it is called both at join (catch-up from base)
// and after every write (live tail), and it is the same code path for both.
static void MockTransferLedger(MockServer* s, MockClient* c) {
    if (!c->active || !c->ready) {
        return;
    }
    while (c->base < s->count) {
        const MockEntry* e = &s->entries[c->base];
        uint8_t frame[1 + RSBS_RELAY_ENTRY_HEADER_LEN + RSBS_RELAY_MAX_PAYLOAD];
        const uint32_t n = RelayProto_EncodeTransfer(e->key, e->payload, e->payloadLen, frame);
        MockPipeWrite(&c->toClient, frame, n);
        c->base++;
    }
}

// The broadcast in multiClientCmdTransfer does NOT skip the sender (unlike the
// chat path, which does). Reproducing that faithfully is the whole reason
// self-echo filtering is testable here at all.
static void MockBroadcast(MockServer* s) {
    for (int i = 0; i < MOCK_MAX_CLIENTS; ++i) {
        MockTransferLedger(s, &s->clients[i]);
    }
}

// --- Server-side client stream parsing --------------------------------------

static void MockServerConsume(MockClient* c, uint32_t n) {
    if (n >= c->rxLen) {
        c->rxLen = 0;
        return;
    }
    memmove(c->rx, c->rx + n, c->rxLen - n);
    c->rxLen -= n;
}

static void MockServerPump(MockServer* s, int clientIdx) {
    MockClient* c = &s->clients[clientIdx];
    if (!c->active) {
        return;
    }

    // Drain the client's outbound pipe into the server's parse buffer.
    c->rxLen += MockPipeRead(&c->toServer, c->rx + c->rxLen, (uint32_t)sizeof(c->rx) - c->rxLen);

    for (;;) {
        if (!c->sawHello) {
            if (c->rxLen < RSBS_RELAY_HELLO_LEN) {
                return;
            }
            // Validate the magic the way the real server does.
            if (memcmp(c->rx, RSBS_RELAY_MAGIC, RSBS_RELAY_MAGIC_LEN) != 0) {
                c->active = false;
                return;
            }
            MockServerConsume(c, RSBS_RELAY_HELLO_LEN);
            c->sawHello = true;

            // Server hello: magic + version + assigned client id.
            uint8_t hello[RSBS_RELAY_SVHELLO_LEN];
            memcpy(hello, RSBS_RELAY_MAGIC, RSBS_RELAY_MAGIC_LEN);
            const uint32_t v = RSBS_RELAY_PROTOCOL_VERSION;
            hello[5] = (uint8_t)(v & 0xFFu);
            hello[6] = (uint8_t)((v >> 8) & 0xFFu);
            hello[7] = (uint8_t)((v >> 16) & 0xFFu);
            hello[8] = (uint8_t)((v >> 24) & 0xFFu);
            hello[9] = (uint8_t)(clientIdx & 0xFFu);
            hello[10] = 0u;
            MockPipeWrite(&c->toClient, hello, RSBS_RELAY_SVHELLO_LEN);
            continue;
        }

        if (!c->sawJoin) {
            if (c->rxLen < RSBS_RELAY_JOIN_LEN) {
                return;
            }
            uint32_t base = (uint32_t)c->rx[16] | ((uint32_t)c->rx[17] << 8) | ((uint32_t)c->rx[18] << 16) |
                            ((uint32_t)c->rx[19] << 24);
            MockServerConsume(c, RSBS_RELAY_JOIN_LEN);
            c->sawJoin = true;
            c->ready = true;
            c->base = base;
            // Catch-up: everything from base onward, before any live traffic.
            MockTransferLedger(s, c);
            continue;
        }

        // Command stream from the client.
        if (c->rxLen < 1) {
            return;
        }
        if (c->rx[0] == RSBS_RELAY_OP_NONE) {
            MockServerConsume(c, 1);
            continue;
        }
        if (c->rx[0] == RSBS_RELAY_OP_TRANSFER) {
            if (c->rxLen < 1u + RSBS_RELAY_ENTRY_HEADER_LEN) {
                return;
            }
            const uint32_t payloadLen = c->rx[9];
            const uint32_t total = 1u + RSBS_RELAY_ENTRY_HEADER_LEN + payloadLen;
            if (c->rxLen < total) {
                return;
            }
            uint64_t key = 0;
            for (int i = 7; i >= 0; --i) {
                key = (key << 8) | (uint64_t)c->rx[1 + i];
            }
            MockLedgerWrite(s, key, c->rx + 10, payloadLen);
            MockServerConsume(c, total);
            MockBroadcast(s);
            continue;
        }
        // Unknown opcode: the real server disconnects.
        c->active = false;
        return;
    }
}

static void MockServerPumpAll(MockServer* s) {
    for (int i = 0; i < MOCK_MAX_CLIENTS; ++i) {
        MockServerPump(s, i);
    }
}

// --- RelayChannel bindings --------------------------------------------------

static int MockChanSend(void* userData, const uint8_t* data, uint32_t len) {
    MockClient* c = (MockClient*)userData;
    if (!c->active) {
        return -1;
    }
    MockPipeWrite(&c->toServer, data, len);
    return (int)len;
}

static int MockChanRecv(void* userData, uint8_t* data, uint32_t len) {
    MockClient* c = (MockClient*)userData;
    if (!c->active) {
        return -1;
    }
    return (int)MockPipeRead(&c->toClient, data, len);
}

static void MockReset(MockServer* s) {
    memset(s, 0, sizeof(*s));
}

static MockClient* MockAttach(MockServer* s, int idx) {
    MockClient* c = &s->clients[idx];
    memset(c, 0, sizeof(*c));
    c->active = true;
    return c;
}

static RelayChannel MockChannelFor(MockClient* c) {
    RelayChannel ch;
    ch.send = MockChanSend;
    ch.recv = MockChanRecv;
    ch.userData = c;
    return ch;
}

// Run the whole loopback to quiescence: client ticks and server pumps
// interleaved, so multi-round-trip flows (hello -> join -> catch-up) settle.
static int PumpAll(MockServer* s, RelayClient** clients, int n, int rounds) {
    int accepted = 0;
    for (int r = 0; r < rounds; ++r) {
        MockServerPumpAll(s);
        for (int i = 0; i < n; ++i) {
            accepted += Relay_Tick(clients[i]);
        }
    }
    return accepted;
}

// ============================================================================
// Shared fixtures
// ============================================================================

static const uint8_t kRoomA[RSBS_RELAY_UUID_LEN] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                                     0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01 };
static const uint8_t kRoomB[RSBS_RELAY_UUID_LEN] = { 0xFE, 0xED, 0xFA, 0xCE, 0x00, 0x11, 0x22, 0x33,
                                                     0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB };

typedef struct {
    int count;
    uint16_t ids[RSBS_SHARED_ITEM_CAP];
    uint8_t origins[RSBS_SHARED_ITEM_CAP];
} RelayAwardLog;

static void RelayAward(const SharedItem* item, void* ctx) {
    RelayAwardLog* log = (RelayAwardLog*)ctx;
    if (log->count < RSBS_SHARED_ITEM_CAP) {
        log->ids[log->count] = item->id;
        log->origins[log->count] = (uint8_t)item->originGame;
        log->count++;
    }
}

static int RelayOccupied(void) {
    int n = 0;
    for (uint32_t i = 0; i < RSBS_SHARED_ITEM_CAP; ++i) {
        if (gComboCtx.sharedItemsTagged[i].originGame != GAME_NONE) {
            n++;
        }
    }
    return n;
}

// ============================================================================
// Test 1: the wire format itself
// ============================================================================

TestResult Test_RelayWireFormat(void) {
    printf("[TEST] Relay wire format (golden vectors, ADR 0007 §2)\n");

    // --- Client hello: "OOMM2" + LE u32 0x00000200 ---
    uint8_t hello[RSBS_RELAY_HELLO_LEN];
    RelayProto_EncodeHello(hello);
    const uint8_t kHelloGold[RSBS_RELAY_HELLO_LEN] = { 'O', 'O', 'M', 'M', '2', 0x00, 0x02, 0x00, 0x00 };
    NR_ASSERT(memcmp(hello, kHelloGold, sizeof(kHelloGold)) == 0);

    // --- Join: uuid + LE u32 base. Always base 0 (ADR 0007 §3.3). ---
    uint8_t join[RSBS_RELAY_JOIN_LEN];
    RelayProto_EncodeJoin(kRoomA, 0u, join);
    NR_ASSERT(memcmp(join, kRoomA, RSBS_RELAY_UUID_LEN) == 0);
    NR_ASSERT(join[16] == 0 && join[17] == 0 && join[18] == 0 && join[19] == 0);

    // --- Grant payload: exactly 16 bytes, magic-prefixed, little-endian. ---
    RsbsGrantPayload g;
    g.version = RSBS_GRANT_PAYLOAD_VERSION;
    g.senderSlot = 2;
    g.targetSlot = 1;
    g.originGame = (uint8_t)GAME_OOT;
    g.itemId = 0x1234;
    g.senderSeq = 0x0007;
    g.settingsHash = 0xDEADBEEFu;

    uint8_t payload[RSBS_GRANT_PAYLOAD_LEN];
    NR_ASSERT(RelayProto_EncodeGrant(&g, payload));
    const uint8_t kGrantGold[RSBS_GRANT_PAYLOAD_LEN] = {
        'R', 'S', 'B', 'S',      // magic
        0x01,                    // version
        0x02,                    // senderSlot
        0x01,                    // targetSlot
        0x01,                    // originGame == GAME_OOT
        0x34, 0x12,              // itemId LE
        0x07, 0x00,              // senderSeq LE
        0xEF, 0xBE, 0xAD, 0xDE,  // settingsHash LE
    };
    NR_ASSERT(memcmp(payload, kGrantGold, sizeof(kGrantGold)) == 0);

    // Round trip.
    RsbsGrantPayload back;
    NR_ASSERT(RelayProto_DecodeGrant(payload, RSBS_GRANT_PAYLOAD_LEN, &back));
    NR_ASSERT(back.senderSlot == 2 && back.targetSlot == 1);
    NR_ASSERT(back.originGame == (uint8_t)GAME_OOT && back.itemId == 0x1234);
    NR_ASSERT(back.senderSeq == 7 && back.settingsHash == 0xDEADBEEFu);

    // Ledger key: (slot << 48) | (seq << 16) | itemId.
    NR_ASSERT(RelayProto_GrantKey(&g) == (((uint64_t)2 << 48) | ((uint64_t)7 << 16) | 0x1234u));

    // Two DIFFERENT senders of the same item must produce DIFFERENT keys, or
    // the server's own dedup would collapse two legitimate gifts — the ADR
    // 0005 §1 bug, one layer lower.
    RsbsGrantPayload g2 = g;
    g2.senderSlot = 3;
    NR_ASSERT(RelayProto_GrantKey(&g2) != RelayProto_GrantKey(&g));

    // --- Malformed / foreign payloads are rejected, never accepted. ---
    uint8_t bad[RSBS_GRANT_PAYLOAD_LEN];
    memcpy(bad, payload, sizeof(bad));
    bad[0] = 'X'; // foreign magic (e.g. an OoTMM client in the same room)
    NR_ASSERT(!RelayProto_DecodeGrant(bad, RSBS_GRANT_PAYLOAD_LEN, &back));
    memcpy(bad, payload, sizeof(bad));
    bad[4] = 0x7F; // unknown version
    NR_ASSERT(!RelayProto_DecodeGrant(bad, RSBS_GRANT_PAYLOAD_LEN, &back));
    memcpy(bad, payload, sizeof(bad));
    bad[7] = 0x00; // GAME_NONE
    NR_ASSERT(!RelayProto_DecodeGrant(bad, RSBS_GRANT_PAYLOAD_LEN, &back));
    NR_ASSERT(!RelayProto_DecodeGrant(payload, RSBS_GRANT_PAYLOAD_LEN - 1u, &back)); // short

    // Encoder refuses malformed grants too, so a local bug never looks like a
    // peer's bad data.
    RsbsGrantPayload zero = g;
    zero.targetSlot = 0;
    NR_ASSERT(!RelayProto_EncodeGrant(&zero, payload));

    // --- Incremental frame decode, including the keepalive NOP. ---
    uint8_t frame[64];
    uint32_t consumed = 0;
    RelayLedgerEntry entry;

    // The server emits OP_NONE keepalives; treating one as a bad opcode would
    // drop a healthy connection after a few idle seconds.
    frame[0] = RSBS_RELAY_OP_NONE;
    NR_ASSERT(RelayProto_DecodeFrame(frame, 1, &consumed, &entry) == RSBS_RELAY_DECODE_NOP);
    NR_ASSERT(consumed == 1);

    const uint32_t n = RelayProto_EncodeTransfer(0xAABBCCDDu, payload, RSBS_GRANT_PAYLOAD_LEN, frame);
    NR_ASSERT(n == 10u + RSBS_GRANT_PAYLOAD_LEN);
    // A partial frame must ask for more rather than misparse.
    NR_ASSERT(RelayProto_DecodeFrame(frame, n - 1u, &consumed, &entry) == RSBS_RELAY_DECODE_NEED_MORE);
    NR_ASSERT(RelayProto_DecodeFrame(frame, n, &consumed, &entry) == RSBS_RELAY_DECODE_ENTRY);
    NR_ASSERT(consumed == n && entry.key == 0xAABBCCDDu && entry.payloadLen == RSBS_GRANT_PAYLOAD_LEN);
    NR_ASSERT(memcmp(entry.payload, payload, RSBS_GRANT_PAYLOAD_LEN) == 0);

    // Room binding: a different room is a different ADR 0005 source, so a
    // cursor can never be applied against a room that did not produce it.
    NR_ASSERT(Relay_SourceKeyForRoom(kRoomA) != Relay_SourceKeyForRoom(kRoomB));
    NR_ASSERT(Relay_SourceKeyForRoom(kRoomA) != 0u);

    printf("[TEST] PASS: wire format\n");
    return TEST_PASS;
}

// ============================================================================
// Test 2: loopback delivery, self-echo, retransmit, distinct gifts, ordering
// ============================================================================

TestResult Test_RelayLoopback(void) {
    printf("[TEST] Relay loopback delivery (ADR 0007 §8)\n");

    ComboContext_Init();
    MockReset(&gMockServer);

    // Slot 1 = us, slot 2 and 3 = peers.
    MockClient* mcA = MockAttach(&gMockServer, 0);
    MockClient* mcB = MockAttach(&gMockServer, 1);
    MockClient* mcC = MockAttach(&gMockServer, 2);

    RelayClient a, b, cc;
    Relay_Init(&a, kRoomA, 1, 0xABCDu); // receiver
    Relay_Init(&b, kRoomA, 2, 0xABCDu); // peer
    Relay_Init(&cc, kRoomA, 3, 0xABCDu); // second peer

    RelayChannel chA = MockChannelFor(mcA);
    RelayChannel chB = MockChannelFor(mcB);
    RelayChannel chC = MockChannelFor(mcC);
    NR_ASSERT(Relay_Connect(&a, &chA));
    NR_ASSERT(Relay_Connect(&b, &chB));
    NR_ASSERT(Relay_Connect(&cc, &chC));

    RelayClient* all[3] = { &a, &b, &cc };
    PumpAll(&gMockServer, all, 3, 4);
    NR_ASSERT(a.state == RSBS_RELAY_STATE_READY);
    NR_ASSERT(b.state == RSBS_RELAY_STATE_READY);

    // --- Peer B gifts us two items, in order. ---
    NR_ASSERT(Relay_SendGrant(&b, 1, GAME_OOT, 100));
    NR_ASSERT(Relay_SendGrant(&b, 1, GAME_OOT, 200));
    PumpAll(&gMockServer, all, 3, 4);

    NR_ASSERT(a.statAccepted == 2);
    NR_ASSERT(Combo_GetGrantCursor(a.sourceKey) == 2);
    NR_ASSERT(RelayOccupied() == 2);

    // --- SELF-ECHO: B receives its own two entries back (the real server does
    // not skip the sender) and must award itself NOTHING. ---
    NR_ASSERT(b.statEntriesSeen == 2);
    NR_ASSERT(b.statNotForUs == 2);
    NR_ASSERT(b.statAccepted == 0);
    NR_ASSERT(Combo_GetGrantCursor(b.sourceKey) == 2); // same room == same source key
    // C is in the room but addressed by nothing.
    NR_ASSERT(cc.statNotForUs == 2 && cc.statAccepted == 0);

    // --- RETRANSMIT yields exactly one item. Re-offering the identical
    // (senderSlot, senderSeq) produces the same ledger key, which the server
    // dedups before it ever fans out. ---
    RsbsGrantPayload dupGrant;
    dupGrant.version = RSBS_GRANT_PAYLOAD_VERSION;
    dupGrant.senderSlot = 2;
    dupGrant.targetSlot = 1;
    dupGrant.originGame = (uint8_t)GAME_OOT;
    dupGrant.itemId = 100;
    dupGrant.senderSeq = 1; // the seq B already used for item 100
    dupGrant.settingsHash = 0xABCDu;
    uint8_t dupPayload[RSBS_GRANT_PAYLOAD_LEN];
    NR_ASSERT(RelayProto_EncodeGrant(&dupGrant, dupPayload));
    uint8_t dupFrame[64];
    const uint32_t dn = RelayProto_EncodeTransfer(RelayProto_GrantKey(&dupGrant), dupPayload, RSBS_GRANT_PAYLOAD_LEN, dupFrame);
    MockPipeWrite(&mcB->toServer, dupFrame, dn);
    PumpAll(&gMockServer, all, 3, 4);

    NR_ASSERT(gMockServer.count == 2);   // server-side dedup absorbed it
    NR_ASSERT(a.statAccepted == 2);      // still exactly two
    NR_ASSERT(RelayOccupied() == 2);

    // --- TWO PEERS, SAME ITEM = TWO ITEMS (the ADR 0005 §1 regression). C
    // gifts item 100 as well; distinct sender => distinct key => it survives
    // the server's dedup, and the client-side content de-dup never sees it
    // because sourced entries are flagged. ---
    NR_ASSERT(Relay_SendGrant(&cc, 1, GAME_OOT, 100));
    PumpAll(&gMockServer, all, 3, 4);
    NR_ASSERT(gMockServer.count == 3);
    NR_ASSERT(a.statAccepted == 3);
    NR_ASSERT(RelayOccupied() == 3);

    // --- RECEIVED-ORDER redemption. The give paths resolve progressives
    // against the live save, so order changes WHAT the player gets. ---
    RelayAwardLog log;
    memset(&log, 0, sizeof(log));
    const int redeemed = Combo_RedeemSharedItemsForGame(GAME_OOT, RelayAward, &log);
    NR_ASSERT(redeemed == 3 && log.count == 3);
    NR_ASSERT(log.ids[0] == 100 && log.ids[1] == 200 && log.ids[2] == 100);

    // Single-use: a second safe point awards nothing more.
    RelayAwardLog again;
    memset(&again, 0, sizeof(again));
    NR_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, RelayAward, &again) == 0);
    NR_ASSERT(again.count == 0);

    printf("[TEST] PASS: loopback delivery\n");
    return TEST_PASS;
}

// ============================================================================
// Test 3: late-join catch-up, cross-game switch, foreign entries
// ============================================================================

TestResult Test_RelayCatchup(void) {
    printf("[TEST] Relay late-join catch-up + cross-game survival (ADR 0007 §3.3, §5.1)\n");

    ComboContext_Init();
    MockReset(&gMockServer);

    MockClient* mcB = MockAttach(&gMockServer, 1);
    RelayClient b;
    Relay_Init(&b, kRoomA, 2, 0);
    RelayChannel chB = MockChannelFor(mcB);
    NR_ASSERT(Relay_Connect(&b, &chB));
    RelayClient* justB[1] = { &b };
    PumpAll(&gMockServer, justB, 1, 3);

    // Peer B sends three grants for slot 1 BEFORE slot 1 ever joins — one of
    // them for the OTHER game, so we also prove a grant crosses the switch.
    NR_ASSERT(Relay_SendGrant(&b, 1, GAME_OOT, 11));
    NR_ASSERT(Relay_SendGrant(&b, 1, GAME_MM, 22));
    NR_ASSERT(Relay_SendGrant(&b, 1, GAME_OOT, 33));
    PumpAll(&gMockServer, justB, 1, 4);
    NR_ASSERT(gMockServer.count == 3);

    // A foreign entry lands in the room too — an OoTMM client sharing the
    // ledger. It must be skipped without desyncing the stream (ADR 0007 §4.3).
    const uint8_t kForeign[8] = { 'O', 'o', 'T', 'M', 'M', 0, 0, 0 };
    uint8_t fFrame[64];
    const uint32_t fn = RelayProto_EncodeTransfer(0x1234567890ABCDEFull, kForeign, sizeof(kForeign), fFrame);
    MockPipeWrite(&mcB->toServer, fFrame, fn);
    PumpAll(&gMockServer, justB, 1, 3);
    NR_ASSERT(gMockServer.count == 4);

    // --- NOW slot 1 joins, at ledgerBase 0, and must receive the whole
    // backlog. This is the late-joiner case. ---
    MockClient* mcA = MockAttach(&gMockServer, 0);
    RelayClient a;
    Relay_Init(&a, kRoomA, 1, 0);
    RelayChannel chA = MockChannelFor(mcA);
    NR_ASSERT(Relay_Connect(&a, &chA));

    RelayClient* both[2] = { &a, &b };
    PumpAll(&gMockServer, both, 2, 6);

    NR_ASSERT(a.state == RSBS_RELAY_STATE_READY);
    NR_ASSERT(a.statEntriesSeen == 4);
    NR_ASSERT(a.statForeignSkipped == 1); // skipped, and it did not desync
    NR_ASSERT(a.statAccepted == 3);       // all three real grants caught up
    NR_ASSERT(Combo_GetGrantCursor(a.sourceKey) == 3);

    // --- Grants are split across the two games and BOTH survive: the OoT ones
    // redeem on an OoT safe point, the MM one waits, un-redeemed, for MM. The
    // relay needs no switch awareness at all — the array is process-global.
    RelayAwardLog ootLog;
    memset(&ootLog, 0, sizeof(ootLog));
    NR_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, RelayAward, &ootLog) == 2);
    NR_ASSERT(ootLog.ids[0] == 11 && ootLog.ids[1] == 33);

    // The MM grant is still pending — exactly what "crosses the switch" means.
    NR_ASSERT(Combo_CountSharedItems(GAME_MM, false) == 1);
    RelayAwardLog mmLog;
    memset(&mmLog, 0, sizeof(mmLog));
    NR_ASSERT(Combo_RedeemSharedItemsForGame(GAME_MM, RelayAward, &mmLog) == 1);
    NR_ASSERT(mmLog.count == 1 && mmLog.ids[0] == 22);
    NR_ASSERT(mmLog.origins[0] == (uint8_t)GAME_MM);

    // --- REJOIN: a reconnect replays from base 0 again. Every already-
    // delivered grant must come back DUPLICATE (cursor), not re-award. This is
    // the designed path of ADR 0005 §1, not an error case. ---
    MockClient* mcA2 = MockAttach(&gMockServer, 3);
    RelayClient a2;
    Relay_Init(&a2, kRoomA, 1, 0);
    RelayChannel chA2 = MockChannelFor(mcA2);
    NR_ASSERT(Relay_Connect(&a2, &chA2));
    RelayClient* rejoin[1] = { &a2 };
    PumpAll(&gMockServer, rejoin, 1, 6);

    NR_ASSERT(a2.statEntriesSeen == 4);
    NR_ASSERT(a2.statDuplicate == 3); // the whole prefix was already delivered
    NR_ASSERT(a2.statAccepted == 0);
    NR_ASSERT(Combo_GetGrantCursor(a2.sourceKey) == 3); // unmoved
    NR_ASSERT(RelayOccupied() == 3);                    // no re-award, no new entries

    printf("[TEST] PASS: catch-up + cross-game\n");
    return TEST_PASS;
}

// ============================================================================
// Test 4: backpressure and loud overflow
// ============================================================================

TestResult Test_RelayBackpressure(void) {
    printf("[TEST] Relay backpressure is loud and lossless (ADR 0005 §3, ADR 0007 §4.2)\n");

    ComboContext_Init();
    MockReset(&gMockServer);

    // Fill the durable array with UN-REDEEMED in-process entries so the seam
    // has nothing to reclaim and must refuse.
    for (uint32_t i = 0; i < RSBS_SHARED_ITEM_CAP; ++i) {
        NR_ASSERT(Combo_RecordSharedItem(GAME_OOT, (uint16_t)(1000 + i)) >= 0);
    }
    NR_ASSERT(RelayOccupied() == (int)RSBS_SHARED_ITEM_CAP);
    NR_ASSERT(Combo_GetSharedItemOverflowCount() == 0);

    MockClient* mcA = MockAttach(&gMockServer, 0);
    MockClient* mcB = MockAttach(&gMockServer, 1);
    RelayClient a, b;
    Relay_Init(&a, kRoomA, 1, 0);
    Relay_Init(&b, kRoomA, 2, 0);
    RelayChannel chA = MockChannelFor(mcA);
    RelayChannel chB = MockChannelFor(mcB);
    NR_ASSERT(Relay_Connect(&a, &chA));
    NR_ASSERT(Relay_Connect(&b, &chB));
    RelayClient* all[2] = { &a, &b };
    PumpAll(&gMockServer, all, 2, 4);

    // A peer gift now cannot be recorded: the array is full of undelivered
    // items, so the seam answers RETRY_FULL.
    NR_ASSERT(Relay_SendGrant(&b, 1, GAME_OOT, 4242));
    PumpAll(&gMockServer, all, 2, 4);

    NR_ASSERT(a.statRetryFull > 0);
    NR_ASSERT(a.statAccepted == 0);
    // Backpressure, NOT loss: the cursor did not advance, so the grant is
    // still owed and the client knows it needs a replay.
    NR_ASSERT(Combo_GetGrantCursor(a.sourceKey) == 0);
    NR_ASSERT(Relay_ReplayNeeded(&a));
    // LOUD: the durable, .redsave-serialized counter moved.
    NR_ASSERT(Combo_GetSharedItemOverflowCount() > 0);
    // The grant is still queued client-side — it was not dropped on the floor.
    NR_ASSERT(a.inboxCount == 1);

    // --- Drain the array. The SAME grant, re-offered at the SAME seq, is now
    // ACCEPTED rather than deduped. That is the property that makes
    // RETRY_FULL backpressure instead of loss. ---
    RelayAwardLog drain;
    memset(&drain, 0, sizeof(drain));
    NR_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, RelayAward, &drain) == (int)RSBS_SHARED_ITEM_CAP);

    const int accepted = Relay_Tick(&a);
    NR_ASSERT(accepted == 1);
    NR_ASSERT(a.statAccepted == 1);
    NR_ASSERT(Combo_GetGrantCursor(a.sourceKey) == 1);
    NR_ASSERT(a.inboxCount == 0);

    // --- Session invalidation retires the client's RAM alongside gComboCtx,
    // so a dead session's undelivered grants cannot drain into the next one
    // (#440 guidance on #460). ---
    Relay_ClearSessionState(&a);
    NR_ASSERT(a.inboxCount == 0 && a.outboxCount == 0);
    // The counter restarts at 1 because the cursor died with gComboCtx —
    // submitting seq N+1 against a fresh cursor of 0 would wedge on GAP.
    NR_ASSERT(a.nextSeq == 1u);

    printf("[TEST] PASS: backpressure\n");
    return TEST_PASS;
}

// ============================================================================
// Test 5: the suspend latch
// ============================================================================

TestResult Test_RelaySuspendLatch(void) {
    printf("[TEST] Relay suspend latch stops applying, not polling (ADR 0006 §5)\n");

    ComboContext_Init();
    MockReset(&gMockServer);

    MockClient* mcA = MockAttach(&gMockServer, 0);
    MockClient* mcB = MockAttach(&gMockServer, 1);
    RelayClient a, b;
    Relay_Init(&a, kRoomA, 1, 0);
    Relay_Init(&b, kRoomA, 2, 0);
    RelayChannel chA = MockChannelFor(mcA);
    RelayChannel chB = MockChannelFor(mcB);
    NR_ASSERT(Relay_Connect(&a, &chA));
    NR_ASSERT(Relay_Connect(&b, &chB));
    RelayClient* all[2] = { &a, &b };
    PumpAll(&gMockServer, all, 2, 4);

    // Suspend the receiver, then have the peer send. Polling and decoding must
    // continue — otherwise a suspended game stalls the peer and loses its
    // ledger position — but nothing may be APPLIED.
    Relay_OnSuspend(&a);
    NR_ASSERT(Relay_SendGrant(&b, 1, GAME_OOT, 77));
    NR_ASSERT(Relay_SendGrant(&b, 1, GAME_OOT, 88));
    PumpAll(&gMockServer, all, 2, 4);

    NR_ASSERT(a.statEntriesSeen == 2); // received and decoded
    NR_ASSERT(a.inboxCount == 2);      // filed
    NR_ASSERT(a.statAccepted == 0);    // but NOT applied
    NR_ASSERT(RelayOccupied() == 0);
    NR_ASSERT(Combo_GetGrantCursor(a.sourceKey) == 0);

    // Resume drains in received order — the lifecycle has no implicit inverse,
    // so this release is a separate, named action.
    Relay_OnResume(&a);
    const int accepted = Relay_Tick(&a);
    NR_ASSERT(accepted == 2);
    NR_ASSERT(a.inboxCount == 0);
    NR_ASSERT(Combo_GetGrantCursor(a.sourceKey) == 2);

    RelayAwardLog log;
    memset(&log, 0, sizeof(log));
    NR_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, RelayAward, &log) == 2);
    NR_ASSERT(log.ids[0] == 77 && log.ids[1] == 88); // order preserved

    printf("[TEST] PASS: suspend latch\n");
    return TEST_PASS;
}
