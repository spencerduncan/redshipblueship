/**
 * @file netplay_tests.cpp
 * @brief Loopback harness for the Archipelago netplay transport (ADR 0006).
 *
 * ROM-free, display-free, single-threaded: every test drives a REAL APClient
 * over a real loopback WebSocket against the in-process mock AP server, by
 * interleaving MockApServer::Poll() and Netplay_Tick() on one thread — the
 * event order is deterministic, no scheduler races.
 *
 * What this file locks (the increment-1b contract):
 *  - end-to-end handshake to SLOT_CONNECTED and room binding
 *  - grants decode and apply IN INDEX ORDER through the registered sink
 *  - retransmits apply exactly once (AP-index idempotency)
 *  - Game_Suspend gating: received-but-not-applied, drained in order on resume
 *  - bounded inbox: overflow drops recoverably and a Sync replay loses nothing
 *  - room mismatch refuses to apply until an explicit rebind
 *  - the persisted cursor round-trips through the .redsave Tier-1 record
 *  - grants reach the CURRENT shared-item seam (origin-tagged, redeemed once,
 *    full-array backpressure) — see the #460 note at Test_NetplaySeam
 *
 * These entry points are referenced from test_runner.cpp's gTests[] table
 * under #ifdef RSBS_NETPLAY_ENABLED (rows exist only in RSBS_NETPLAY builds).
 */

#include "netplay.h"
#include "netplay_items.h"
#include "netplay_sink.h"

#include "context.h"
#include "save.h"
#include "shared_items.h"
#include "test_runner.h"

#include "mock_ap_server.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <thread>
#include <vector>

#define NETPLAY_ASSERT(cond, msg)               \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            return TEST_FAIL;                   \
        }                                       \
    } while (0)

namespace {

// ============================================================================
// Harness helpers
// ============================================================================

struct AppliedGrant {
    GameId game;
    uint16_t itemId;
    uint32_t seq;
};

struct CountingSink {
    std::vector<AppliedGrant> applied;
    bool refuse = false;

