#include "level_info_layer.hpp"

#include <hooks/game_manager.hpp>
#include <hooks/gjbasegamelayer.hpp>
#include <managers/daily_manager.hpp>
#include <managers/admin.hpp>
#include <managers/room.hpp>
#include <managers/settings.hpp>
#include <data/packets/client/room.hpp>
#include <data/packets/client/general.hpp>
#include <data/packets/server/general.hpp>
#include <net/manager.hpp>
#include <ui/menu/level_list/level_list_layer.hpp>
#include <ui/menu/featured/edit_featured_level_popup.hpp>
#include <ui/menu/featured/featured_list_layer.hpp>
#include <util/gd.hpp>

using namespace geode::prelude;

static int& storedRateTier() {
    return static_cast<HookedGameManager*>(GameManager::get())->m_fields->lastLevelRateTier;
}

bool HookedLevelInfoLayer::init(GJGameLevel* level, bool challenge) {
    if (!LevelInfoLayer::init(level, challenge)) return false;

    auto& am = AdminManager::get();
    if (am.authorized() && am.getRole().canModerate()) {
        this->addLevelSendButton();
    }

    auto& rm = RoomManager::get();
    if (rm.isInRoom() && rm.isOwner()) {
        this->addRoomLevelButton();
    }

    std::vector<LevelId> levelIds;
    levelIds.push_back(level->m_levelID);

    auto& nm = NetworkManager::get();
    if (nm.established() && GlobedSettings::get().globed.playerCountOnLvlPage) {
        this->addPlayerCountLabel();
        nm.send(RequestPlayerCountPacket::create(std::move(levelIds)));
        nm.addListener<LevelPlayerCountPacket>(this, [this](std::shared_ptr<LevelPlayerCountPacket> packet) {
            auto [levelId, playerCount] = packet->levels[0];
            this->updatePlayerCount(playerCount);
        });
        this->schedule(schedule_selector(HookedLevelInfoLayer::scheduledPlayerCountUpdate), 5.0f);
    }

    // i hate myself
    if (level->m_levelIndex == 0) {
        for (auto child : CCArrayExt<CCNode*>(CCScene::get()->getChildren())) {
            if (typeinfo_cast<GlobedLevelListLayer*>(child) || typeinfo_cast<GlobedFeaturedListLayer*>(child)) {
                util::gd::reorderDownloadedLevel(level);
                break;
            }
        }
    }

    int rating = DailyManager::get().rateTierOpen;

    bool isQuittingPlayLayer = false;
    if (auto pl = GlobedGJBGL::get()) {
        isQuittingPlayLayer = pl->m_fields->quitting;
    }

    if (rating == -1 && storedRateTier() != -1 && (!PlayLayer::get() || isQuittingPlayLayer)) {
        rating = storedRateTier();
        storedRateTier() = -1;
    }

    if (rating != -1) {
        if (auto* diff = DailyManager::get().findDifficultySprite(this)) {
            DailyManager::get().attachRatingSprite(rating, diff);
        }

        this->m_fields->rateTier = rating;
    }

    return true;
}

void HookedLevelInfoLayer::onPlay(CCObject* s) {
    if (m_fields->allowOpeningAnyway) {
        m_fields->allowOpeningAnyway = false;
        storedRateTier() = m_fields->rateTier;
        LevelInfoLayer::onPlay(s);
        return;
    }

    // if we are already in a playlayer, don't allow sillyness
    if (NetworkManager::get().established() && GJBaseGameLayer::get() != nullptr) {
        geode::createQuickPopup("Warning", "You are already inside of a level, opening another level may cause Globed to crash. <cy>Do you still want to proceed?</c>", "Cancel", "Play", [this, s](auto, bool play) {
            if (play) {
                this->forcePlay(s);
            }
        });
        return;
    }

    storedRateTier() = m_fields->rateTier;
    LevelInfoLayer::onPlay(s);
}

void HookedLevelInfoLayer::forcePlay(CCObject* s) {
    m_fields->allowOpeningAnyway = true;

    storedRateTier() = m_fields->rateTier;
    LevelInfoLayer::onPlay(s);
}

void HookedLevelInfoLayer::tryCloneLevel(CCObject* s) {
    if (NetworkManager::get().established() && GJBaseGameLayer::get() != nullptr) {
        FLAlertLayer::create("Globed Error", "Cannot perform this action while in a level", "Ok")->show();
        return;
    }

    LevelInfoLayer::tryCloneLevel(s);
}

void HookedLevelInfoLayer::addLevelSendButton() {
    auto* leftMenu = typeinfo_cast<CCMenu*>(this->getChildByIDRecursive("left-side-menu"));
    if (!leftMenu) {
        return;
    }

    bool plat = this->m_level->isPlatformer();
    Build<CCSprite>::createSpriteName("icon-send-btn.png"_spr)
        .intoMenuItem([this, plat] {
            if (plat) {
                EditFeaturedLevelPopup::create(this->m_level)->show();
            } else {
                FLAlertLayer::create("Error", "Only <cj>Platformer levels</c> are eligible to be <cg>Globed Featured!</c>", "Ok")->show();
            }
        })
        .with([&](auto* btn) {
            if (!plat) {
                btn->setColor({100, 100, 100});
            }
        })
        .id("send-btn"_spr)
        .parent(leftMenu);

    leftMenu->updateLayout();
}

void HookedLevelInfoLayer::addRoomLevelButton() {
    auto rightMenu = this->getChildByIDRecursive("right-side-menu");
    if (!rightMenu) return;

    Build<CCSprite>::createSpriteName("GJ_shareBtn_001.png")
        .scale(0.625f)
        .intoMenuItem([this] {
            auto settings = RoomManager::get().getInfo().settings;
            settings.levelId = this->m_level->m_levelID.value();
            NetworkManager::get().send(UpdateRoomSettingsPacket::create(settings));
        })
        .id("share-room-btn"_spr)
        .parent(rightMenu);

    rightMenu->updateLayout();
}

void HookedLevelInfoLayer::addPlayerCountLabel() {
    auto creatorName = this->getChildByIDRecursive("creator-name");
    if (!creatorName) return;

    auto creatorYPos = creatorName->convertToWorldSpace(CCPointZero).y;

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    Build<CCLabelBMFont>::create("0 players", "goldFont.fnt")
        .pos(winSize.width / 2, creatorYPos - 8.f)
        .parent(this)
        .scale(0.5f)
        .id("player-count"_spr)
        .visible(false)
        .store(m_fields->playerCountLabel);
}

void HookedLevelInfoLayer::updatePlayerCount(int playerCount) {
    if (playerCount == 0) {
        m_fields->playerCountLabel->setVisible(false);
        return;
    }
    m_fields->playerCountLabel->setVisible(true);
    m_fields->playerCountLabel->setString(fmt::format(
        "{} {}",
        playerCount, playerCount == 1 ? "player" : "players"
    ).c_str());
}

void HookedLevelInfoLayer::scheduledPlayerCountUpdate(float) {
    std::vector<LevelId> levelIds;
    levelIds.push_back(m_level->m_levelID);

    auto& nm = NetworkManager::get();
    if (nm.established()) {
        nm.send(RequestPlayerCountPacket::create(std::move(levelIds)));
    }
}
