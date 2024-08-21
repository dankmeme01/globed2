#include "player_list_cell.hpp"

#include <data/packets/client/room.hpp>
#include <hooks/level_select_layer.hpp>
#include <hooks/gjgamelevel.hpp>
#include <managers/admin.hpp>
#include <managers/friend_list.hpp>
#include <managers/role.hpp>
#include <net/manager.hpp>
#include <ui/menu/room/download_level_popup.hpp>
#include <ui/general/simple_player.hpp>
#include <util/ui.hpp>
#include <util/gd.hpp>

using namespace geode::prelude;

namespace {
    namespace btnorder {
        constexpr int Icon = 1;
        constexpr int Name = 2;
        constexpr int Badge = 3;
        constexpr int FriendsIcon = 4;
    }
}

bool PlayerListCell::init(const PlayerRoomPreviewAccountData& data, float cellWidth, bool forInviting, bool isIconLazyLoad) {
    if (!CCLayer::init()) return false;

    this->playerData = data;
    this->cellWidth = cellWidth;

    this->setContentWidth(cellWidth);
    this->setContentHeight(CELL_HEIGHT);

    auto* gm = GameManager::get();

    Build<CCMenu>::create()
        .pos(10.f, CELL_HEIGHT / 2.f)
        .anchorPoint(0.f, 0.5f)
        .layout(RowLayout::create()->setAutoScale(false)->setAxisAlignment(AxisAlignment::Start))
        .contentSize(cellWidth * 0.75f, CELL_HEIGHT)
        .parent(this)
        .id("left-layout")
        .store(leftSideLayout);

    if (isIconLazyLoad) {
        this->createPlaceholderPlayerIcon();
    } else {
        this->createPlayerIcon();
    }

    // name label
    RichColor nameColor = util::ui::getNameRichColor(playerData.specialUserData);
    float labelWidth;

    auto* nameBtn = Build<CCLabelBMFont>::create(playerData.name.c_str(), "bigFont.fnt")
        .limitLabelWidth(170.f, 0.6f, 0.1f)
        .with([&, nameColor = std::move(nameColor)](CCLabelBMFont* label) {
            label->setScale(label->getScale() * 0.9f);
            labelWidth = label->getScaledContentSize().width;
            nameColor.animateLabel(label);

            // TODO: this is a shitty workaround but for some reason it didnt work
            Loader::get()->queueInMainThread([label = Ref(label), nameColor = std::move(nameColor)] {
                nameColor.animateLabel(label);
            });
        })
        .intoMenuItem(this, menu_selector(PlayerListCell::onOpenProfile))
        .zOrder(btnorder::Name)
        .scaleMult(1.1f)
        .parent(leftSideLayout)
        .collect();

    nameBtn->setLayoutOptions(AxisLayoutOptions::create()->setPrevGap(10.f));

    // badge with s
    if (playerData.specialUserData.roles) {
        std::vector<std::string> badgeVector = RoleManager::get().getBadgeList(playerData.specialUserData.roles.value());
        util::ui::addBadgesToMenu(badgeVector, leftSideLayout, btnorder::Badge);
    }

    // friend gradient and own gradient
    if (FriendListManager::get().isFriend(data.accountId)) {
        CCSprite* gradient = Build<CCSprite>::createSpriteName("friend-gradient.png"_spr)
            .color(globed::into<ccColor3B>(forInviting ? globed::color::FriendIngameGradient : globed::color::FriendGradient))
            .opacity(forInviting ? 75 : 90)
            .pos(0, 0)
            .anchorPoint({0, 0})
            .zOrder(-2)
            .scaleX(1.5)
            .parent(this);

        if (forInviting)
            gradient->setBlendFunc({GL_ONE, GL_ONE});

        CCSprite* icon = Build<CCSprite>::createSpriteName("friend-icon.png"_spr)
            .scale(0.3)
            .zOrder(btnorder::FriendsIcon)
            .parent(leftSideLayout);
    }

    auto* gjam = GJAccountManager::get();
    if (playerData.accountId == gjam->m_accountID) {
        CCSprite* gradient = Build<CCSprite>::createSpriteName("friend-gradient.png"_spr)
            .color(globed::into<ccColor3B>(globed::color::SelfGradient))
            .opacity(globed::color::SelfGradient.a)
            .pos(0, 0)
            .anchorPoint({0, 0})
            .zOrder(-2)
            .scaleX(10)
            //.blendFunc({GL_ONE, GL_ONE})
            .parent(this);
    }

    leftSideLayout->updateLayout();

    nameBtn->setPositionY(CELL_HEIGHT / 2 - 5.00f);

    Build<CCMenu>::create()
        .anchorPoint(1.f, 0.5f)
        .pos(cellWidth - (forInviting ? 10.f : 5.f), CELL_HEIGHT / 2.f)
        .contentSize(cellWidth - 10.f, CELL_HEIGHT)
        .layout(RowLayout::create()->setGap(5.f)->setAxisAlignment(AxisAlignment::End)->setAxisReverse(true))
        .parent(this)
        .store(rightButtonMenu);

    if (AdminManager::get().authorized() && !forInviting) {
        this->createAdminButton();
    }

    if (forInviting) {
        this->createInviteButton();
    } else if (playerData.levelId != 0) {
        this->createJoinButton();
    }

    return true;
}

