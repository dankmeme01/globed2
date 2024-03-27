#include "ui.hpp"

#include <hooks/game_manager.hpp>
#include <managers/settings.hpp>
#include <util/format.hpp>
#include <util/misc.hpp>
#include <util/time.hpp>
#include <util/math.hpp>

using namespace geode::prelude;

namespace util::ui {
    void switchToScene(CCLayer* layer) {
        // massive geode L
        // uses replaceScene instead of pushScene
        auto scene = CCScene::create();
        scene->addChild(layer);
        CCDirector::get()->pushScene(CCTransitionFade::create(.5f, scene));
    }

    void prepareLayer(CCLayer* layer) {
        util::ui::addBackground(layer);

        auto menu = CCMenu::create();
        layer->addChild(menu);

        util::ui::addBackButton(menu, util::ui::navigateBack);

        layer->setKeyboardEnabled(true);
        layer->setKeypadEnabled(true);
    }

    void addBackground(CCNode* layer) {
        auto windowSize = CCDirector::get()->getWinSize();

        auto bg = CCSprite::create("GJ_gradientBG.png");
        auto bgSize = bg->getTextureRect().size;

        Build<CCSprite>(bg)
            .anchorPoint({0.f, 0.f})
            .scaleX((windowSize.width + 10.f) / bgSize.width)
            .scaleY((windowSize.height + 10.f) / bgSize.height)
            .pos({-5.f, -5.f})
            .color({0, 102, 255})
            .zOrder(-1)
            .parent(layer);
    }

    void addBackButton(CCMenu* menu, std::function<void()> callback) {
        auto windowSize = CCDirector::get()->getWinSize();
        Build<CCSprite>::createSpriteName("GJ_arrow_01_001.png")
            .intoMenuItem([callback](CCObject*) {
                callback();
            })
            .pos(-windowSize.width / 2 + 25.0f, windowSize.height / 2 - 25.0f)
            .parent(menu);
    }

    void navigateBack() {
        auto director = CCDirector::get();
        director->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
    }

    void rescaleToMatch(cocos2d::CCNode* node, cocos2d::CCNode* target, bool stretch) {
        auto targetSize = target->getScaledContentSize();
        rescaleToMatch(node, targetSize, stretch);
    }

    void rescaleToMatch(cocos2d::CCNode *node, cocos2d::CCSize targetSize, bool stretch) {
        auto nodeSize = node->getContentSize();

        if (!stretch) {
            float scale = targetSize.width / nodeSize.width;
            node->setScale(scale);
        } else {
            node->setScaleX(targetSize.width / nodeSize.width);
            node->setScaleY(targetSize.height / nodeSize.height);
        }
    }

    float getScrollPos(BoomListView* listView) {
        auto* cl = listView->m_tableView->m_contentLayer;
        if (cl->getPositionY() > 0.f) return 99999.f;
        return cl->getScaledContentSize().height + cl->getPositionY();
    }

    void setScrollPos(BoomListView* listView, float pos) {
        if (util::math::equal(pos, 99999.f)) return;

        auto* cl = listView->m_tableView->m_contentLayer;
        float actualPos = pos - cl->getScaledContentSize().height;

        cl->setPositionY(std::min(actualPos, 0.f));
    }

    void tryLoadDeathEffect(int id) {
        if (id <= 1) return;

        auto textureCache = CCTextureCache::sharedTextureCache();
        auto sfCache  = CCSpriteFrameCache::sharedSpriteFrameCache();

        auto pngKey = fmt::format("PlayerExplosion_{:02}.png", id - 1);
        auto plistKey = fmt::format("PlayerExplosion_{:02}.plist", id - 1);

        if (textureCache->textureForKey(pngKey.c_str()) == nullptr) {
            textureCache->addImage(pngKey.c_str(), false);
            sfCache->addSpriteFramesWithFile(plistKey.c_str());
        }
    }

    PopupLayout getPopupLayout(CCSize popupSize) {
        PopupLayout layout;

        layout.winSize = CCDirector::get()->getWinSize();
        layout.popupSize = popupSize;
        layout.center = CCSize{layout.winSize.width / 2, layout.winSize.height / 2};

        layout.left = layout.center.width - popupSize.width / 2;
        layout.bottom = layout.center.height - popupSize.height / 2;
        layout.right = layout.center.width + popupSize.width / 2;
        layout.top = layout.center.height + popupSize.height / 2;

        layout.centerLeft = CCSize{layout.left, layout.center.height};
        layout.centerRight = CCSize{layout.right, layout.center.height};
        layout.centerTop = CCSize{layout.center.width, layout.top};
        layout.centerBottom = CCSize{layout.center.width, layout.bottom};

        layout.bottomLeft = CCSize{layout.left, layout.bottom};
        layout.topLeft = CCSize{layout.left, layout.top};
        layout.bottomRight = CCSize{layout.right, layout.bottom};
        layout.topRight = CCSize{layout.right, layout.top};

        return layout;
    }

    CCNode* findChildByMenuSelectorRecursive(CCNode* node, uintptr_t function) {
        if (auto button = typeinfo_cast<CCMenuItem*>(node)) {
            uintptr_t btnAddr = *(uintptr_t*)&button->m_pfnSelector;
            if (function == btnAddr) {
                return node;
            }
        }

        if (!node->getChildren() || node->getChildrenCount() == 0) return nullptr;

        for (CCNode* child : CCArrayExt<CCNode*>(node->getChildren())) {
            auto result = findChildByMenuSelectorRecursive(child, function);
            if (result) {
                return result;
            }
        }

        return nullptr;
    }

    uibuilder::Build<cocos2d::CCSprite> createBadge(std::string badgePNG, cocos2d::CCPoint pos, std::string id) {
        log::info("creating badge");

        auto badgeSprite = Build<cocos2d::CCSprite>::createSpriteName(badgePNG.c_str())
            .scale(1.f)
            .pos(pos)
            .id(id);

        return badgeSprite.collect();
    }

    CCSprite* createBadgeIfSpecial(ccColor3B color, cocos2d::CCPoint pos) {
        if (color == ccc3(15, 239, 195)) return createBadge("role-mod.png"_spr, pos, "globed-mod-badge").collect();
        if (color == ccc3(233, 30, 99)) return createBadge("role-admin.png"_spr, pos, "globed-admin-badge").collect();
        if (color == ccc3(52, 152, 219)) return createBadge("role-helper.png"_spr, pos, "globed-helper-badge").collect();
        if (color == ccc3(154, 88, 255)) return createBadge("role-supporter.png"_spr, pos, "globed-supporter-badge").collect();
        if (color == ccc3(248, 0, 255)) return createBadge("role-booster.png"_spr, pos, "globed-booster-badge").collect();

        return nullptr;
    }
}