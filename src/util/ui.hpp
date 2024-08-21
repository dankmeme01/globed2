#pragma once

#include <defs/all.hpp>
#include <data/types/gd.hpp>
#include <ui/ui.hpp>
#include <functional>

class UserEntry;

namespace util::ui {
    const cocos2d::CCSize BADGE_SIZE = {16.f, 16.f};
    const cocos2d::CCSize BADGE_SIZE_SMALL = BADGE_SIZE * 0.8f;

    void switchToScene(cocos2d::CCScene* layer);
    void switchToScene(cocos2d::CCLayer* layer);

    void replaceScene(cocos2d::CCScene* layer);
    void replaceScene(cocos2d::CCLayer* layer);

    void prepareLayer(cocos2d::CCLayer* layer, bool bg = true, cocos2d::ccColor3B color = {0, 102, 255});
    void addBackground(cocos2d::CCNode* layer, cocos2d::ccColor3B color = {0, 102, 255});
    void addBackButton(cocos2d::CCMenu* menu, std::function<void()> callback);
    void navigateBack();

    // set the scale for `node` in a way that `node->getScaledContentSize()` will match `target->getScaledContentSize()`
    void rescaleToMatch(cocos2d::CCNode* node, cocos2d::CCNode* target, bool stretch = false);

    // set the scale for `node` in a way that `node->getScaledContentSize()` will match `targetSize`
    void rescaleToMatch(cocos2d::CCNode* node, cocos2d::CCSize targetSize, bool stretch = false);

    void rescaleToMatchX(cocos2d::CCNode* node, float targetWidth);

    float getScrollPos(BoomListView* listView);
    void setScrollPos(BoomListView* listView, float pos);

    // scrolls to the bottom of the list view
    void scrollToBottom(BoomListView* listView);
    // scrolls to the bottom of the scroll layer
    void scrollToBottom(geode::ScrollLayer* listView);

    // scrolls to the top of the list view
    void scrollToTop(BoomListView* listView);
    // scrolls to the top of the scroll layer
    void scrollToTop(geode::ScrollLayer* listView);

    // small wrapper with precalculated sizes to make ui easier
    struct PopupLayout {
        cocos2d::CCSize winSize, popupSize;
        float left, right, bottom, top;

        cocos2d::CCSize center, centerLeft, centerTop, centerRight, centerBottom;
        cocos2d::CCSize bottomLeft, topLeft, bottomRight, topRight;

        cocos2d::CCPoint fromTop(float y);
        cocos2d::CCPoint fromTop(cocos2d::CCSize off);

        cocos2d::CCPoint fromBottom(float y);
        cocos2d::CCPoint fromBottom(cocos2d::CCSize off);
    };

    PopupLayout getPopupLayout(const cocos2d::CCSize& popupSize);
    PopupLayout getPopupLayoutAnchored(const cocos2d::CCSize& popupSize);

    cocos2d::CCNode* findChildByMenuSelectorRecursive(cocos2d::CCNode* node, uintptr_t function);

    cocos2d::CCSprite* createBadge(const std::string& sprite);
    cocos2d::CCSprite* createBadgeIfSpecial(const SpecialUserData& data);
    cocos2d::CCSprite* createBadgeIfSpecial(const UserEntry& data);
    void addBadgesToMenu(const std::vector<std::string>& roleVector, cocos2d::CCNode* menu, int z = 0, cocos2d::CCSize size = BADGE_SIZE);
    // cocos2d::CCMenu* createBadgeMenuIfSpecial(const SpecialUserData& data);
    // cocos2d::CCMenu* createBadgeMenuIfSpecial(const UserEntry& data);
    cocos2d::ccColor3B getNameColor(const SpecialUserData& data);
    RichColor getNameRichColor(const SpecialUserData& data);

    void makeListGray(GJListLayer* list);
    void setCellColors(cocos2d::CCArray* cells, cocos2d::ccColor3B color);

    enum class RepeatMode {
        X, Y, Both
    };

    class RepeatingBackground : public cocos2d::CCSprite {
        void update(float dt) override;

    public:
        static RepeatingBackground* create(const char* name);
    };

    // make a background that loops
    cocos2d::CCSprite* makeRepeatingBackground(const char* texture, cocos2d::ccColor3B color, float speed = 1.0f, float scale = 1.f, RepeatMode mode = RepeatMode::X, const cocos2d::CCSize& visibleSize = {0.f, 0.f});

    // cap popup width so it doesnt become too big on small aspect ratios (max winSize.width * 0.8)
    float capPopupWidth(float in);

    // i hate gjcommentlistlayer i hate gjcommentlistlayer i hate gjcommentlistlayer
    void fixListBorders(GJCommentListLayer* list);

    float getAspectRatio();
}
