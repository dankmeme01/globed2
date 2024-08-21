#include "user_cell.hpp"

#include "userlist.hpp"
#include "actions_popup.hpp"
#include <audio/voice_playback_manager.hpp>
#include <managers/admin.hpp>
#include <managers/block_list.hpp>
#include <managers/settings.hpp>
#include <managers/profile_cache.hpp>
#include <managers/friend_list.hpp>
#include <managers/role.hpp>
#include <hooks/gjbasegamelayer.hpp>
#include <hooks/gjgamelevel.hpp>
#include <ui/general/ask_input_popup.hpp>
#include <util/format.hpp>
#include <util/ui.hpp>
#include <util/gd.hpp>

using namespace geode::prelude;

bool GlobedUserCell::init(const PlayerStore::Entry& entry, const PlayerAccountData& data, GlobedUserListPopup* parent) {
    if (!CCLayer::init()) return false;

    this->parent = parent;
    accountData = data;

    auto winSize = CCDirector::get()->getWinSize();

    auto gm = GameManager::get();
    auto& flm = FriendListManager::get();
    this->isFriend = flm.isFriend(data.accountId);

    Build<CCNode>::create()
        .pos(0.f, 0.f)
        .parent(this)
        .store(menu);

    Build<CCMenu>::create()
        .layout(RowLayout::create()->setAutoScale(false)->setAxisAlignment(AxisAlignment::Start))
        .anchorPoint(0.f, 0.5f)
        .pos(5.f, CELL_HEIGHT / 2.f)
        .contentSize(GlobedUserListPopup::LIST_WIDTH, CELL_HEIGHT)
        .parent(menu)
        .id("name-layout"_spr)
        .store(usernameLayout);

    auto sp = Build<GlobedSimplePlayer>::create(data.icons)
        .scale(0.6f)
        .parent(usernameLayout)
        .id("player-icon"_spr)
        .collect();

    auto& pcm = ProfileCacheManager::get();
    RichColor nameColor = util::ui::getNameRichColor(data.specialUserData);

    auto* nameButton = Build<CCLabelBMFont>::create(data.name.data(), "bigFont.fnt")
        .with([&nameColor](auto* label) {
            nameColor.animateLabel(label);
        })
        .limitLabelWidth(130.f, 0.45f, 0.1f)
        .intoMenuItem([this] {
            util::gd::openProfile(accountData.accountId, accountData.userId, accountData.name);
        })
        .scaleMult(1.1f)
        .parent(usernameLayout)
        .store(nameBtn)
        .collect();

    // CCSprite* badgeIcon = util::ui::createBadgeIfSpecial(data.specialUserData);
    // if (badgeIcon) {
    //     util::ui::rescaleToMatch(badgeIcon, util::ui::BADGE_SIZE);
    //     usernameLayout->addChild(badgeIcon);
    // }

    // badge with s
    if (data.specialUserData.roles) {
        std::vector<std::string> badgeVector = RoleManager::get().getBadgeList(data.specialUserData.roles.value());
        util::ui::addBadgesToMenu(badgeVector, usernameLayout, 1);
    }

    if (isFriend) {
        // cell gradient
        auto* cellGradient = Build<CCSprite>::createSpriteName("friend-gradient.png"_spr)
            .color(globed::into<ccColor3B>(globed::color::FriendIngameGradient))
            .opacity(globed::color::FriendIngameGradient.a)
            .pos(0, 0)
            .anchorPoint({0, 0})
            .zOrder(-2)
            .scaleX(1.5)
            .blendFunc({GL_ONE, GL_ONE})
            .parent(this)
            .collect();

        util::ui::rescaleToMatch(cellGradient, {cellGradient->getScaledContentSize().width, CELL_HEIGHT}, true);

        // friend icon
        Build<CCSprite>::createSpriteName("friend-icon.png"_spr)
            .anchorPoint({0, 0.5})
            .scale(0.3)
            .parent(usernameLayout);
    }

    auto acm = GJAccountManager::get();
    if (data.accountId == acm->m_accountID) {
        CCSprite* gradient = Build<CCSprite>::createSpriteName("friend-gradient.png"_spr)
            .color(globed::into<ccColor3B>(globed::color::SelfIngameGradient))
            .opacity(globed::color::SelfIngameGradient.a)
            .pos(0, 0)
            .anchorPoint({0, 0})
            .zOrder(-2)
            .scaleX(10)
            .parent(this);

        util::ui::rescaleToMatch(gradient, {gradient->getScaledContentSize().width, CELL_HEIGHT}, true);
    }

    // percentage label
    Build<CCLabelBMFont>::create("", "goldFont.fnt")
        .scale(0.4f)
        .parent(usernameLayout)
        .id("percentage-label"_spr)
        .store(percentageLabel);

    usernameLayout->updateLayout();
    menu->updateLayout();

    this->fixNamePosition();

    this->makeButtons();

    this->refreshData(entry);
    this->schedule(schedule_selector(GlobedUserCell::updateVisualizer), 1.f / 60.f);

    return true;
}

