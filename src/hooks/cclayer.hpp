#pragma once
#include <defs/geode.hpp>

#ifndef GEODE_IS_WINDOWS

#include <Geode/modify/CCLayer.hpp>

struct GLOBED_DLL HookedCCLayer : geode::Modify<HookedCCLayer, cocos2d::CCLayer> {
    void onEnter();
};

#endif // GEODE_IS_WINDOWS
