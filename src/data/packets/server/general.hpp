#pragma once
#include <data/packets/packet.hpp>
#include <data/types/gd.hpp>
#include <data/types/misc.hpp>

// 21000 - GlobalPlayerListPacket
class GlobalPlayerListPacket : public Packet {
    GLOBED_PACKET(21000, GlobalPlayerListPacket, false, false)

    GlobalPlayerListPacket() {}

    std::vector<PlayerPreviewAccountData> data;
};

GLOBED_SERIALIZABLE_STRUCT(GlobalPlayerListPacket, (data));

// 21001 - LevelListPacket
class LevelListPacket : public Packet {
    GLOBED_PACKET(21001, LevelListPacket, false, false)

    LevelListPacket() {}

    std::vector<GlobedLevel> levels;
};

GLOBED_SERIALIZABLE_STRUCT(LevelListPacket, (levels));

// 21002 - LevelPlayerCountPacket
class LevelPlayerCountPacket : public Packet {
    GLOBED_PACKET(21002, LevelPlayerCountPacket, false, false)

    LevelPlayerCountPacket() {}

    std::vector<std::pair<LevelId, uint16_t>> levels;
};

GLOBED_SERIALIZABLE_STRUCT(LevelPlayerCountPacket, (levels));

// 21003 - RolesUpdatedPacket
class RolesUpdatedPacket : public Packet {
    GLOBED_PACKET(21003, RolesUpdatedPacket, false, false)

    RolesUpdatedPacket() {}

    SpecialUserData specialUserData;
};

GLOBED_SERIALIZABLE_STRUCT(RolesUpdatedPacket, (specialUserData));

// 21004 - LinkCodeResponsePacket
class LinkCodeResponsePacket : public Packet {
    GLOBED_PACKET(21004, LinkCodeResponsePacket, false, false);

    LinkCodeResponsePacket() {}

    uint32_t linkCode;
};

GLOBED_SERIALIZABLE_STRUCT(LinkCodeResponsePacket, (linkCode));
