#include "user_cell.hpp"

#include "userlist.hpp"
#include "actions_popup.hpp"
#include "ui/menu/room/room_popup.hpp"
#include <audio/voice_playback_manager.hpp>
#include <data/packets/client/admin.hpp>
#include <data/packets/server/admin.hpp>
#include <managers/block_list.hpp>
#include <managers/settings.hpp>
#include <managers/profile_cache.hpp>
#include <hooks/play_layer.hpp>
#include <hooks/gjgamelevel.hpp>
#include <ui/menu/admin/user_popup.hpp>
#include <ui/general/ask_input_popup.hpp>
#include <ui/general/intermediary_loading_popup.hpp>
#include <util/format.hpp>
#include <util/ui.hpp>

using namespace geode::prelude;
using namespace util::ui;

bool GlobedUserCell::init(const PlayerStore::Entry& entry, const PlayerAccountData& data) {
    if (!CCLayer::init()) return false;

    accountData = data;

    auto winSize = CCDirector::get()->getWinSize();

    auto gm = GameManager::get();

    auto sp = Build<SimplePlayer>::create(data.icons.cube)
        .scale(0.6f)
        .playerFrame(data.icons.cube, IconType::Cube)
        .color(gm->colorForIdx(data.icons.color1))
        .secondColor(gm->colorForIdx(data.icons.color2))
        .parent(this)
        .pos(18.f, CELL_HEIGHT / 2.f)
        .id("player-icon"_spr)
        .collect();

    if (data.icons.glowColor != -1) {
        sp->setGlowOutline(gm->colorForIdx(data.icons.glowColor));
    } else {
        sp->disableGlowOutline();
    }

    Build<CCMenu>::create()
        .pos(0.f, 0.f)
        .parent(this)
        .store(menu);

    auto& pcm = ProfileCacheManager::get();
    ccColor3B nameColor = ccc3(255, 255, 255);
    if (data.specialUserData.has_value()) {
        nameColor = data.specialUserData->nameColor;
    }

    badgeWrapper = Build<CCMenu>::create()
        .pos(sp->getPositionX() + sp->m_firstLayer->getScaledContentSize().width / 2 + 10.f, CELL_HEIGHT / 2)
        .layout(RowLayout::create()->setGap(5.f)->setAxisAlignment(AxisAlignment::Start)->setAutoScale(false))
        .anchorPoint(0.f, 0.5f)
        .contentSize(RoomPopup::LIST_WIDTH / 2, CELL_HEIGHT / 2)
        .scale(1.f)
        .parent(this)
        .id("badge-wrapper"_spr)
        .collect();
    
    auto* nameLabel = Build<CCLabelBMFont>::create(data.name.data(), "bigFont.fnt")
        .color(nameColor)
        .limitLabelWidth(125.f, 0.5f, 0.1f)
        .collect();

    auto* nameButton = Build<CCMenuItemSpriteExtra>::create(nameLabel, this, menu_selector(GlobedUserCell::onOpenProfile))
        // thank god no more
        .pos(nameLabel->getPosition())
        .parent(badgeWrapper)
        .collect();
    nameButton->m_scaleMultiplier = 1.1f;

    if (data.specialUserData.has_value()) {
        auto badge = createBadgeIfSpecial(nameColor, badgeWrapper->getPosition());
        if (badge != nullptr) badgeWrapper->addChild(badge);
    }

    // percentage label
    Build<CCLabelBMFont>::create("", "goldFont.fnt")
        .pos(nameButton->getPosition() + nameButton->getScaledContentSize() / 2.f + CCPoint{20.f, 0.f})
        .anchorPoint({0.f, 0.5f})
        .scale(0.4f)
        .parent(badgeWrapper)
        .id("percentage-label"_spr)
        .store(percentageLabel);

    badgeWrapper->updateLayout();

    this->makeButtons();

    this->refreshData(entry);
    this->schedule(schedule_selector(GlobedUserCell::updateVisualizer), 1.f / 60.f);

    return true;
}

void GlobedUserCell::refreshData(const PlayerStore::Entry& entry) {
    if (_data != entry) {
        _data = entry;

        bool platformer = PlayLayer::get()->m_level->isPlatformer();
        if (platformer && _data.localBest != 0) {
            percentageLabel->setString(util::format::formatPlatformerTime(_data.localBest).c_str());
        } else if (!platformer) {
            percentageLabel->setString(fmt::format("{}%", _data.localBest).c_str());
        }
    }
}

