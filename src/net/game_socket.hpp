#pragma once

#include "address.hpp"
#include "udp_socket.hpp"
#include "tcp_socket.hpp"

#include <data/packets/packet.hpp>
#include <crypto/box.hpp>

class GLOBED_DLL GameSocket {
    static constexpr uint8_t MARKER_CONN_INITIAL = 0xe0;
    static constexpr uint8_t MARKER_CONN_RECOVERY = 0xe1;

public:
    GameSocket();
    ~GameSocket();

    Result<> connect(const NetworkAddress& address, bool isRecovering);
    void disconnect();
    bool isConnected();

    struct ReceivedPacket {
        std::shared_ptr<Packet> packet;
        bool fromConnected;
    };

    // Try to receive a packet on the TCP socket
    Result<std::shared_ptr<Packet>> recvPacketTCP();

    // Try to receive a packet on the UDP socket
    Result<ReceivedPacket> recvPacketUDP();

    // Try to receive a packet
    Result<ReceivedPacket> recvPacket();

    // Try to receive a packet, returns "timed out" if timeout is reached.
    Result<ReceivedPacket> recvPacket(int timeoutMs);

    // Send a packet to the currently active connection. Throws if disconnected
    Result<> sendPacket(std::shared_ptr<Packet> packet);

    // Send a UDP packet to a specific address
    Result<> sendPacketTo(std::shared_ptr<Packet> packet, const NetworkAddress& address);

    Result<> sendRecoveryData(int accountId, uint32_t secretKey);

    void cleanupBox();
    void createBox();

    void togglePacketLogging(bool enabled);

    enum class PollResult {
        None, Tcp, Udp, Both
    };

    Result<PollResult> poll(int timeoutMs);

private:
    friend class NetworkManager;

    TcpSocket tcpSocket;
    UdpSocket udpSocket;

    std::unique_ptr<CryptoBox> cryptoBox;
    util::data::byte* dataBuffer;

    bool dumpPackets = false;

    // Write a packet, packet header, and optionally length if the packet is TCP to the given buffer.
    Result<> encodePacket(Packet& packet, ByteBuffer& buffer);

    // Decode a packet from a buffer
    Result<std::shared_ptr<Packet>> decodePacket(ByteBuffer& buffer);

    void dumpPacket(packetid_t id, ByteBuffer& buffer, bool sending);
};
