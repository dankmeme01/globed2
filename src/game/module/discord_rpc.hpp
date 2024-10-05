#pragma once

#include "base.hpp"
#include <defs/platform.hpp>

class GLOBED_DLL DiscordRpcModule : public BaseGameplayModule {
public:
    DiscordRpcModule(GlobedGJBGL* gameLayer) : BaseGameplayModule(gameLayer) {}

    void selUpdate(float dt) override;
    void postInitActions() override;
    void onQuit() override;

private:
    float counter = 0.f;

    void postDRPCEvent();
};