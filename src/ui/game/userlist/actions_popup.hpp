#pragma once
#include <defs/all.hpp>

class GLOBED_DLL GlobedUserActionsPopup : public geode::Popup<int, cocos2d::CCArray*> {
public:
    static constexpr float POPUP_WIDTH = 200.f;
    static constexpr float POPUP_HEIGHT = 90.f;

    static GlobedUserActionsPopup* create(int accountId, cocos2d::CCArray* buttons);

private:
    Ref<cocos2d::CCMenu> buttonLayout;
    int accountId;

    bool setup(int accountId, cocos2d::CCArray* buttons) override;
};
