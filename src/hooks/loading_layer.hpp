#pragma once
#include <defs/geode.hpp>

#include <Geode/modify/LoadingLayer.hpp>

#include <managers/settings.hpp>
#include <util/ui.hpp>
#include <util/time.hpp>

// GLOBED_LOADING_FINISHED_MIDHOOK - whether to midhook the inlined func
#if defined(GEODE_IS_WINDOWS) || defined(GEODE_IS_IOS) || defined(GEODE_IS_MACOS)
# define GLOBED_LOADING_FINISHED_MIDHOOK 1
#endif

struct GLOBED_DLL HookedLoadingLayer : geode::Modify<HookedLoadingLayer, LoadingLayer> {
    struct Fields {
        int preloadingStage = 0;
        util::time::system_time_point loadingStartedTime;
    };

#ifndef GLOBED_LOADING_FINISHED_MIDHOOK
    static void onModify(auto& self) {
        (void) self.setHookPriority("LoadingLayer::loadingFinished", -99999999);
    }

    void loadingFinished();
#endif

    void loadingFinishedHook();

    void setLabelText(const std::string_view text);
    void setLabelTextForStage();
    void finishLoading();

    void preloadingStage1(float);
    void preloadingStage2(float);
    void preloadingStage3(float);
};
