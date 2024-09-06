#pragma once
#include <data/types/gd.hpp>
#include <data/bytebuffer.hpp>
#include <data/bitfield.hpp>

struct RoomSettingsFlags : BitfieldBase {
    bool isHidden;
    bool publicInvites;
    bool collision;
    bool twoPlayerMode;
    bool deathlink;

    // we need the struct to be 2 bytes
    bool _pad1, _pad2, _pad3, _pad4, _pad5;
};

static_assert((sizeof(RoomSettingsFlags) + 7) / 8 == 2);

GLOBED_SERIALIZABLE_BITFIELD(RoomSettingsFlags, (
    isHidden, publicInvites, collision, twoPlayerMode, deathlink
))

struct RoomSettings {
    RoomSettingsFlags flags;
    uint16_t playerLimit;
    LevelId levelId;
    bool fasterReset;
};

GLOBED_SERIALIZABLE_STRUCT(RoomSettings, (
    flags, playerLimit, levelId, fasterReset
))

struct RoomInfo {
    uint32_t id;
    PlayerPreviewAccountData owner;
    std::string name;
    std::string password;
    RoomSettings settings;
};

GLOBED_SERIALIZABLE_STRUCT(RoomInfo, (
    id, owner, name, password, settings
));

struct RoomListingInfo {
    uint32_t id;
    uint16_t playerCount;
    PlayerPreviewAccountData owner;
    std::string name;
    bool hasPassword;
    RoomSettings settings;
};

GLOBED_SERIALIZABLE_STRUCT(RoomListingInfo, (
    id, playerCount, owner, name, hasPassword, settings
));
