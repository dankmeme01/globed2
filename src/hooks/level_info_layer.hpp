#pragma once
#include <defs/geode.hpp>

#include <Geode/modify/LevelInfoLayer.hpp>

struct GLOBED_DLL HookedLevelInfoLayer : geode::Modify<HookedLevelInfoLayer, LevelInfoLayer> {
    static void onModify(auto& self) {
        (void) self.setHookPriority("LevelInfoLayer::onPlay", -9999);
    }

    struct Fields {
        bool allowOpeningAnyway = false;
        int rateTier = -1;
        Ref<cocos2d::CCLabelBMFont> playerCountLabel;
    };

    $override
    bool init(GJGameLevel* level, bool challenge);

    $override
    void onPlay(cocos2d::CCObject*);

    $override
    void tryCloneLevel(cocos2d::CCObject*);

    void forcePlay(cocos2d::CCObject*);

    void addLevelSendButton();

    void addRoomLevelButton();

    void addPlayerCountLabel();
    void updatePlayerCount(int);
    void scheduledPlayerCountUpdate(float);
};
