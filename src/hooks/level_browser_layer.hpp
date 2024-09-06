#pragma once
#include <defs/geode.hpp>

#include <Geode/modify/LevelBrowserLayer.hpp>

#include <data/types/gd.hpp>

struct GLOBED_DLL HookedLevelBrowserLayer : geode::Modify<HookedLevelBrowserLayer, LevelBrowserLayer> {
    struct Fields {
        std::unordered_map<LevelId, uint16_t> levels;
    };

    $override
    void setupLevelBrowser(cocos2d::CCArray* p0);

    void refreshPagePlayerCounts();
    void updatePlayerCounts(float);

    constexpr bool isValidLevelType(GJLevelType level) {
        return (int)level == 3 || (int)level == 4;
    }
};
