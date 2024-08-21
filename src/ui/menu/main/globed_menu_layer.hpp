#pragma once
#include <defs/all.hpp>

#include <Geode/utils/web.hpp>

#include <managers/web.hpp>

class GlobedMenuLayer : public cocos2d::CCLayer {
public:
    static constexpr float LIST_WIDTH = 358.f;
    static constexpr float LIST_HEIGHT = 220.f;

    static GlobedMenuLayer* create();
    ~GlobedMenuLayer();

private:
    Ref<CCMenuItemSpriteExtra> levelListButton, serverSwitcherButton, discordButton, settingsButton;
    Ref<cocos2d::CCSprite> featuredBtnGlow, featuredPopupNew;
    cocos2d::CCMenu *leftButtonMenu, *rightButtonMenu, *dailyButtonMenu;
    cocos2d::CCSprite* background;

    bool currentlyShowingButtons = false;

    bool init() override;
    void keyBackClicked() override;
    void keyDown(cocos2d::enumKeyCodes key) override;
    void update(float dt) override;
    void navigateToServerLayer();
    void onEnterTransitionDidFinish() override;
};
