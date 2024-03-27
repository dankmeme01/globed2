#include "player_list_cell.hpp"

#include "room_popup.hpp"
#include "download_level_popup.hpp"
#include <hooks/gjgamelevel.hpp>
#include <util/ui.hpp>
#include "hooks/level_select_layer.hpp"

using namespace geode::prelude;
using namespace util::ui;

bool PlayerListCell::init(const PlayerRoomPreviewAccountData& data) {
    if (!CCLayer::init()) return false;
    this->data = data;

    auto* gm = GameManager::get();

    Build<SimplePlayer>::create(data.cube)
        .scale(0.65f)
        .playerFrame(data.cube, IconType::Cube)
        .color(gm->colorForIdx(data.color1))
        .secondColor(gm->colorForIdx(data.color2))
        .parent(this)
        .pos(25.f, CELL_HEIGHT / 2.f)
        .id("player-icon"_spr)
        .store(simplePlayer);

    if (data.glowColor == -1) {
        simplePlayer->disableGlowOutline();
    } else {
        simplePlayer->setGlowOutline(gm->colorForIdx(data.glowColor));
    }

    // name label

    ccColor3B nameColor = ccc3(255, 255, 255);
    if (data.specialUserData) {
        nameColor = data.specialUserData->nameColor;
    }

    CCMenu* badgeWrapper = Build<CCMenu>::create()
        .pos(simplePlayer->getPositionX() + simplePlayer->m_firstLayer->getScaledContentSize().width / 2 + 10.f, CELL_HEIGHT / 2)
        .layout(RowLayout::create()->setGap(5.f)->setAxisAlignment(AxisAlignment::Start))
        .anchorPoint(0.f, 0.5f)
        .contentSize(RoomPopup::LIST_WIDTH, CELL_HEIGHT)
        .scale(1.f)
        .parent(this)
        .id("badge-wrapper"_spr)
        .collect();

    auto* label = Build<CCLabelBMFont>::create(data.name.c_str(), "bigFont.fnt")
        .color(nameColor)
        .limitLabelWidth(170.f, 0.6f, 0.1f)
        .collect();

    label->setScale(label->getScale() * 0.9f);

    auto* btn = Build<CCMenuItemSpriteExtra>::create(label, this, menu_selector(PlayerListCell::onOpenProfile))
        .pos(simplePlayer->getPositionX() + label->getScaledContentSize().width / 2.f + 25.f, CELL_HEIGHT / 2.f)
        .parent(badgeWrapper)
        .collect();
    btn->m_scaleMultiplier = 1.1f;

    if (data.specialUserData.has_value()) {
        auto badge = createBadgeIfSpecial(nameColor, CCPoint{0.f, 0.f});
        if (badge != nullptr) badgeWrapper->addChild(badge);
    }
    badgeWrapper->updateLayout();

    if (this->data.levelId != 0) {
        Build<CCSprite>::createSpriteName("GJ_playBtn2_001.png")
            .scale(0.30f)
            .intoMenuItem([levelId = this->data.levelId](auto) {
                auto* glm = GameLevelManager::sharedState();
                auto mlevel = glm->m_mainLevels->objectForKey(std::to_string(levelId));
                bool isMainLevel = std::find(HookedLevelSelectLayer::MAIN_LEVELS.begin(), HookedLevelSelectLayer::MAIN_LEVELS.end(), levelId) != HookedLevelSelectLayer::MAIN_LEVELS.end();

                if (mlevel != nullptr) {
                    if (isMainLevel) { //if its a classic main level go to that page in LevelSelectLayer
                        auto lsl = LevelSelectLayer::create(levelId - 1);
                        util::ui::switchToScene(lsl);
                        return;
                    } //otherwise we just go right to playlayer
                    auto level = static_cast<HookedGJGameLevel*>(glm->getMainLevel(levelId, false));
                    level->m_fields->shouldTransitionWithPopScene = true;
                    auto pl = PlayLayer::create(level, false, false);
                    util::ui::switchToScene(pl);
                    return;
                }

                if (auto popup = DownloadLevelPopup::create(levelId)) {
                    popup->show();
                }
            })
            .pos(RoomPopup::LIST_WIDTH - 30.f, CELL_HEIGHT / 2.f)
            .store(playButton)
            .intoNewParent(CCMenu::create())
            .pos(0.f, 0.f)
            .parent(this);
        playButton->m_scaleMultiplier = 1.1f;
    }

    return true;
}

void PlayerListCell::onOpenProfile(cocos2d::CCObject*) {
    GameLevelManager::sharedState()->storeUserName(data.userId, data.accountId, data.name);
    ProfilePage::create(data.accountId, false)->show();
}

PlayerListCell* PlayerListCell::create(const PlayerRoomPreviewAccountData& data) {
    auto ret = new PlayerListCell;
    if (ret->init(data)) {
        return ret;
    }

    delete ret;
    return nullptr;
}