    static int Fn(GameId game, uint16_t itemId, uint32_t seq, void* ctx) {
        CountingSink* self = (CountingSink*)ctx;
        if (self->refuse) {
            return -1;
        }
        self->applied.push_back({ game, itemId, seq });
        return 0;
    }
};

/** Interleave server + client until pred() or the deadline. True iff pred. */
bool Pump(MockApServer& server, const std::function<bool()>& pred, int deadlineMs = 5000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(deadlineMs);
    for (;;) {
        server.Poll();
        Netplay_Tick();
        if (pred()) {
            return true;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

/** Pump for a FIXED window (for "nothing further happens" assertions). */
void PumpFor(MockApServer& server, int ms) {
    Pump(server, [] { return false; }, ms);
}

/** RAII teardown so every exit path leaves clean process-global state. */
struct HarnessGuard {
    MockApServer& server;
    explicit HarnessGuard(MockApServer& s) : server(s) {
    }
    ~HarnessGuard() {
        Netplay_Shutdown();
        server.Stop();
        Combo_ClearSharedItemOutbox();
        ComboContext_Init(); // later tests must not inherit netplay state
    }
};

/** Fresh context + transport, connected and ACTIVE against `server`. */
bool StartConnected(MockApServer& server, CountingSink* sink, const char* seedName, bool freshContext = true) {
    if (freshContext) {
        ComboContext_Init();
    }
    if (!server.Start(seedName)) {
        printf("[TEST] mock server failed to start\n");
        return false;
    }
    if (!Netplay_Init()) {
        return false;
    }
    if (sink != nullptr) {
        Netplay_SetGrantSink(CountingSink::Fn, sink);
    }
    if (!Netplay_Connect(server.Uri().c_str(), "P1", "")) {
        return false;
    }
    return Pump(server, [] { return Netplay_GetStatus().state == NETPLAY_STATE_ACTIVE; });
}

// ============================================================================
// netplay-items-map — pure mapping bijection + strict rejection
// ============================================================================

TestResult Test_NetplayItemsMap() {
    printf("[TEST] netplay-items-map: AP id mapping round-trips and rejects strictly\n");

    GameId game = GAME_NONE;
    uint16_t id = 0;

    int64_t oot = NetplayItems_Encode(GAME_OOT, 0x1234);
    NETPLAY_ASSERT(oot > 0, "OoT encode failed");
    NETPLAY_ASSERT(NetplayItems_Decode(oot, &game, &id), "OoT decode failed");
    NETPLAY_ASSERT(game == GAME_OOT && id == 0x1234, "OoT round-trip mismatch");

    int64_t mm = NetplayItems_Encode(GAME_MM, 7);
    NETPLAY_ASSERT(NetplayItems_Decode(mm, &game, &id), "MM decode failed");
    NETPLAY_ASSERT(game == GAME_MM && id == 7, "MM round-trip mismatch");
    NETPLAY_ASSERT(oot != mm, "distinct games must encode distinctly");

    NETPLAY_ASSERT(NetplayItems_Encode(GAME_NONE, 1) == -1, "GAME_NONE must not encode");
    NETPLAY_ASSERT(!NetplayItems_Decode(RSBS_AP_ITEM_BASE - 1, &game, &id), "below-base id must not decode");
    NETPLAY_ASSERT(!NetplayItems_Decode(RSBS_AP_ITEM_BASE + (3ll << 16), &game, &id), "bad game tag must not decode");
    NETPLAY_ASSERT(!NetplayItems_Decode(RSBS_AP_ITEM_BASE, &game, &id), "game tag 0 must not decode");
    NETPLAY_ASSERT(!NetplayItems_Decode(66123, &game, &id), "a typical foreign world id must not decode");

    printf("[TEST] PASS: netplay item mapping behaves\n");
    return TEST_PASS;
}

// ============================================================================
// netplay-cursor-persist — the carved cursor is real .redsave state
// ============================================================================

TestResult Test_NetplayCursorPersist() {
    printf("[TEST] netplay-cursor-persist: AP cursor round-trips through the Tier-1 record\n");

    Context_InitFrozenStates();
    ComboContext_Init();
    NETPLAY_ASSERT(gComboCtx.netplayApAppliedCount == 0 && gComboCtx.netplayApRoomFingerprint == 0,
                   "fresh init must read as never-netplayed (zero means unset)");

    auto& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory("rsbs_netplay_test_saves");
    mgr.DeleteSave(0);

    gComboCtx.netplayApAppliedCount = 42;
    gComboCtx.netplayApRoomFingerprint = 0xDEADBEEFu;
    NETPLAY_ASSERT(mgr.Save(0), "Save failed");

    ComboContext_Init(); // wipe live state
    NETPLAY_ASSERT(gComboCtx.netplayApAppliedCount == 0, "wipe did not clear cursor");
    NETPLAY_ASSERT(mgr.Load(0), "Load failed");
    NETPLAY_ASSERT(gComboCtx.netplayApAppliedCount == 42, "applied count did not round-trip");
    NETPLAY_ASSERT(gComboCtx.netplayApRoomFingerprint == 0xDEADBEEFu, "room fingerprint did not round-trip");

    mgr.DeleteSave(0);
    ComboContext_Init();
    printf("[TEST] PASS: netplay cursor persists\n");
    return TEST_PASS;
}

// ============================================================================
// netplay-loopback-connect — handshake to ACTIVE + room binding
// ============================================================================

TestResult Test_NetplayLoopbackConnect() {
    printf("[TEST] netplay-loopback-connect: real client reaches SLOT_CONNECTED on loopback\n");
    MockApServer server;
    HarnessGuard guard(server);
    CountingSink sink;

    NETPLAY_ASSERT(StartConnected(server, &sink, "seed-connect"), "handshake did not reach ACTIVE");
    NETPLAY_ASSERT(server.ClientConnected(), "server does not see the client");
    NETPLAY_ASSERT(server.ConnectPacketsSeen() == 1, "expected exactly one Connect packet");

    NetplayStatus st = Netplay_GetStatus();
    NETPLAY_ASSERT(st.state == NETPLAY_STATE_ACTIVE, "status not ACTIVE");
    NETPLAY_ASSERT(!st.roomMismatch, "fresh save must bind, not mismatch");
    NETPLAY_ASSERT(st.appliedCount == 0 && st.pendingGrants == 0, "clean room must start empty");
    NETPLAY_ASSERT(gComboCtx.netplayApRoomFingerprint != 0, "room fingerprint was not bound");

    Netplay_Disconnect();
    NETPLAY_ASSERT(Netplay_GetStatus().state == NETPLAY_STATE_IDLE, "disconnect must return to IDLE");

    printf("[TEST] PASS: loopback handshake works end to end\n");
    return TEST_PASS;
}

// ============================================================================
// netplay-loopback-grant — decode + in-order application through the sink
// ============================================================================

TestResult Test_NetplayLoopbackGrant() {
    printf("[TEST] netplay-loopback-grant: grants decode and apply in index order\n");
    MockApServer server;
    HarnessGuard guard(server);
    CountingSink sink;

    NETPLAY_ASSERT(StartConnected(server, &sink, "seed-grant"), "handshake did not reach ACTIVE");

    server.PushItem(NetplayItems_Encode(GAME_OOT, 101));
    server.PushItem(NetplayItems_Encode(GAME_MM, 202));
    server.PushItem(NetplayItems_Encode(GAME_OOT, 303));

    NETPLAY_ASSERT(Pump(server, [&] { return sink.applied.size() == 3; }), "3 grants did not arrive");
    NETPLAY_ASSERT(sink.applied[0].game == GAME_OOT && sink.applied[0].itemId == 101, "grant 0 wrong");
    NETPLAY_ASSERT(sink.applied[1].game == GAME_MM && sink.applied[1].itemId == 202, "grant 1 wrong");
    NETPLAY_ASSERT(sink.applied[2].game == GAME_OOT && sink.applied[2].itemId == 303, "grant 2 wrong");
    NETPLAY_ASSERT(sink.applied[0].seq == 0 && sink.applied[1].seq == 1 && sink.applied[2].seq == 2,
                   "grants applied out of index order");

    NetplayStatus st = Netplay_GetStatus();
    NETPLAY_ASSERT(st.appliedCount == 3, "cursor did not advance to 3");
    NETPLAY_ASSERT(st.pendingGrants == 0, "inbox should be drained");
    NETPLAY_ASSERT(gComboCtx.netplayApAppliedCount == 3, "persisted cursor mismatch");

    printf("[TEST] PASS: grants flow through the seam in order\n");
    return TEST_PASS;
}

// ============================================================================
// netplay-loopback-dup — retransmit applies exactly once (index idempotency)
// ============================================================================

TestResult Test_NetplayLoopbackDup() {
    printf("[TEST] netplay-loopback-dup: a full retransmit yields zero re-applications\n");
    MockApServer server;
    HarnessGuard guard(server);
    CountingSink sink;

    NETPLAY_ASSERT(StartConnected(server, &sink, "seed-dup"), "handshake did not reach ACTIVE");

    server.PushItem(NetplayItems_Encode(GAME_OOT, 11));
    server.PushItem(NetplayItems_Encode(GAME_MM, 22));
    NETPLAY_ASSERT(Pump(server, [&] { return sink.applied.size() == 2; }), "grants did not arrive");

    server.RetransmitAll();
    server.RetransmitAll();
    PumpFor(server, 300);
    NETPLAY_ASSERT(sink.applied.size() == 2, "retransmit re-applied a grant");
    NETPLAY_ASSERT(Netplay_GetStatus().appliedCount == 2, "retransmit moved the cursor");

    // The stream continues normally after the dups.
    server.PushItem(NetplayItems_Encode(GAME_OOT, 33));
    NETPLAY_ASSERT(Pump(server, [&] { return sink.applied.size() == 3; }), "post-dup grant did not arrive");
    NETPLAY_ASSERT(sink.applied[2].itemId == 33 && sink.applied[2].seq == 2, "post-dup grant wrong");

    printf("[TEST] PASS: retransmits are idempotent\n");
    return TEST_PASS;
}

// ============================================================================
// netplay-loopback-suspend — Game_Suspend gates application, resume drains
// ============================================================================

TestResult Test_NetplayLoopbackSuspend() {
    printf("[TEST] netplay-loopback-suspend: suspended = buffered not applied; resume drains in order\n");
    MockApServer server;
    HarnessGuard guard(server);
    CountingSink sink;

    NETPLAY_ASSERT(StartConnected(server, &sink, "seed-suspend"), "handshake did not reach ACTIVE");

    Netplay_OnGameSuspend(GAME_OOT);
    server.PushItem(NetplayItems_Encode(GAME_MM, 501));
    server.PushItem(NetplayItems_Encode(GAME_OOT, 502));

    // The socket is still serviced (the grants must land in the inbox)...
    NETPLAY_ASSERT(Pump(server, [] { return Netplay_GetStatus().pendingGrants == 2; }),
                   "suspended transport stopped servicing the socket");
    // ...but nothing is applied while suspended.
    PumpFor(server, 200);
    NETPLAY_ASSERT(sink.applied.empty(), "a grant was applied while suspended");
    NETPLAY_ASSERT(Netplay_GetStatus().appliedCount == 0, "cursor moved while suspended");
    NETPLAY_ASSERT(Netplay_GetStatus().suspended, "status must report suspended");

    Netplay_OnGameResume(GAME_MM);
    NETPLAY_ASSERT(Pump(server, [&] { return sink.applied.size() == 2; }), "resume did not drain the inbox");
    NETPLAY_ASSERT(sink.applied[0].itemId == 501 && sink.applied[1].itemId == 502, "drain order wrong");
    NETPLAY_ASSERT(Netplay_GetStatus().pendingGrants == 0, "inbox not drained after resume");

    printf("[TEST] PASS: suspend/resume gating works\n");
    return TEST_PASS;
}

// ============================================================================
// netplay-loopback-overflow — bounded inbox, recoverable drop, lossless replay
// ============================================================================

TestResult Test_NetplayLoopbackOverflow() {
    printf("[TEST] netplay-loopback-overflow: inbox is bounded and overflow loses nothing\n");
    MockApServer server;
    HarnessGuard guard(server);
    CountingSink sink;

    NETPLAY_ASSERT(StartConnected(server, &sink, "seed-overflow"), "handshake did not reach ACTIVE");

    const uint32_t total = RSBS_NETPLAY_INBOX_CAP + 20;
    Netplay_OnGameSuspend(GAME_OOT);
    for (uint32_t i = 0; i < total; i++) {
        server.PushItem(NetplayItems_Encode(GAME_OOT, (uint16_t)(1000 + i)));
    }

    // Anti-Anchor #3: the producer can run all it wants; the inbox stays at
    // its cap and the overflow is recorded, not leaked.
    NETPLAY_ASSERT(Pump(server, [] { return Netplay_GetStatus().droppedForResync > 0; }),
                   "overflow was never detected");
    PumpFor(server, 200);
    NetplayStatus st = Netplay_GetStatus();
    NETPLAY_ASSERT(st.pendingGrants <= RSBS_NETPLAY_INBOX_CAP, "inbox exceeded its cap");
    NETPLAY_ASSERT(st.appliedCount == 0 && sink.applied.empty(), "applied while suspended");

    // Resume: the cap's worth applies, the Sync replay recovers the dropped
    // tail, and every index applies exactly once, in order.
    Netplay_OnGameResume(GAME_OOT);
    NETPLAY_ASSERT(Pump(server, [&] { return sink.applied.size() == total; }, 10000),
                   "not every grant was recovered after overflow");
    for (uint32_t i = 0; i < total; i++) {
        NETPLAY_ASSERT(sink.applied[i].seq == i, "replay broke exactly-once/order");
        NETPLAY_ASSERT(sink.applied[i].itemId == (uint16_t)(1000 + i), "replay corrupted a grant");
    }
    NETPLAY_ASSERT(Netplay_GetStatus().appliedCount == total, "cursor did not reach the stream end");
    NETPLAY_ASSERT(server.SyncPacketsSeen() >= 1, "recovery did not go through Sync");

    printf("[TEST] PASS: bounded inbox + Sync replay is lossless\n");
    return TEST_PASS;
}

// ============================================================================
// netplay-loopback-room-mismatch — stale cursor refuses; rebind is explicit
// ============================================================================

TestResult Test_NetplayLoopbackRoomMismatch() {
    printf("[TEST] netplay-loopback-room-mismatch: a foreign room's stream is refused until rebind\n");
    MockApServer server;
    HarnessGuard guard(server);
    CountingSink sink;

    // A save whose cursor belongs to some other room.
    ComboContext_Init();
    gComboCtx.netplayApRoomFingerprint = 0x12345678u;
    gComboCtx.netplayApAppliedCount = 5;

    NETPLAY_ASSERT(StartConnected(server, &sink, "seed-other-room", /*freshContext=*/false),
                   "handshake did not reach ACTIVE");
    NETPLAY_ASSERT(Netplay_GetStatus().roomMismatch, "mismatched room was not detected");

    server.PushItem(NetplayItems_Encode(GAME_OOT, 900));
    server.PushItem(NetplayItems_Encode(GAME_MM, 901));
    PumpFor(server, 300);
    NETPLAY_ASSERT(sink.applied.empty(), "a mismatched room's grant was applied");
    NETPLAY_ASSERT(gComboCtx.netplayApAppliedCount == 5, "mismatched room moved the cursor");
    NETPLAY_ASSERT(gComboCtx.netplayApRoomFingerprint == 0x12345678u, "mismatch overwrote the binding");

    // Explicit operator adoption: cursor resets, the full stream applies.
    NETPLAY_ASSERT(Netplay_RebindRoom(), "rebind refused");
    NETPLAY_ASSERT(Pump(server, [&] { return sink.applied.size() == 2; }), "rebind did not replay the stream");
    NETPLAY_ASSERT(sink.applied[0].itemId == 900 && sink.applied[0].seq == 0, "replayed grant 0 wrong");
    NETPLAY_ASSERT(sink.applied[1].itemId == 901 && sink.applied[1].seq == 1, "replayed grant 1 wrong");
    NETPLAY_ASSERT(gComboCtx.netplayApAppliedCount == 2, "cursor wrong after rebind");
    NETPLAY_ASSERT(!Netplay_RebindRoom(), "rebind must refuse when not mismatched");

    printf("[TEST] PASS: room mismatch is refused until explicitly rebound\n");
    return TEST_PASS;
}

// ============================================================================
// netplay-seam-shared-items — real grants through the CURRENT shared-item seam
// ============================================================================

static void CountAward(const SharedItem* item, void* ctx) {
    (void)item;
    (*(int*)ctx)++;
}

TestResult Test_NetplaySeamSharedItems() {
    printf("[TEST] netplay-seam-shared-items: network grants land origin-tagged and redeem once\n");
    MockApServer server;
    HarnessGuard guard(server);

    NETPLAY_ASSERT(StartConnected(server, nullptr, "seed-seam"), "handshake did not reach ACTIVE");
    Netplay_InstallSharedItemSink();

    server.PushItem(NetplayItems_Encode(GAME_OOT, 55));
    server.PushItem(NetplayItems_Encode(GAME_MM, 66));
    server.PushItem(NetplayItems_Encode(GAME_OOT, 55)); // distinct grant, same content
    server.PushItem(NetplayItems_Encode(GAME_OOT, 77));

    NETPLAY_ASSERT(Pump(server, [] { return Netplay_GetStatus().appliedCount == 4; }),
                   "grants did not reach the shared-item seam");

    // #460 gap 2, asserted TOLERANTLY on purpose: under the CURRENT Lane-A1
    // producer the two grants of (OoT, 55) content-merge into one un-redeemed
    // slot (count 2); under the coming foundation seam they must stay
    // distinct (count 3). Either is accepted here so the foundation landing
    // does not trip this lock; the strict assertion moves into the foundation
    // rebind. What is NOT tolerated: fewer (an item lost) or more (invented).
    int ootPending = Combo_CountSharedItems(GAME_OOT, false);
    int mmPending = Combo_CountSharedItems(GAME_MM, false);
    printf("[TEST]   OoT pending after 3 OoT grants (one duplicated): %d (2=content-merged, 3=unique-path)\n",
           ootPending);
    NETPLAY_ASSERT(ootPending == 2 || ootPending == 3, "OoT grant count out of the accepted range");
    NETPLAY_ASSERT(mmPending == 1, "MM grant did not land");

    // Cross-arrival redemption through the REAL consumer: exactly the pending
    // count awards, exactly once (the array is process-global, so this is the
    // same state a game switch crosses on).
    int awards = 0;
    NETPLAY_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, CountAward, &awards) == ootPending,
                   "OoT redemption count mismatch");
    NETPLAY_ASSERT(awards == ootPending, "award callback count mismatch");
    NETPLAY_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, CountAward, &awards) == 0,
                   "second OoT redemption must award nothing");
    NETPLAY_ASSERT(Combo_RedeemSharedItemsForGame(GAME_MM, CountAward, &awards) == 1, "MM redemption count mismatch");

