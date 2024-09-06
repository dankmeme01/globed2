#pragma once

#include <Geode/utils/Result.hpp>
#include <defs/platform.hpp>

#include <util/singleton.hpp>

using packetid_t = uint16_t;

class PacketListener;
class NetworkAddress;
struct GameServer;
class Packet;
struct UserPrivacyFlags;

template <typename T>
concept HasPacketID = requires { T::PACKET_ID; };

namespace cocos2d {
    class CCNode;
}

class GLOBED_DLL NetworkManager : public SingletonBase<NetworkManager> {
protected:
    friend class SingletonBase;
    NetworkManager();
    ~NetworkManager();

public:
    using PacketCallback = std::function<void(std::shared_ptr<Packet>)>;

    template <HasPacketID Pty>
    using PacketCallbackSpecific = std::function<void(std::shared_ptr<Pty>)>;

    static constexpr unsigned char SERVER_MAGIC[10] = {0xdd, 0xee, 'g', 'l', 'o', 'b', 'e', 'd', 0xda, 0xee};

    enum class ConnectionState : int {
        Disconnected,    // not connected to any server
        TcpConnecting,   // attempting to establish a TCP connection
        Authenticating,  // performing a handshake and login
        Established,     // fully connected to a server
    };

    // Connect to a server
    geode::Result<> connect(const NetworkAddress& address, const std::string_view serverId, bool standalone);

    // Connect to a server
    geode::Result<> connect(const GameServer& gsview);

    // Connect to the currently selected standalone server
    geode::Result<> connectStandalone();

    // Disconnect from the server, does nothing if not connected
    void disconnect(bool quiet = false, bool noclear = false);

    // Call `disconnect` and show an error popup with a message
    void disconnectWithMessage(const std::string_view message, bool quiet = true);

    // Cancel reconnection
    void cancelReconnect();

    // Sends a packet to the currently established connection. Throws if disconnected.
    void send(std::shared_ptr<Packet> packet);

    // Pings all known servers and stores the pings in `GameServerManager`
    void pingServers();

    // If connected, pings the active server if there have been no pings for >5 seconds.
    void updateServerPing();

    // Registers a packet listener and adds it to `target`
    void addListener(cocos2d::CCNode* target, packetid_t id, PacketListener* listener);

    // Adds a packet listener and calls your callback function when a packet with `id` is received.
    // If there already was a callback with this packet ID, it gets replaced.
    // All callbacks are ran in the main (GD) thread.
    void addListener(cocos2d::CCNode* target, packetid_t id, PacketCallback&& callback, int priority = 0, bool isFinal = false);

    // Same as addListener(packetid_t, PacketCallback) but hacky syntax xd
    template <HasPacketID Pty>
    void addListener(cocos2d::CCNode* target, PacketCallbackSpecific<Pty>&& callback, int priority = 0, bool isFinal = false) {
        this->addListener(target, Pty::PACKET_ID, [callback = std::move(callback)](std::shared_ptr<Packet> pkt) {
            return callback(std::static_pointer_cast<Pty>(pkt));
        }, priority, isFinal);
    }

    // Removes a listener by packet ID.
    void removeListener(cocos2d::CCNode* target, packetid_t id);

    // Same as removeListener(packetid_t) but hacky syntax once again
    template <HasPacketID T>
    void removeListener(cocos2d::CCNode* target) {
        this->removeListener(target, T::PACKET_ID);
    }

    // Removes all listeners.
    void removeAllListeners();

    // Enable whether packets are logged to a file (and to the console)
    void togglePacketLogging(bool enabled);

    // Returns the protocol version of this client
    uint16_t getUsedProtocol();

    // Get the TPS of the currently connected server, or 0
    uint32_t getServerTps();

    // Get the maximum protocol version of the currently connected server
    uint16_t getServerProtocol();

    // Returns true if we are connected to a standalone game server, not tied to any central server.
    bool standalone();

    // Returns the current connection state
    ConnectionState getConnectionState();

    // Returns whether we are connected to a server and have logged in
    bool established();

    // Returns whether we are currently trying to reconnect to a server due to an earlier connection break.
    bool reconnecting();

    // Pause all network threads
    void suspend();
    // Resume all network threads
    void resume();

    uint16_t getMinProtocol();
    uint16_t getMaxProtocol();
    bool isProtocolSupported(uint16_t version);

    // Packet sending
    void sendUpdatePlayerStatus(const UserPrivacyFlags& flags);
    void sendRequestRoomPlayerList();
    void sendLeaveRoom();
    void sendCloseRoom(uint32_t roomId = 0);
    void sendRequestPlayerCount(LevelId id);
    void sendRequestPlayerCount(std::vector<LevelId> ids);
    void sendLinkCodeRequest();

private:
    class Impl;
    Impl* impl;

    friend class PacketListener;
    friend class PacketListenerPool;
    void unregisterPacketListener(packetid_t packet, PacketListener* listener, bool suppressUnhandled = true);
};