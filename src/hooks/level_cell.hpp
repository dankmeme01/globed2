#pragma once
#include "Geode/modify/Modify.hpp"
#include <defs/geode.hpp>

#include <Geode/modify/LevelCell.hpp>

struct GLOBED_DLL GlobedLevelCell : geode::Modify<GlobedLevelCell, LevelCell> {
    struct Fields {
        cocos2d::CCLabelBMFont* playerCountLabel = nullptr;
        cocos2d::CCSprite* playerCountIcon = nullptr;
        int rateTier = -1;
    };

    void updatePlayerCount(int count, bool inLists = false);
    void modifyToFeaturedCell(int rating);
    void onClick(CCObject* sender);
};
