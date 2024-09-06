#pragma once
#include <defs/geode.hpp>
#include <data/types/gd.hpp>

/* Container for username and badge */
class GlobedNameLabel : public cocos2d::CCNode {
public:
    void updateData(const std::string& name, const std::vector<std::string>& badges, const RichColor& nameColor);
    void updateData(const std::string& name, const SpecialUserData& sud);
    void updateBadge(cocos2d::CCSprite* badge);
    void updateBadges(const std::vector<std::string>& badges);
    void updateName(const std::string& name);
    void updateName(const char* name);
    void updateOpacity(float opacity);
    void updateOpacity(unsigned char opacity);
    void updateColor(const RichColor& color);

    void setMultipleBadgeMode(bool state);

    static GlobedNameLabel* create(const std::string& name, const std::vector<std::string>&, const RichColor& color, bool bigFont = false);
    static GlobedNameLabel* create(const std::string& name, const SpecialUserData& sud, bool bigFont = false);
    // deprecated
    static GlobedNameLabel* create(const std::string& name, cocos2d::CCSprite* badge, const RichColor& color, bool bigFont = false);
    static GlobedNameLabel* create(const std::string& name, bool bigFont = false);

private:
    Ref<cocos2d::CCLabelBMFont> label;
    Ref<cocos2d::CCLabelBMFont> labelShadow;
    Ref<cocos2d::CCNode> labelContainer;
    Ref<cocos2d::CCNode> badgeContainer;
    bool bigFont, multipleBadge = false;
    constexpr static int COLOR_ACTION_TAG = 3498567;

    bool init(const std::string& name, const std::vector<std::string>& badges, const RichColor& nameColor, bool bigFont = false);
};