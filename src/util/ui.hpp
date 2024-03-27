#pragma once

#include <defs/all.hpp>
#include <functional>

namespace util::ui {
    constexpr cocos2d::ccColor4B BG_COLOR_BROWN = {191, 114, 62, 255};
    constexpr cocos2d::ccColor4B BG_COLOR_TRANSPARENT = {0, 0, 0, 180};

    void switchToScene(cocos2d::CCLayer* layer);
    void prepareLayer(cocos2d::CCLayer* layer);
    void addBackground(cocos2d::CCNode* layer);
    void addBackButton(cocos2d::CCMenu* menu, std::function<void()> callback);
    void navigateBack();

    // set the scale for `node` in a way that `node->getScaledContentSize()` will match `target->getScaledContentSize()`
    void rescaleToMatch(cocos2d::CCNode* node, cocos2d::CCNode* target, bool stretch = false);

    // set the scale for `node` in a way that `node->getScaledContentSize()` will match `targetSize`
    void rescaleToMatch(cocos2d::CCNode *node, cocos2d::CCSize targetSize, bool stretch = false);

    float getScrollPos(BoomListView* listView);
    void setScrollPos(BoomListView* listView, float pos);

    void tryLoadDeathEffect(int id);

    // small wrapper with precalculated sizes to make ui easier
    struct PopupLayout {
        cocos2d::CCSize winSize, popupSize;
        float left, right, bottom, top;

        cocos2d::CCSize center, centerLeft, centerTop, centerRight, centerBottom;
        cocos2d::CCSize bottomLeft, topLeft, bottomRight, topRight;
    };

    PopupLayout getPopupLayout(cocos2d::CCSize popupSize);

    cocos2d::CCNode* findChildByMenuSelectorRecursive(cocos2d::CCNode* node, uintptr_t function);   

    uibuilder::Build<cocos2d::CCSprite> createBadge(std::string badgePNG, cocos2d::CCPoint pos, std::string id);

    cocos2d::CCSprite* createBadgeIfSpecial(cocos2d::ccColor3B color, cocos2d::CCPoint pos);
}
