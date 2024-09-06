#pragma once
#include <defs/geode.hpp>

#include <Geode/modify/PauseLayer.hpp>

// Defines globed-related PauseLayer hooks
struct GLOBED_DLL GlobedPauseLayer : geode::Modify<GlobedPauseLayer, PauseLayer> {
    static void onModify(auto& self) {
        (void) self.setHookPriority("PauseLayer::keyDown", -999999999);
        (void) self.setHookPriority("PauseLayer::goEdit", -999999999);
    }

    bool hasPopup();

    $override
    void customSetup();

    $override
    void goEdit();

    void selUpdate(float dt);

    // callbacks

    $override
    void onQuit(cocos2d::CCObject*);

    $override
    void onResume(cocos2d::CCObject*);

    $override
    void onRestart(cocos2d::CCObject*);

    $override
    void onRestartFull(cocos2d::CCObject*);

    $override
    void onEdit(cocos2d::CCObject*);

    $override
    void onNormalMode(cocos2d::CCObject*);

    $override
    void onPracticeMode(cocos2d::CCObject*);
};
