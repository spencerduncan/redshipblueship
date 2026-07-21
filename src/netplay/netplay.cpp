/**
 * @file netplay.cpp
 * @brief Archipelago netplay transport implementation (increment 1b).
 *
 * See netplay.h for the design contract (single-threaded poll-driven client,
 * bounded inbox, AP-owned ordering cursor, explicit suspend/resume gating)
 * and docs/adr/0006-netplay-archipelago-transport.md for the decision record.
 *
 * This TU is compiled only when RSBS_NETPLAY=ON. Build configuration
 * (CMake/Netplay.cmake) defines ASIO_STANDALONE, WSWRAP_NO_SSL,
 * WSWRAP_NO_COMPRESSION, AP_NO_SCHEMA and AP_NO_DEFAULT_DATA_PACKAGE_STORE,
 * so the dependency surface is exactly: apclientpp + wswrap + websocketpp +
 * asio + nlohmann/json — all header-only, no OpenSSL, no valijson.
 */

#include "netplay.h"
#include "netplay_items.h"
#include "netplay_sink.h"

#include "context.h" // gComboCtx (netplayApAppliedCount / netplayApRoomFingerprint)

#include <apclient.hpp>

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <thread>

namespace {

// ============================================================================
// In-RAM data package store.
//
// Increment 1 resolves item ids numerically (netplay_items.h), so data
// package CONTENT is irrelevant — but apclientpp requires a store (we build
// with AP_NO_DEFAULT_DATA_PACKAGE_STORE so the default store's filesystem
// cache under the user's home directory is never touched). Reporting every
// (game, checksum) as already cached also means the client never issues
// GetDataPackage, keeping the handshake minimal.
// ============================================================================

class RamDataPackageStore final : public APDataPackageStore {
  public:
    bool load(const std::string& game, const std::string& checksum, json& data) override {
        (void)game;
        data = json{
            { "item_name_to_id", json::object() },
            { "location_name_to_id", json::object() },
            { "checksum", checksum },
        };
        return true;
    }

    bool save(const std::string& game, const json& data) override {
        (void)game;
        (void)data;
        return true; // accept and forget
    }
};

// ============================================================================
// Core state (owning-thread only; there is no other thread)
// ============================================================================

struct PendingGrant {
    uint32_t apIndex;   // AP's monotonic item index
    uint8_t originGame; // decoded GameId, or GAME_NONE when !valid
    uint16_t itemId;    // decoded RG_*/RI_* id, meaningless when !valid
    bool valid;         // false: outside the RSBS mapping — applied as a no-op
};

struct NetplayCore {
    RamDataPackageStore store;
    std::unique_ptr<APClient> client;
    std::thread::id owner;

    std::string slotName;
    std::string password;

    std::deque<PendingGrant> inbox; // contiguous run starting at appliedCount
    bool suspended = false;
    bool roomMismatch = false;
    bool resyncScheduled = false;
    bool slotRefused = false;
    uint32_t droppedForResync = 0;
    uint32_t decodeFailures = 0;
    uint32_t connectedFingerprint = 0; // fingerprint of the live room (0 = none yet)
    std::chrono::steady_clock::time_point lastSyncRequest{};

    NetplayGrantSink sink = nullptr;
    void* sinkCtx = nullptr;
};

NetplayCore* sCore = nullptr;

bool OnOwnerThread(const char* api) {
    if (sCore == nullptr) {
        fprintf(stderr, "[Netplay] %s called before Netplay_Init\n", api);
        return false;
    }
    if (std::this_thread::get_id() != sCore->owner) {
        // Refusing (not asserting) keeps a misuse observable without UB; the
        // whole module is specified single-threaded, so this firing at all is
        // a caller bug.
        fprintf(stderr, "[Netplay] %s called off the owning thread — refused\n", api);
        return false;
    }
    return true;
}

uint32_t Fnv1a32(const std::string& s) {
    uint32_t h = 0x811C9DC5u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x01000193u;
    }
    return h;
}

uint32_t RoomFingerprint(const std::string& seedName, const std::string& slotName) {
    uint32_t fp = Fnv1a32(seedName + '\x1F' + slotName);
    return fp == 0 ? 1u : fp; // 0 is the serialized "unbound" sentinel
}

/** First AP index NOT yet accounted for (applied cursor + queued run). */
uint32_t ExpectedNextIndex() {
    return gComboCtx.netplayApAppliedCount + (uint32_t)sCore->inbox.size();
}

