/**
 * @file mock_ap_server.hpp
 * @brief In-process mock Archipelago server for the netplay loopback harness.
 *
 * A minimal AP-protocol WebSocket server (websocketpp, plain asio transport,
 * no TLS) speaking exactly the subset the transport exercises: RoomInfo on
 * connect, Connected in reply to Connect, ReceivedItems (initial, pushed,
 * and Sync replays), and DataPackage on request. Listens on 127.0.0.1 with
 * an ephemeral port.
 *
 * DETERMINISM: the server is polled, not threaded. Tests interleave
 * MockApServer::Poll() with Netplay_Tick() on one thread, so every test run
 * executes the same event order — no scheduler-dependent flakiness. This is
 * test infrastructure only; it is never compiled into a shipping build
 * (RSBS_NETPLAY=ON builds compile it into the test TU of redship).
 */

#ifndef RSBS_NETPLAY_TESTS_MOCK_AP_SERVER_HPP
#define RSBS_NETPLAY_TESTS_MOCK_AP_SERVER_HPP

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

class MockApServer {
    using WsServer = websocketpp::server<websocketpp::config::asio>;
    using json = nlohmann::json;

  public:
    MockApServer() = default;
    ~MockApServer() {
        Stop();
    }

    /** Bind 127.0.0.1 on an ephemeral port and start accepting. */
    bool Start(const std::string& seedName) {
        mSeedName = seedName;
        mServer.clear_access_channels(websocketpp::log::alevel::all);
        mServer.clear_error_channels(websocketpp::log::elevel::all);
        mServer.init_asio();
        mServer.set_reuse_addr(true);
        mServer.set_open_handler([this](websocketpp::connection_hdl hdl) { OnOpen(hdl); });
        mServer.set_close_handler([this](websocketpp::connection_hdl hdl) { OnClose(hdl); });
        mServer.set_message_handler(
            [this](websocketpp::connection_hdl hdl, WsServer::message_ptr msg) { OnMessage(hdl, msg); });

        websocketpp::lib::error_code ec;
        mServer.listen(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0), ec);
        if (ec) {
            return false;
        }
        mServer.start_accept(ec);
        if (ec) {
            return false;
        }
        mPort = mServer.get_local_endpoint(ec).port();
        return !ec;
    }

    void Stop() {
        if (mPort == 0) {
            return;
        }
        websocketpp::lib::error_code ec;
        mServer.stop_listening(ec);
        for (auto& hdl : mConnections) {
            mServer.close(hdl, websocketpp::close::status::going_away, "server stopping", ec);
        }
        mConnections.clear();
        // Drain the close handshakes without blocking.
        for (int i = 0; i < 16; i++) {
            mServer.poll();
        }
        mPort = 0;
    }

    /** Service pending socket work (non-blocking). */
    void Poll() {
        mServer.poll();
    }

    uint16_t Port() const {
        return mPort;
    }
    std::string Uri() const {
        return "ws://127.0.0.1:" + std::to_string(mPort);
    }
    bool ClientConnected() const {
        return !mConnections.empty();
    }
    int ConnectPacketsSeen() const {
        return mConnectPacketsSeen;
    }
    int SyncPacketsSeen() const {
        return mSyncPacketsSeen;
    }

    /** Append an item to the room's stream and push it to the client. */
    void PushItem(int64_t apItemId) {
        mItems.push_back(apItemId);
        Broadcast(ReceivedItemsFrom((int)mItems.size() - 1));
    }

    /** Append without pushing (delivered only by a later Sync/retransmit). */
    void StageItem(int64_t apItemId) {
        mItems.push_back(apItemId);
    }

    /** Retransmit the full stream from index 0 (dup/replay simulation). */
    void RetransmitAll() {
        Broadcast(ReceivedItemsFrom(0));
    }

  private:
    json VersionJson() const {
        return json{ { "major", 0 }, { "minor", 5 }, { "build", 1 }, { "class", "Version" } };
    }

    json ReceivedItemsFrom(int startIndex) const {
        json items = json::array();
        for (size_t i = (size_t)startIndex; i < mItems.size(); i++) {
            items.push_back(json{
                { "item", mItems[i] },
                { "location", (int64_t)(1000 + i) },
                { "player", 2 }, // "another player found it"
                { "flags", 1 },
            });
        }
        return json{ { "cmd", "ReceivedItems" }, { "index", startIndex }, { "items", items } };
    }

    void Send(websocketpp::connection_hdl hdl, const json& command) {
        websocketpp::lib::error_code ec;
        mServer.send(hdl, json::array({ command }).dump(), websocketpp::frame::opcode::text, ec);
    }

    void Broadcast(const json& command) {
        for (auto& hdl : mConnections) {
            Send(hdl, command);
        }
    }

    void OnOpen(websocketpp::connection_hdl hdl) {
        mConnections.insert(hdl);
        Send(hdl, json{
                      { "cmd", "RoomInfo" },
                      { "time", 0.0 },
                      { "version", VersionJson() },
                      { "generator_version", VersionJson() },
                      { "seed_name", mSeedName },
                      { "games", json::array({ "RedShipBlueShip" }) },
                      { "datapackage_checksums", json{ { "RedShipBlueShip", "rsbs-stub" } } },
                      { "hint_cost", 0 },
                      { "location_check_points", 0 },
                      { "password", false },
                      { "tags", json::array() },
                  });
    }

    void OnClose(websocketpp::connection_hdl hdl) {
        mConnections.erase(hdl);
    }

    void OnMessage(websocketpp::connection_hdl hdl, WsServer::message_ptr msg) {
        json packet = json::parse(msg->get_payload(), nullptr, false);
        if (!packet.is_array()) {
            return;
        }
        for (const auto& command : packet) {
            const std::string cmd = command.value("cmd", "");
            if (cmd == "Connect") {
                mConnectPacketsSeen++;
                Send(hdl, json{
                              { "cmd", "Connected" },
                              { "team", 0 },
                              { "slot", 1 },
                              { "players", json::array({ json{ { "team", 0 },
                                                              { "slot", 1 },
                                                              { "alias", "P1" },
                                                              { "name", command.value("name", "P1") } } }) },
                              { "checked_locations", json::array() },
                              { "missing_locations", json::array() },
                              { "hint_points", 0 },
                          });
                if (!mItems.empty()) {
                    Send(hdl, ReceivedItemsFrom(0));
                }
            } else if (cmd == "Sync") {
                mSyncPacketsSeen++;
                Send(hdl, ReceivedItemsFrom(0));
            } else if (cmd == "GetDataPackage") {
                Send(hdl, json{ { "cmd", "DataPackage" }, { "data", json{ { "games", json::object() } } } });
            }
            // Everything else (LocationChecks, StatusUpdate, ...) is ignored.
        }
    }

    WsServer mServer;
    std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> mConnections;
    std::vector<int64_t> mItems;
    std::string mSeedName;
    uint16_t mPort = 0;
    int mConnectPacketsSeen = 0;
    int mSyncPacketsSeen = 0;
};

#endif // RSBS_NETPLAY_TESTS_MOCK_AP_SERVER_HPP
