#pragma once
#include <data/packets/packet.hpp>
#include <data/types/room.hpp>

// 13000 - CreateRoomPacket
class CreateRoomPacket : public Packet {
    GLOBED_PACKET(13000, CreateRoomPacket, false, false)

    CreateRoomPacket() {}
    CreateRoomPacket(const std::string_view roomName, const std::string_view password, const RoomSettings& settings)
        : roomName(roomName), password(password), settings(settings) {}

    std::string roomName;
    std::string password;
    RoomSettings settings;
};

GLOBED_SERIALIZABLE_STRUCT(CreateRoomPacket, (roomName, password, settings));

// 13001 - JoinRoomPacket
class JoinRoomPacket : public Packet {
    GLOBED_PACKET(13001, JoinRoomPacket, false, false)

    JoinRoomPacket() {}
    JoinRoomPacket(uint32_t roomId, const std::string_view password) : roomId(roomId), password(password) {}

    uint32_t roomId;
    std::string password;
};

GLOBED_SERIALIZABLE_STRUCT(JoinRoomPacket, (roomId, password));

// 13002 - LeaveRoomPacket
class LeaveRoomPacket : public Packet {
    GLOBED_PACKET(13002, LeaveRoomPacket, false, false)

    LeaveRoomPacket() {}
};

GLOBED_SERIALIZABLE_STRUCT(LeaveRoomPacket, ());

// 13003 - RequestRoomPlayerListPacket
class RequestRoomPlayerListPacket : public Packet {
    GLOBED_PACKET(13003, RequestRoomPlayerListPacket, false, false)

    RequestRoomPlayerListPacket() {}
};

GLOBED_SERIALIZABLE_STRUCT(RequestRoomPlayerListPacket, ());

// 13004 - UpdateRoomSettingsPacket
class UpdateRoomSettingsPacket : public Packet {
    GLOBED_PACKET(13004, UpdateRoomSettingsPacket, false, false)

    UpdateRoomSettingsPacket() {}
    UpdateRoomSettingsPacket(const RoomSettings& settings) : settings(settings) {}

    RoomSettings settings;
};

GLOBED_SERIALIZABLE_STRUCT(UpdateRoomSettingsPacket, (settings));

// 13005 - RoomSendInvitePacket
class RoomSendInvitePacket : public Packet {
    GLOBED_PACKET(13005, RoomSendInvitePacket, false, false)

    RoomSendInvitePacket() {}
    RoomSendInvitePacket(int32_t player) : player(player) {}

    int32_t player;
};

GLOBED_SERIALIZABLE_STRUCT(RoomSendInvitePacket, (player));

// 13006 - RequestRoomListPacket
class RequestRoomListPacket : public Packet {
    GLOBED_PACKET(13006, RequestRoomListPacket, false, false)

    RequestRoomListPacket() {}
};
GLOBED_SERIALIZABLE_STRUCT(RequestRoomListPacket, ());

// 13007 - CloseRoomPacket
class CloseRoomPacket : public Packet {
    GLOBED_PACKET(13007, CloseRoomPacket, false, false)

    CloseRoomPacket() {}
    CloseRoomPacket(uint32_t roomId) : roomId(roomId) {}

    uint32_t roomId;
};
GLOBED_SERIALIZABLE_STRUCT(CloseRoomPacket, (roomId));