bool PlayerListCell::isIconLoaded() {
    return simplePlayer != nullptr;
}

void PlayerListCell::createPlayerIcon() {
    if (placeholderIcon) {
        placeholderIcon->removeFromParent();
        placeholderIcon = nullptr;
    }

    // player cube icon
    if (!simplePlayer) {
        Build<GlobedSimplePlayer>::create(playerData)
            .scale(0.65f)
            .zOrder(btnorder::Icon)
            .id("player-icon"_spr)
            .parent(leftSideLayout)
            .store(simplePlayer);

        leftSideLayout->updateLayout();
    }
}

void PlayerListCell::createPlaceholderPlayerIcon() {
    if (placeholderIcon || simplePlayer) return;

    Build<CCSprite>::createSpriteName("default-player-cube.png"_spr)
        .scale(0.65f)
        .zOrder(btnorder::Icon)
        .id("player-icon-placeholder")
        .parent(leftSideLayout)
        .store(placeholderIcon);
}

void PlayerListCell::createInviteButton() {
    Build<CCSprite>::createSpriteName("icon-invite.png"_spr)
        .scale(0.85f)
        .intoMenuItem([this](auto) {
            this->sendInvite();
        })
        .scaleMult(1.25f)
        .store(inviteButton)
        .parent(rightButtonMenu);

    rightButtonMenu->updateLayout();
}

void PlayerListCell::createJoinButton() {
    Build<CCSprite>::createSpriteName("GJ_playBtn2_001.png")
        .scale(0.31f)
        .intoMenuItem([levelId = this->playerData.levelId](auto) {
            auto* glm = GameLevelManager::sharedState();
            auto mlevel = glm->m_mainLevels->objectForKey(std::to_string(levelId));
            bool isMainLevel = std::find(HookedLevelSelectLayer::MAIN_LEVELS.begin(), HookedLevelSelectLayer::MAIN_LEVELS.end(), levelId) != HookedLevelSelectLayer::MAIN_LEVELS.end();

            if (mlevel != nullptr) {
                // if its a classic main level go to that page in LevelSelectLayer
                if (isMainLevel) {
                    util::ui::switchToScene(LevelSelectLayer::scene(levelId - 1));
                    return;
                }

                // otherwise we just go right to playlayer
                auto level = static_cast<HookedGJGameLevel*>(glm->getMainLevel(levelId, false));
                level->m_fields->shouldTransitionWithPopScene = true;
                util::ui::switchToScene(PlayLayer::scene(level, false, false));
                return;
            }

            if (auto popup = DownloadLevelPopup::create(levelId)) {
                popup->show();
            }
        })
        .zOrder(10)
        .pos(cellWidth - 30.f, CELL_HEIGHT / 2.f)
        .scaleMult(1.1f)
        .parent(rightButtonMenu);

    rightButtonMenu->updateLayout();
}

void PlayerListCell::createAdminButton() {
    // admin menu button
    Build<CCSprite>::createSpriteName("GJ_reportBtn_001.png")
        .scale(0.525f)
        .intoMenuItem([this](auto) {
            AdminManager::get().openUserPopup(playerData);
        })
        .parent(rightButtonMenu)
        .zOrder(3)
        .id("admin-button"_spr);

    rightButtonMenu->updateLayout();
}

void PlayerListCell::sendInvite() {
    NetworkManager::get().send(RoomSendInvitePacket::create(playerData.accountId));

    // disable sending invites for a few seconds
    inviteButton->setEnabled(false);
    inviteButton->setOpacity(90);

    this->runAction(CCSequence::create(
        CCDelayTime::create(3.f),
        CCCallFunc::create(this, callfunc_selector(PlayerListCell::enableInvites)),
        nullptr
    ));
}

void PlayerListCell::enableInvites() {
    inviteButton->setEnabled(true);
    inviteButton->setOpacity(255);
}

void PlayerListCell::onOpenProfile(cocos2d::CCObject*) {
    util::gd::openProfile(playerData.accountId, playerData.userId, playerData.name);
}

PlayerListCell* PlayerListCell::create(const PlayerRoomPreviewAccountData& data, float cellWidth, bool forInviting, bool isIconLazyLoad) {
    auto ret = new PlayerListCell;
    if (ret->init(data, cellWidth, forInviting, isIconLazyLoad)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}