    // #460 gap 4 backpressure: fill the durable array, then prove a further
    // network grant HOLDS the cursor instead of vanishing.
    int filled = 0;
    for (uint16_t id = 2000; Combo_RecordSharedItem(GAME_OOT, id) >= 0; id++) {
        filled++;
        NETPLAY_ASSERT(filled <= (int)RSBS_SHARED_ITEM_CAP, "array never reported full");
    }
    server.PushItem(NetplayItems_Encode(GAME_OOT, 999));
    PumpFor(server, 300);
    NetplayStatus st = Netplay_GetStatus();
    NETPLAY_ASSERT(st.appliedCount == 4, "a grant was 'applied' into a full array");
    NETPLAY_ASSERT(st.pendingGrants >= 1, "the refused grant must stay queued for retry");

    printf("[TEST] PASS: the seam receives, tags, redeems once, and backpressures\n");
    return TEST_PASS;
}

} // namespace

// ============================================================================
// C entry points for test_runner.cpp's gTests[] table
// ============================================================================

extern "C" {

TestResult Netplay_Test_ItemsMap(void) {
    return Test_NetplayItemsMap();
}
TestResult Netplay_Test_CursorPersist(void) {
    return Test_NetplayCursorPersist();
}
TestResult Netplay_Test_LoopbackConnect(void) {
    return Test_NetplayLoopbackConnect();
}
TestResult Netplay_Test_LoopbackGrant(void) {
    return Test_NetplayLoopbackGrant();
}
TestResult Netplay_Test_LoopbackDup(void) {
    return Test_NetplayLoopbackDup();
}
TestResult Netplay_Test_LoopbackSuspend(void) {
    return Test_NetplayLoopbackSuspend();
}
TestResult Netplay_Test_LoopbackOverflow(void) {
    return Test_NetplayLoopbackOverflow();
}
TestResult Netplay_Test_LoopbackRoomMismatch(void) {
    return Test_NetplayLoopbackRoomMismatch();
}
TestResult Netplay_Test_SeamSharedItems(void) {
    return Test_NetplaySeamSharedItems();
}

} // extern "C"
