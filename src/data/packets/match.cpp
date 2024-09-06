#include "all.hpp" // include all packets

#define PACKET(pt) case pt::PACKET_ID: return std::make_shared<pt>()

std::shared_ptr<Packet> matchPacket(packetid_t packetId) {
    switch (packetId) {
        // connection related

        PACKET(PingResponsePacket);
        PACKET(CryptoHandshakeResponsePacket);
        PACKET(KeepaliveResponsePacket);
        PACKET(ServerDisconnectPacket);
        PACKET(LoggedInPacket);
        PACKET(LoginFailedPacket);
        PACKET(ProtocolMismatchPacket);
        PACKET(KeepaliveTCPResponsePacket);
        PACKET(ClaimThreadFailedPacket);
        PACKET(LoginRecoveryFailecPacket);

        PACKET(ServerNoticePacket);
        PACKET(ServerBannedPacket);
        PACKET(ServerMutedPacket);
        PACKET(ConnectionTestResponsePacket);

        // general

        PACKET(GlobalPlayerListPacket);
        PACKET(LevelListPacket);
        PACKET(LevelPlayerCountPacket);
        PACKET(RolesUpdatedPacket);
        PACKET(LinkCodeResponsePacket);

        // game related

        PACKET(PlayerProfilesPacket);
        PACKET(LevelDataPacket);
        PACKET(LevelPlayerMetadataPacket);
        PACKET(VoiceBroadcastPacket);
        PACKET(ChatMessageBroadcastPacket);

        // room related

        PACKET(RoomCreatedPacket);
        PACKET(RoomJoinedPacket);
        PACKET(RoomJoinFailedPacket);
        PACKET(RoomPlayerListPacket);
        PACKET(RoomInfoPacket);
        PACKET(RoomInvitePacket);
        PACKET(RoomListPacket);
        PACKET(RoomCreateFailedPacket);

        // admin related

        PACKET(AdminAuthSuccessPacket);
        PACKET(AdminErrorPacket);
        PACKET(AdminUserDataPacket);
        PACKET(AdminSuccessMessagePacket);
        PACKET(AdminAuthFailedPacket);

        default:
            return std::shared_ptr<Packet>(nullptr);
    }
}