#pragma once
#include <defs/geode.hpp>

#include <Geode/modify/GJGameLevel.hpp>

// revolutionary
struct GLOBED_DLL HookedGJGameLevel : geode::Modify<HookedGJGameLevel, GJGameLevel> {
    struct Fields {
        bool shouldTransitionWithPopScene = false;
        int rateTier = -1;
    };

#ifndef GEODE_IS_ARM_MAC
    $override
    void savePercentage(int, bool, int, int, bool);
#endif

    static LevelId getLevelIDFrom(GJGameLevel* level);
};
