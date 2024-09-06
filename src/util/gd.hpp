#pragma once

#include <defs/platform.hpp>
#include <Geode/Bindings.hpp>

class PlayerIconData;
enum class PlayerIconType : uint8_t;

namespace util::gd {
    GLOBED_DLL void reorderDownloadedLevel(GJGameLevel* level);
    GLOBED_DLL void openProfile(int accountId, int userId, const std::string& name);
    GLOBED_DLL int calcLevelDifficulty(GJGameLevel* level);

    int getIconWithType(const PlayerIconData& data, PlayerIconType type);
    int getIconWithType(const PlayerIconData& data, IconType type);

    enum class GameVariable {
        FastRespawn = 52,
        FastMenu = 168,
    };

    bool variable(GameVariable var);
    void setVariable(GameVariable var, bool state);
}