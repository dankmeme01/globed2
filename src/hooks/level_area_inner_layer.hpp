#pragma once
#include <defs/geode.hpp>

#include <Geode/modify/LevelAreaInnerLayer.hpp>

#include <data/types/gd.hpp>

struct GLOBED_DLL HookedLevelAreaInnerLayer : geode::Modify<HookedLevelAreaInnerLayer, LevelAreaInnerLayer> {
    static inline const auto TOWER_LEVELS = std::to_array<LevelId>({5001, 5002, 5003, 5004});

    struct Fields {
        std::unordered_map<int, uint16_t> levels;
        std::unordered_map<int, Ref<cocos2d::CCNode>> doorNodes;
    };

    $override
    bool init(bool p0);

    $override
    void onBack(cocos2d::CCObject*);

    $override
    void onDoor(cocos2d::CCObject*);

    void sendRequest(float);
    void updatePlayerCounts();
};