void ScheduleResync(const char* why) {
    if (!sCore->resyncScheduled) {
        fprintf(stderr, "[Netplay] resync scheduled (%s)\n", why);
    }
    sCore->resyncScheduled = true;
}

// ============================================================================
// APClient handlers (all fire inside Netplay_Tick, on the owning thread)
// ============================================================================

void OnRoomInfo() {
    // Join the slot. items_handling 0b111: receive remote items, our own
    // world's items, and starting inventory — the transport does not care
    // which class a grant is; the mapping layer filters by id.
    sCore->slotRefused = false;
    sCore->client->ConnectSlot(sCore->slotName, sCore->password, 7);
}

void OnSlotConnected(const nlohmann::json& slotData) {
    (void)slotData;
    const uint32_t fp = RoomFingerprint(sCore->client->get_seed(), sCore->slotName);
    sCore->connectedFingerprint = fp;

    if (gComboCtx.netplayApRoomFingerprint == 0) {
        // First contact for this save: bind, and make the cursor's zero-state
        // explicit (a stale count with an unbound fingerprint is impossible
        // to interpret, so it resets — fields are written as a pair).
        gComboCtx.netplayApRoomFingerprint = fp;
        gComboCtx.netplayApAppliedCount = 0;
        sCore->roomMismatch = false;
        fprintf(stderr, "[Netplay] bound to room (seed '%s', slot '%s')\n", sCore->client->get_seed().c_str(),
                sCore->slotName.c_str());
    } else if (gComboCtx.netplayApRoomFingerprint == fp) {
        sCore->roomMismatch = false;
        fprintf(stderr, "[Netplay] resumed room at applied count %u\n", gComboCtx.netplayApAppliedCount);
    } else {
        // The save's cursor belongs to a different room. Applying anything
        // would either skip items (stale high cursor) or double-apply (stale
        // low cursor); refuse until the operator rebinds explicitly.
        sCore->roomMismatch = true;
        fprintf(stderr,
                "[Netplay] ROOM MISMATCH: this save's netplay cursor belongs to a different "
                "room/seed. No items will be applied. Rebind explicitly to adopt the new room "
                "(resets the cursor).\n");
    }
}

void OnSlotRefused(const std::list<std::string>& errors) {
    sCore->slotRefused = true;
    std::string joined;
    for (const auto& e : errors) {
        if (!joined.empty()) {
            joined += ", ";
        }
        joined += e;
    }
    fprintf(stderr, "[Netplay] slot join refused: %s\n", joined.c_str());
}

void OnItemsReceived(const std::list<APClient::NetworkItem>& items) {
    if (sCore->roomMismatch) {
        // These belong to a room this save is not bound to; nothing here may
        // touch the cursor or the inbox. A RebindRoom + Sync re-fetches them.
        return;
    }
    for (const auto& item : items) {
        if (item.index < 0) {
            continue; // not part of the indexed stream
        }
        const uint32_t idx = (uint32_t)item.index;
        const uint32_t expected = ExpectedNextIndex();
        if (idx < expected) {
            continue; // retransmit of an index already applied or queued
        }
        if (idx > expected) {
            // Gap: we missed part of the stream (should not happen on a
            // healthy connection — AP resends from 0 on reconnect). Never
            // guess; ask the server to replay.
            ScheduleResync("index gap");
            return;
        }
        if (sCore->inbox.size() >= RSBS_NETPLAY_INBOX_CAP) {
            // Bounded-queue overflow policy: drop the tail WITHOUT advancing
            // any cursor and schedule a Sync. AP's index makes the replay
            // exact, so this is backpressure, not loss (anti-Anchor #3).
            sCore->droppedForResync++;
            ScheduleResync("inbox full");
            return;
        }

        PendingGrant grant;
        grant.apIndex = idx;
        GameId game = GAME_NONE;
        uint16_t localId = 0;
        if (NetplayItems_Decode(item.item, &game, &localId)) {
            grant.originGame = (uint8_t)game;
            grant.itemId = localId;
            grant.valid = true;
        } else {
            // Outside the RSBS id mapping (e.g. a foreign world's id in a
            // real AP room). Skipping it must still consume its index or the
            // stream wedges forever; it queues as an explicit no-op.
            sCore->decodeFailures++;
            fprintf(stderr, "[Netplay] item id %" PRId64 " (index %u) is outside the RSBS mapping — skipped\n",
                    item.item, idx);
            grant.originGame = (uint8_t)GAME_NONE;
            grant.itemId = 0;
            grant.valid = false;
        }
        sCore->inbox.push_back(grant);
    }
}