void GlobedUserCell::refreshData(const PlayerStore::Entry& entry) {
    if (_data != entry) {
        _data = entry;

        bool platformer = GJBaseGameLayer::get()->m_level->isPlatformer();
        if (platformer && _data.localBest != 0) {
            percentageLabel->setString(util::format::formatPlatformerTime(_data.localBest).c_str());
            usernameLayout->updateLayout();
            this->fixNamePosition();
        } else if (!platformer) {
            percentageLabel->setString(fmt::format("{}%", _data.localBest).c_str());
            usernameLayout->updateLayout();
            this->fixNamePosition();
        }
    }
}

void GlobedUserCell::fixNamePosition() {
    nameBtn->setPositionY(CELL_HEIGHT / 2 - 2.5f);
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
    const float gap = 3.f;

    Build<CCMenu>::create()
        .anchorPoint(1.0f, 0.5f)
        .pos(GlobedUserListPopup::LIST_WIDTH - 7.f, CELL_HEIGHT / 2.f)
        .layout(RowLayout::create()
                    ->setGap(gap)
                    ->setAutoScale(false)
                    ->setAxisReverse(true)
                    ->setAxisAlignment(AxisAlignment::End))
        .id("btn-menu"_spr)
        .parent(menu)
        .store(buttonsWrapper);

    auto& settings = GlobedSettings::get();
    auto pl = GlobedGJBGL::get();

    bool notSelf = accountData.accountId != GJAccountManager::get()->m_accountID;

    if (!pl->m_fields->players.contains(accountData.accountId)) {
        return;
    }

    bool createBtnHide = notSelf; // always created
    bool createBtnMute = notSelf; // always created
    bool createBtnAdmin = AdminManager::get().authorized();

    using UserCellButton = BaseGameplayModule::UserCellButton;

    std::vector<BaseGameplayModule::UserCellButton> customButtons;
    for (auto& module : pl->m_fields->modules) {
        auto btns = module->onUserActionsPopup(accountData.accountId, !notSelf);
        for (auto& btn : btns) {
            customButtons.emplace_back(std::move(btn));
        }
    }

    bool createBtnTp = createBtnAdmin && notSelf;
    bool createVisualizer = settings.communication.voiceEnabled && notSelf;

    size_t buttonCount = (size_t)createBtnHide + createBtnMute + createBtnAdmin + createBtnTp + customButtons.size();

    // if no visualizer, max button count is 4, otherwise 2
    size_t maxButtonCount = createVisualizer ? 2 : 4;

    // we create the settings button and popup if things arent gonna fit
    bool createSettingsPopup = buttonCount > maxButtonCount;

    CCArray* mainButtons = CCArray::create();
    CCArray* popupButtons = CCArray::create();
    CCSize btnSize = {20.f, 20.f};
    CCSize btnSizeBig = {28.f, 28.f};

    // Creare various buttons

    // god i hate this
    bool muteAndHideInCell = (!createVisualizer || buttonCount == 2);

    bool isMuted = !pl->shouldLetMessageThrough(accountData.accountId);
    bool isHidden = notSelf ? pl->m_fields->players.at(accountData.accountId)->getForciblyHidden() : false;

    // Mute button
    auto* muteOn = CCSprite::createWithSpriteFrameName("icon-mute.png"_spr);
    auto* muteOff = CCSprite::createWithSpriteFrameName("icon-unmute.png"_spr);
    util::ui::rescaleToMatch(muteOn, muteAndHideInCell ? btnSize : btnSizeBig);
    util::ui::rescaleToMatch(muteOff, muteAndHideInCell ? btnSize : btnSizeBig);

    auto* muteButton = CCMenuItemExt::createToggler(
        muteOn,
        muteOff,
        [accountId = accountData.accountId, pl](CCMenuItemToggler* btn) {
            bool muted = btn->isOn();

            auto& bl = BlockListManager::get();
            muted ? bl.blacklist(accountId) : bl.whitelist(accountId);
            // mute them immediately
            auto& settings = GlobedSettings::get();
            auto& vpm = VoicePlaybackManager::get();

            if (!pl->m_fields->players.contains(accountId)) {
                return;
            }

            if (muted) {
                vpm.setVolume(accountId, 0.f);
            } else {
                vpm.setVolume(accountId, settings.communication.voiceVolume);
            }
        }
    );

    muteButton->setID("mute-btn"_spr);
    muteButton->m_onButton->m_scaleMultiplier = 1.2f;
    muteButton->m_offButton->m_scaleMultiplier = 1.2f;
    muteButton->toggle(!isMuted); // fucked up

    // Hide button

    auto* hideOn = CCSprite::createWithSpriteFrameName("icon-hide-player.png"_spr);
    auto* hideOff = CCSprite::createWithSpriteFrameName("icon-show-player.png"_spr);
    util::ui::rescaleToMatch(hideOn, muteAndHideInCell ? btnSize : btnSizeBig);
    util::ui::rescaleToMatch(hideOff, muteAndHideInCell ? btnSize : btnSizeBig);

    auto* hideButton = CCMenuItemExt::createToggler(
        hideOn,
        hideOff,
        [accountId = accountData.accountId, pl](CCMenuItemToggler* btn) {
            bool hidden = btn->isOn();

            auto& bl = BlockListManager::get();
            bl.setHidden(accountId, hidden);

            if (!pl->m_fields->players.contains(accountId)) {
                return;
            }

            pl->m_fields->players.at(accountId)->setForciblyHidden(hidden);
        }
    );

    hideButton->setID("hide-btn"_spr);
    hideButton->m_onButton->m_scaleMultiplier = 1.2f;
    hideButton->m_offButton->m_scaleMultiplier = 1.2f;
    hideButton->toggle(!isHidden);

    if (createSettingsPopup) {
        popupButtons->addObject(muteButton);
        popupButtons->addObject(hideButton);
    } else {
        // if not creating a settings popup, slot them here
        mainButtons->addObject(muteButton);
        mainButtons->addObject(hideButton);
    }

    // admin menu button
    if (createBtnAdmin) {
        auto btn = Build<CCSprite>::createSpriteName("GJ_reportBtn_001.png")
            .with([&](CCSprite* spr) {
                util::ui::rescaleToMatch(spr, btnSize);
            })
            .intoMenuItem([roomPreview = accountData.makeRoomPreview(0)](auto) {
                AdminManager::get().openUserPopup(roomPreview);
            })
            .id("admin-button"_spr)
            .collect();

        mainButtons->addObject(btn);
    }

    if (createBtnTp) {
        auto btnSprite = Build<CCSprite>::createSpriteName("icon-teleport.png"_spr).collect();

        if (createSettingsPopup) {
            util::ui::rescaleToMatch(btnSprite, btnSizeBig);
        } else {
            util::ui::rescaleToMatch(btnSprite, btnSize);
        }

        auto btn = Build(btnSprite)
            .intoMenuItem([pl, accountId = accountData.accountId](auto) {
                auto& settings = GlobedSettings::get();
                if (!settings.flags.seenTeleportNotice)  {
                    settings.flags.seenTeleportNotice = true;

                    FLAlertLayer::create(
                        "Note",
                        "Teleporting to a player will <cr>disable level progress</c> until you <cy>fully reset</c> the level.",
                        "Ok"
                    )->show();

                    return;
                }

                pl->toggleSafeMode(true);

                PlayerObject* po1 = pl->m_player1;
                CCPoint position = {0, 0}; // just in case the player has already left by the time we teleport
                if (pl->m_fields->interpolator->hasPlayer(accountId)) position = pl->m_fields->interpolator->getPlayerState(accountId).player1.position;

                po1->m_position = position;
            })
            .id("teleport-button"_spr)
            .collect();

        if (createSettingsPopup) {
            popupButtons->addObject(btn);
        } else {
            mainButtons->addObject(btn);
        }
    }

    for (auto& btn : customButtons) {
        auto spr = Build<CCSprite>::createSpriteName(btn.spriteName.c_str())
            .with([&](CCSprite* spr) {
                util::ui::rescaleToMatch(spr, btnSize);
            })
            .intoMenuItem(std::move(btn.callback))
            .scaleMult(1.2f)
            .id(btn.id)
            .zOrder(btn.order)
            .collect();

        if (mainButtons->count() < maxButtonCount) {
            util::ui::rescaleToMatch(spr, btnSize);
            mainButtons->addObject(spr);
        } else {
            util::ui::rescaleToMatch(spr, btnSizeBig);
            popupButtons->addObject(spr);
        }
    }

#ifdef GLOBED_VOICE_SUPPORT
    if (createVisualizer) {
        // audio visualizer
        Build<GlobedAudioVisualizer>::create()
            .id("audio-visualizer"_spr)
            .zOrder(999999) // force to be on the left
            .store(audioVisualizer);

        audioVisualizer->setScaleX(0.5f);

        mainButtons->addObject(audioVisualizer);
    }
#endif // GLOBED_VOICE_SUPPORT

    for (auto node : CCArrayExt<CCNode*>(mainButtons)) {
        buttonsWrapper->addChild(node);
    }

    if (createSettingsPopup && popupButtons->count() > 0) {
        this->popupButtons = popupButtons;

        // settings button
        Build<CCSprite>::createSpriteName("GJ_optionsBtn_001.png")
            .with([&](CCSprite* spr) {
                util::ui::rescaleToMatch(spr, btnSize);
            })
            .intoMenuItem([this, id = accountData.accountId](auto) {
                auto* pl = GlobedGJBGL::get();

                // if they left the level, do nothing
                if (!pl->m_fields->players.contains(id)) {
                    return;
                }

                GlobedUserActionsPopup::create(id, this->popupButtons)->show();
            })
            .zOrder(-999999) // force to be on the right
            .scaleMult(1.2f)
            .parent(buttonsWrapper)
            .id("player-actions-button"_spr);
    }

    buttonsWrapper->setContentWidth(150.f);
    buttonsWrapper->updateLayout();
}

GlobedUserCell* GlobedUserCell::create(const PlayerStore::Entry& entry, const PlayerAccountData& data, GlobedUserListPopup* parent) {
    auto ret = new GlobedUserCell;
    if (ret->init(entry, data, parent)) {
        return ret;
    }

    delete ret;
    return nullptr;
}