void GlobedUserCell::updateVisualizer(float dt) {
#ifdef GLOBED_VOICE_SUPPORT
    if (audioVisualizer) {
        auto& vpm = VoicePlaybackManager::get();

        float loudness = vpm.getLoudness(accountData.accountId);
        audioVisualizer->setVolume(loudness);
    }
#endif // GLOBED_VOICE_SUPPORT
}

void GlobedUserCell::makeButtons() {
    if (buttonsWrapper) buttonsWrapper->removeFromParent();
    const float gap = 5.f;

    Build<CCMenu>::create()
        .anchorPoint(1.0f, 0.5f)
        .pos(GlobedUserListPopup::LIST_WIDTH - 7.f, CELL_HEIGHT / 2.f)
        .layout(RowLayout::create()
                    ->setGap(gap)
                    ->setAutoScale(false)
                    ->setAxisReverse(true))
        .parent(menu)
        .store(buttonsWrapper);

    auto& settings = GlobedSettings::get();

    auto maxWidth = -5.f;

    bool notSelf = accountData.accountId != GJAccountManager::get()->m_accountID;
    bool createBtnSettings = notSelf;
    bool createBtnAdmin = NetworkManager::get().isAuthorizedAdmin();
    bool createBtnTp = createBtnAdmin && notSelf;
    bool createVisualizer = settings.communication.voiceEnabled && notSelf;
    bool createSettingsAlts = false;

    if (!createBtnAdmin && createBtnSettings) {
        createSettingsAlts = true;
        createBtnSettings = false;
    }

    auto pl = static_cast<GlobedPlayLayer*>(PlayLayer::get());

    if (!pl->m_fields->players.contains(accountData.accountId)) return;

    if (createBtnSettings) {
        // settings button
        Build<CCSprite>::createSpriteName("GJ_optionsBtn_001.png")
            .scale(0.36f)
            .intoMenuItem([this, id = accountData.accountId](auto) {
                auto* pl = static_cast<GlobedPlayLayer*>(PlayLayer::get());

                // if they left the level, do nothing
                if (!pl->m_fields->players.contains(accountData.accountId)) return;

                GlobedUserActionsPopup::create(id)->show();
            })
            .parent(buttonsWrapper)
            .id("player-actions-button"_spr)
            .store(actionsButton);

        actionsButton->m_scaleMultiplier = 1.2f;
        maxWidth += actionsButton->getScaledContentSize().width + gap;
    } else if (createSettingsAlts) {
        bool isUnblocked = pl->shouldLetMessageThrough(accountData.accountId);
        bool isHidden = pl->m_fields->players.at(accountData.accountId)->getForciblyHidden();

        Build<CCSprite>::createSpriteName(isUnblocked ? "icon-mute.png"_spr : "icon-unmute.png"_spr)
            .scale(0.45f)
            .intoMenuItem([this, isUnblocked, accountId = accountData.accountId](auto) {
                auto& bl = BlockListManager::get();
                isUnblocked ? bl.blacklist(accountId) : bl.whitelist(accountId);
                // mute them immediately
                auto& settings = GlobedSettings::get();
                auto& vpm = VoicePlaybackManager::get();

                if (isUnblocked) {
                    vpm.setVolume(accountId, 0.f);
                } else {
                    vpm.setVolume(accountId, settings.communication.voiceVolume);
                }

                // delay by 1 frame to prevent epic ccmenuitem::activate crash
                Loader::get()->queueInMainThread([this] {
                    this->makeButtons();
                });
            })
            .parent(buttonsWrapper)
            .id("player-mute-button"_spr)
            .store(muteButton);

        muteButton->m_scaleMultiplier = 1.2f;
        maxWidth += muteButton->getScaledContentSize().width + gap;

        auto spr = Build<CCSprite>::createSpriteName(isHidden ? "icon-show-player.png"_spr : "icon-hide-player.png"_spr).collect();
        util::ui::rescaleToMatch(spr, muteButton);

        Build(spr)
            .intoMenuItem([this, isHidden, pl, accountId = accountData.accountId](auto) {
                if (!pl->m_fields->players.contains(accountId)) return;

                pl->m_fields->players.at(accountId)->setForciblyHidden(!isHidden);
                auto& bl = BlockListManager::get();
                bl.setHidden(accountId, !isHidden);

                Loader::get()->queueInMainThread([this] {
                    this->makeButtons();
                });
            })
            .parent(buttonsWrapper)
            .id("player-hide-button"_spr)
            .store(hideButton);

        hideButton->m_scaleMultiplier = 1.2f;
        maxWidth += hideButton->getScaledContentSize().width + gap;
    }

    // admin menu button
    if (createBtnAdmin) {
        Build<CCSprite>::createSpriteName("GJ_reportBtn_001.png")
            .scale(0.4f)
            .intoMenuItem([this](auto) {
                // load the data from the server
                auto& nm = NetworkManager::get();
                IntermediaryLoadingPopup::create([&nm, this](auto popup) {
                    nm.send(AdminGetUserStatePacket::create(std::to_string(accountData.accountId)));
                    nm.addListener<AdminUserDataPacket>([this, popup = popup](auto packet) {
                        popup->onClose(popup);

                        // delay the cration to avoid deadlock
                        Loader::get()->queueInMainThread([userEntry = packet->userEntry, accountData = accountData.makeRoomPreview(0)] {
                            AdminUserPopup::create(userEntry, accountData)->show();
                        });
                    });
                }, [&nm, this](auto) {
                    nm.removeListenerDelayed<AdminUserDataPacket>(util::time::seconds(3));
                })->show();
            })
            .parent(buttonsWrapper)
            .id("kick-button"_spr)
            .store(kickButton);

        maxWidth += kickButton->getScaledContentSize().width + gap;
    }

    if (createBtnTp) {
        GlobedPlayLayer* gpl = static_cast<GlobedPlayLayer*>(PlayLayer::get()); // ggpl

        Build<CCSprite>::createSpriteName("icon-teleport.png"_spr)
            .scale(0.35f)
            .intoMenuItem([gpl, this](auto) {
                auto& settings = GlobedSettings::get();
                if (!settings.flags.seenTeleportNotice)  {
                    settings.flags.seenTeleportNotice = true;
                    settings.save();

                    FLAlertLayer::create(
                        "Note",
                        "Teleporting to a player will <cr>disable level progress</c> until you <cy>fully reset</c> the level.",
                        "Ok"
                    )->show();

                    return;
                }

                gpl->toggleSafeMode(true);

                PlayerObject* po1 = gpl->m_player1;
                CCPoint position = {0,0}; // just in case the player has already left by the time we teleport
                if (gpl->m_fields->interpolator->hasPlayer(accountData.accountId)) position = gpl->m_fields->interpolator->getPlayerState(accountData.accountId).player1.position;

                po1->m_position = position;
            })
            .parent(buttonsWrapper)
            .id("teleport-button"_spr)
            .store(teleportButton);

        maxWidth += teleportButton->getScaledContentSize().width + gap;
    }

    if (createVisualizer) {
        // audio visualizer
        Build<GlobedAudioVisualizer>::create()
#ifndef GLOBED_VOICE_SUPPORT
            .visible(false)
#endif // GLOBED_VOICE_SUPPORT
            .parent(buttonsWrapper)
            .id("audio-visualizer"_spr)
            .store(audioVisualizer);

        audioVisualizer->setScaleX(0.5f);

        maxWidth += audioVisualizer->getScaledContentSize().width + gap;
    }

    buttonsWrapper->setContentSize({maxWidth, 20.f});

    buttonsWrapper->updateLayout();
    
    badgeWrapper->updateLayout();
}

void GlobedUserCell::onOpenProfile(CCObject*) {
    bool myself = accountData.accountId == GJAccountManager::get()->m_accountID;
    if (!myself) {
        GameLevelManager::sharedState()->storeUserName(accountData.userId, accountData.accountId, accountData.name);
    }

    ProfilePage::create(accountData.accountId, myself)->show();
}

GlobedUserCell* GlobedUserCell::create(const PlayerStore::Entry& entry, const PlayerAccountData& data) {
    auto ret = new GlobedUserCell;
    if (ret->init(entry, data)) {
        return ret;
    }

    delete ret;
    return nullptr;
}