// ============================================================================
// Application (inside Netplay_Tick)
// ============================================================================

void ApplyPendingGrants() {
    if (sCore->suspended || sCore->roomMismatch) {
        return; // gated: buffer, never apply
    }
    while (!sCore->inbox.empty()) {
        const PendingGrant& g = sCore->inbox.front();
        if (g.apIndex != gComboCtx.netplayApAppliedCount) {
            // Contiguity invariant breach (cursor changed underneath us, e.g.
            // an external ComboContext reset). Drop everything and resync.
            fprintf(stderr, "[Netplay] inbox head %u != cursor %u — clearing and resyncing\n", g.apIndex,
                    gComboCtx.netplayApAppliedCount);
            sCore->inbox.clear();
            ScheduleResync("cursor moved externally");
            return;
        }
        if (g.valid) {
            if (sCore->sink == nullptr) {
                return; // nobody to hand grants to; hold position (bounded inbox)
            }
            const int rc = sCore->sink((GameId)g.originGame, g.itemId, g.apIndex, sCore->sinkCtx);
            if (rc != 0) {
                // Sink refused (e.g. durable storage full): keep the cursor,
                // retry on a later Tick. Backpressure, not loss.
                return;
            }
        }
        // Applied (or an explicit no-op): consume its index durably.
        sCore->inbox.pop_front();
        gComboCtx.netplayApAppliedCount++;
    }
}

void MaybeResync() {
    if (!sCore->resyncScheduled) {
        return;
    }
    if (sCore->client == nullptr || sCore->client->get_state() != APClient::State::SLOT_CONNECTED) {
        return; // retried once the slot is (re)connected
    }
    if (sCore->roomMismatch) {
        return; // pointless until rebound
    }
    // Only ask for a replay when the inbox can absorb it, and rate-limit so a
    // persistently full inbox does not turn into a Sync storm.
    if (sCore->inbox.size() > RSBS_NETPLAY_INBOX_CAP / 2) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - sCore->lastSyncRequest < std::chrono::milliseconds(100)) {
        return;
    }
    sCore->lastSyncRequest = now;
    sCore->resyncScheduled = false; // re-latches if the replay overflows again
    // A replay from 0 arrives with indices < cursor discarded and the tail
    // requeued; the inbox contiguity invariant needs a clean slate.
    sCore->inbox.clear();
    sCore->client->Sync();
}

NetplayConnectionState TranslateState() {
    if (sCore == nullptr) {
        return NETPLAY_STATE_UNINITIALIZED;
    }
    if (sCore->client == nullptr) {
        return NETPLAY_STATE_IDLE;
    }
    switch (sCore->client->get_state()) {
        case APClient::State::DISCONNECTED:
            return NETPLAY_STATE_DISCONNECTED;
        case APClient::State::SOCKET_CONNECTING:
        case APClient::State::SOCKET_CONNECTED:
            return NETPLAY_STATE_CONNECTING;
        case APClient::State::ROOM_INFO:
            return NETPLAY_STATE_ROOM_INFO;
        case APClient::State::SLOT_CONNECTED:
            return NETPLAY_STATE_ACTIVE;
    }
    return NETPLAY_STATE_DISCONNECTED;
}

std::string MakeUuid() {
    // Per-connection random identity. A stable per-install uuid (what
    // apuuid.hpp provides, via a dotfile) helps real servers correlate
    // reconnects; increment 1 deliberately writes no files.
    std::random_device rd;
    char buf[33];
    for (int i = 0; i < 4; i++) {
        snprintf(buf + i * 8, 9, "%08x", rd());
    }
    return std::string(buf, 32);
}

} // namespace

// ============================================================================
// Public API
// ============================================================================

