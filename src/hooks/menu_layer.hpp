#pragma once
#include <defs/geode.hpp>

#include <Geode/modify/MenuLayer.hpp>

struct GLOBED_DLL HookedMenuLayer : geode::Modify<HookedMenuLayer, MenuLayer> {
    struct Fields {
        bool btnActive = false;
        Ref<CCMenuItemSpriteExtra> globedBtn = nullptr;
    };

    $override
    bool init();

#if 0
    $override
    void onMoreGames(cocos2d::CCObject* s);
#endif

    void updateGlobedButton();
    void maybeUpdateButton(float);

    void onGlobedButton(cocos2d::CCObject*);
};