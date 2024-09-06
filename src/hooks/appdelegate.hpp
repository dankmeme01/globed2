#pragma once
#include <defs/geode.hpp>

#include <Geode/modify/AppDelegate.hpp>

#include <util/time.hpp>

struct GLOBED_DLL GlobedAppDelegate : geode::Modify<GlobedAppDelegate, AppDelegate> {
#ifdef GEODE_IS_ANDROID
    $override
    void applicationDidEnterBackground();

    $override
    void applicationWillEnterForeground();
#endif // GEODE_IS_ANDROID
};