extern "C" {

bool Netplay_Init(void) {
    if (sCore != nullptr) {
        fprintf(stderr, "[Netplay] Netplay_Init called twice\n");
        return false;
    }
    sCore = new NetplayCore();
    sCore->owner = std::this_thread::get_id();
    return true;
}

void Netplay_Shutdown(void) {
    if (sCore == nullptr) {
        return;
    }
    if (!OnOwnerThread("Netplay_Shutdown")) {
        return;
    }
    sCore->client.reset(); // closes the socket
    delete sCore;
    sCore = nullptr;
}

bool Netplay_Connect(const char* uri, const char* slotName, const char* password) {
    if (!OnOwnerThread("Netplay_Connect")) {
        return false;
    }
    if (uri == nullptr || slotName == nullptr || uri[0] == '\0' || slotName[0] == '\0') {
        fprintf(stderr, "[Netplay] Netplay_Connect: uri and slotName are required\n");
        return false;
    }
    if (strncmp(uri, "ws://", 5) != 0) {
        // Built with WSWRAP_NO_SSL: a scheme-less uri would make apclientpp
        // try wss:// first and fail confusingly; wss needs the deferred
        // OpenSSL-enabled build. Insist on an explicit ws:// for now.
        fprintf(stderr, "[Netplay] Netplay_Connect: uri must start with ws:// in this build (got '%s')\n", uri);
        return false;
    }
    Netplay_Disconnect();

    sCore->slotName = slotName;
    sCore->password = (password != nullptr) ? password : "";

    // The constructor starts the (non-blocking) connection attempt; from here
    // on everything happens inside Netplay_Tick. "RedShipBlueShip" is the AP
    // game name a future RSBS apworld would declare.
    sCore->client.reset(new APClient(MakeUuid(), "RedShipBlueShip", uri, "", &sCore->store));
    sCore->client->set_room_info_handler(OnRoomInfo);
    sCore->client->set_slot_connected_handler(OnSlotConnected);
    sCore->client->set_slot_refused_handler(OnSlotRefused);
    sCore->client->set_items_received_handler(OnItemsReceived);
    fprintf(stderr, "[Netplay] connecting to %s as '%s'\n", uri, slotName);
    return true;
}

void Netplay_Disconnect(void) {
    if (!OnOwnerThread("Netplay_Disconnect")) {
        return;
    }
    if (sCore->client != nullptr) {
        fprintf(stderr, "[Netplay] disconnected\n");
    }
    sCore->client.reset();
    sCore->inbox.clear();
    sCore->roomMismatch = false;
    sCore->resyncScheduled = false;
    sCore->slotRefused = false;
    sCore->connectedFingerprint = 0;
}

void Netplay_Tick(void) {
    if (!OnOwnerThread("Netplay_Tick")) {
        return;
    }
    if (sCore->client == nullptr) {
        return;
    }
    if (sCore->slotRefused) {
        // A refused join will refuse again; do not let poll() reconnect-loop
        // against it forever. The operator can Connect again with new params.
        Netplay_Disconnect();
        return;
    }
    sCore->client->poll(); // handlers above fire here, on this thread
    MaybeResync();
    ApplyPendingGrants();
}

void Netplay_OnGameSuspend(GameId departing) {
    (void)departing;
    if (!OnOwnerThread("Netplay_OnGameSuspend")) {
        return;
    }
    sCore->suspended = true;
}

void Netplay_OnGameResume(GameId arriving) {
    (void)arriving;
    if (!OnOwnerThread("Netplay_OnGameResume")) {
        return;
    }
    sCore->suspended = false;
}

bool Netplay_RebindRoom(void) {
    if (!OnOwnerThread("Netplay_RebindRoom")) {
        return false;
    }
    if (!sCore->roomMismatch || sCore->connectedFingerprint == 0) {
        return false;
    }
    gComboCtx.netplayApRoomFingerprint = sCore->connectedFingerprint;
    gComboCtx.netplayApAppliedCount = 0;
    sCore->roomMismatch = false;
    sCore->inbox.clear();
    ScheduleResync("room rebound");
    fprintf(stderr, "[Netplay] rebound to the connected room; cursor reset to 0\n");
    return true;
}

NetplayStatus Netplay_GetStatus(void) {
    NetplayStatus st;
    memset(&st, 0, sizeof(st));
    st.state = TranslateState();
    if (sCore == nullptr) {
        return st;
    }
    if (std::this_thread::get_id() != sCore->owner) {
        fprintf(stderr, "[Netplay] Netplay_GetStatus called off the owning thread — refused\n");
        return st;
    }
    st.suspended = sCore->suspended;
    st.roomMismatch = sCore->roomMismatch;
    st.resyncScheduled = sCore->resyncScheduled;
    st.pendingGrants = (uint32_t)sCore->inbox.size();
    st.appliedCount = gComboCtx.netplayApAppliedCount;
    st.droppedForResync = sCore->droppedForResync;
    st.decodeFailures = sCore->decodeFailures;
    return st;
}

void Netplay_SetGrantSink(NetplayGrantSink sink, void* userCtx) {
    if (!OnOwnerThread("Netplay_SetGrantSink")) {
        return;
    }
    sCore->sink = sink;
    sCore->sinkCtx = userCtx;
}

} // extern "C"
