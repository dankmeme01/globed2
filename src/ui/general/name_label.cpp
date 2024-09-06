#include "name_label.hpp"

#include <util/ui.hpp>
#include <managers/role.hpp>

using namespace geode::prelude;

bool GlobedNameLabel::init(const std::string& name, const std::vector<std::string>& badges, const RichColor& nameColor, bool bigFont) {
    if (!CCNode::init()) return false;

    this->bigFont = bigFont;

    this->setAnchorPoint({0.5f, 0.5f});
    this->setLayout(RowLayout::create()->setGap(4.f)->setAutoScale(false));
    this->setContentWidth(300.f);
    this->updateData(name, badges, nameColor);

    return true;
}

void GlobedNameLabel::updateData(const std::string& name, const std::vector<std::string>& badges, const RichColor& nameColor) {
    this->updateName(name);
    this->updateBadges(badges);
    this->updateColor(nameColor);
}

void GlobedNameLabel::updateData(const std::string& name, const SpecialUserData& sud) {
    std::vector<std::string> badgeList;
    if (sud.roles) {
        badgeList = RoleManager::get().getBadgeList(sud.roles.value());
    }
    this->updateData(name, badgeList, util::ui::getNameRichColor(sud));
}

void GlobedNameLabel::updateBadge(cocos2d::CCSprite* badge) {
    if (badgeContainer) badgeContainer->removeFromParent();
    badgeContainer = nullptr;

    if (!badge) return;

    util::ui::rescaleToMatch(badge, util::ui::BADGE_SIZE);

    Build<CCNode>::create()
        .parent(this)
        .store(badgeContainer)
        .child(badge);

    badge->setPosition(badge->getScaledContentSize() / 2.f);
    badgeContainer->setContentSize(badge->getScaledContentSize());

    this->updateLayout();
}

void GlobedNameLabel::updateBadges(const std::vector<std::string>& badges) {
    if (badgeContainer) badgeContainer->removeFromParent();
    badgeContainer = nullptr;

    if (badges.empty()) return;

    size_t badgeCount = multipleBadge ? badges.size() : 1;

    Build<CCNode>::create()
        .contentSize(5.f * (badgeCount - 1) + badgeCount * util::ui::BADGE_SIZE.width, 0.f)
        .layout(RowLayout::create()->setGap(5.f))
        .parent(this)
        .store(badgeContainer);

    if (multipleBadge) {
        util::ui::addBadgesToMenu(badges, badgeContainer, 1);
    } else {
        util::ui::addBadgesToMenu(std::vector<std::string>({badges[0]}), badgeContainer, 1);
    }

    this->updateLayout();
}

void GlobedNameLabel::updateName(const std::string& name) {
    this->updateName(name.c_str());
}

void GlobedNameLabel::updateName(const char* name) {
    if (!label) {
        Build<CCNode>::create()
            .parent(this)
            .zOrder(-2)
            .store(labelContainer);

        Build<CCLabelBMFont>::create("", bigFont ? "bigFont.fnt" : "chatFont.fnt")
            .zOrder(-1)
            .anchorPoint(0.f, 0.f)
            .parent(labelContainer)
            .store(label);

        Build<CCLabelBMFont>::create("", bigFont ? "bigFont.fnt" : "chatFont.fnt")
            .zOrder(-2)
            .color(0, 0, 0)
            .anchorPoint(0.f, 0.0f)
            .pos(0.75f, -0.75f)
            .parent(labelContainer)
            .opacity(label->getOpacity() * 0.75f)
            .store(labelShadow);
    }

    label->setString(name);
    labelShadow->setString(name);
    labelContainer->setScaledContentSize(label->getScaledContentSize());
    this->updateLayout();
}

void GlobedNameLabel::updateOpacity(float opacity) {
    this->updateOpacity(static_cast<unsigned char>(opacity * 255.f));
}

void GlobedNameLabel::updateOpacity(unsigned char opacity) {
    if (label) label->setOpacity(opacity);

    if (badgeContainer) {
        for (auto child : CCArrayExt<CCSprite*>(badgeContainer->getChildren())) {
            child->setOpacity(opacity);
        }
    }

    if (labelShadow) labelShadow->setOpacity(opacity * 0.75);
}

void GlobedNameLabel::updateColor(const RichColor& color) {
    if (!label) return;

    color.animateLabel(label);
}


void GlobedNameLabel::setMultipleBadgeMode(bool state) {
    this->multipleBadge = state;

    if (this->multipleBadge) return;

    // if (this->badgeContainer) {
    //     bool first = true;
    //     for (auto child : CCArrayExt<CCNode*>(this->badgeContainer->getChildren())) {
    //         log::debug("child: {}, first: {}", child, first);
    //         if (first) {
    //             first = false; continue;
    //         }

    //         child->removeFromParent();
    //     }
    // }

    // badgeContainer->setContentWidth(util::ui::BADGE_SIZE.width);
    // badgeContainer->updateLayout();
    // this->updateLayout();
}

GlobedNameLabel* GlobedNameLabel::create(const std::string& name, const std::vector<std::string>& roles, const RichColor& color, bool bigFont) {
    auto ret = new GlobedNameLabel;
    if (ret->init(name, roles, color, bigFont)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

GlobedNameLabel* GlobedNameLabel::create(const std::string& name, cocos2d::CCSprite* badge, const RichColor& color, bool bigFont) {
    auto ret = new GlobedNameLabel;
    if (ret->init(name, {}, color, bigFont)) {
        ret->autorelease();
        ret->updateBadge(badge);
        return ret;
    }

    delete ret;
    return nullptr;
}

GlobedNameLabel* GlobedNameLabel::create(const std::string& name, const SpecialUserData& sud, bool bigFont) {
    std::vector<std::string> badgeList;
    if (sud.roles) {
        badgeList = RoleManager::get().getBadgeList(sud.roles.value());
    }

    return create(name, badgeList, util::ui::getNameRichColor(sud), bigFont);
}

GlobedNameLabel* GlobedNameLabel::create(const std::string& name, bool bigFont) {
    return create(name, std::vector<std::string>{}, RichColor({255, 255, 255}), bigFont);
}
