#include "room_listing_cell.hpp"

#include "room_password_popup.hpp"
#include "room_listing_popup.hpp"
#include <data/packets/client/room.hpp>
#include <managers/role.hpp>
#include <net/manager.hpp>
#include <ui/general/simple_player.hpp>
#include <util/ui.hpp>
#include <util/gd.hpp>

using namespace geode::prelude;

namespace {
    namespace btnorder {
        constexpr int Join = 43;
        constexpr int Settings = 44;
        constexpr int PlayerCount = 45;
    }
}

bool RoomListingCell::init(const RoomListingInfo& rli, RoomListingPopup* parent) {
    if (!CCLayerColor::init())
        return false;

    this->parent = parent;
    this->playerCount = rli.playerCount;
    this->ownerData = rli.owner;

    this->setContentSize({RoomListingPopup::LIST_WIDTH, CELL_HEIGHT});
    this->setAnchorPoint({0.f, 0.f});

    // background
    const float scaleMult = 3.f;
    Build<CCScale9Sprite>::create("square02_001.png")
        .contentSize(this->getContentSize() * scaleMult)
        .scale(1.f / scaleMult)
        .opacity(67)
        .zOrder(-1)
        .anchorPoint(0.f, 0.f)
        .parent(this);

    auto* playerBundle = Build<CCMenu>::create()
        .pos(4.f, 15.5f)
        .anchorPoint(0.f, 1.f)
        .layout(RowLayout::create()
            ->setGap(3.f)
            ->setGrowCrossAxis(true)
            ->setCrossAxisReverse(true)
            ->setAutoScale(false)
            ->setAxisAlignment(AxisAlignment::Start)
        )
        .parent(this)
        .collect();

    auto* playerIcon = Build<GlobedSimplePlayer>::create(rli.owner)
        .scale(0.35f)
        .parent(playerBundle)
        .zOrder(-1)
        .collect();

    auto* nameLabel = Build<CCLabelBMFont>::create(rli.owner.name.c_str(), "goldFont.fnt")
        .scale(0.5f)
        .id("player-name")
        .zOrder(2)
        .collect();

    // badge with s
    if (rli.owner.specialUserData.roles) {
        std::vector<std::string> badgeVector = RoleManager::get().getBadgeList(rli.owner.specialUserData.roles.value());
        util::ui::addBadgesToMenu(badgeVector, playerBundle, 1, util::ui::BADGE_SIZE_SMALL);
    }

    auto* usernameButton = Build<CCMenuItemSpriteExtra>::create(nameLabel, this, menu_selector(RoomListingCell::onUser))
        .pos(3.f, 50.f)
        .zOrder(0)
        .scaleMult(1.1f)
        .parent(playerBundle)
        .collect();

    playerBundle->updateLayout();
    usernameButton->setPositionY(usernameButton->getPositionY() + 1.f); // move text slightly up

    // set the zorder of the button to be the highest, so that when you hold it, the badge and player icon are behind
    // this also *must* be called after updateLayout().
    usernameButton->setZOrder(10);

    auto* roomNameLayout = Build<CCNode>::create()
        .pos(4, CELL_HEIGHT - 10.f)
        .anchorPoint(0.f, 0.5f)
        .layout(
            RowLayout::create()
                ->setGap(3.f)
                ->setAutoScale(false)
                ->setAxisAlignment(AxisAlignment::Start)
        )
        .contentSize(RoomListingPopup::LIST_WIDTH, 0.f)
        .parent(this)
        .collect();

    CCLabelBMFont* roomNameLabel = Build<CCLabelBMFont>::create(rli.name.c_str(), "bigFont.fnt")
        .limitLabelWidth(180.0f, 0.48f, 0.1f)
        .id("message-text")
        .parent(roomNameLayout)
        .collect();

    roomNameLayout->updateLayout();

    auto* rightMenu = Build<CCMenu>::create()
        .layout(
            RowLayout::create()
                    ->setAxisReverse(true)
                    ->setAxisAlignment(AxisAlignment::End)
                    ->setAutoScale(false)
                    ->setGap(4.f)
        )
        .anchorPoint(1.f, 0.5f)
        .pos(RoomListingPopup::LIST_WIDTH - 3.f, CELL_HEIGHT / 2.f)
        .contentSize(this->getContentSize())
        .parent(this)
        .collect();

    auto* roomSettingsMenu = Build<CCMenu>::create()
        .layout(
            RowLayout::create()
                ->setAxisReverse(true)
                // ->setAxisAlignment(AxisAlignment::End)
                ->setAutoScale(false)
                ->setGap(1.f)
        )
        .contentSize(58.f, this->getContentHeight())
        .parent(rightMenu)
        .zOrder(btnorder::Settings)
        .collect();

    // join button
    Build<ButtonSprite>::create("Join", "bigFont.fnt", "GJ_button_01.png", 0.8f)
        .scale(0.7f)
        .intoMenuItem([this, rli](auto) {
            if (rli.hasPassword) {
                RoomPasswordPopup::create(rli.id)->show();
                return;
            }

            NetworkManager::get().send(JoinRoomPacket::create(rli.id, std::string_view("")));
        })
        .scaleMult(1.15f)
        .zOrder(btnorder::Join)
        .parent(rightMenu);

    // lock button
    auto* lockSpr = Build<CCSprite>::createSpriteName("room-icon-lock.png"_spr)
        .scale(0.38f)
        .opacity(rli.hasPassword ? 255 : 80)
        .intoMenuItem([] {
            FLAlertLayer::create("Locked room", "This room requires a password to join.", "Ok")->show();
        })
        .parent(roomSettingsMenu)
        .collect();

    lockSpr->setEnabled(rli.hasPassword);

    // collision icon
    auto* collisionBtn = Build<CCSprite>::createSpriteName("room-icon-collision.png"_spr)
        .with([&](auto* s) {
            util::ui::rescaleToMatch(s, lockSpr->getScaledContentSize());
        })
        .opacity(rli.settings.flags.collision ? 255 : 80)
        .intoMenuItem([] {
            FLAlertLayer::create("Collision", "This room has collision enabled, meaning you can collide with other players.\n\n<cy>Note: this means the room has safe mode, making it impossible to make progress on levels.</c>", "Ok")->show();
        })
        .parent(roomSettingsMenu)
        .collect();

    collisionBtn->setEnabled(rli.settings.flags.collision);

    // deathlink icon
    auto* deathlinkBtn = Build<CCSprite>::createSpriteName("room-icon-deathlink.png"_spr)
        .with([&](auto* s) {
            util::ui::rescaleToMatch(s, lockSpr->getScaledContentSize());
        })
        .opacity(rli.settings.flags.deathlink ? 255 : 80)
        .intoMenuItem([] {
            FLAlertLayer::create("Death Link", "This room has Death Link enabled, which means that if a player dies, everyone in the level dies as well. <cy>Inspired by the mod DeathLink by </c> <cg>Alphalaneous</c>.", "Ok")->show();
        })
        .parent(roomSettingsMenu)
        .collect();

    deathlinkBtn->setEnabled(rli.settings.flags.deathlink);

    auto* playerCountWrapper = Build<CCNode>::create()
        .layout(RowLayout::create()->setGap(1.f)->setAutoScale(false))
        .parent(rightMenu)
        .zOrder(btnorder::PlayerCount)
        .collect();

    // player count number
    int playerCount = rli.playerCount;
    std::string playerCountText = rli.settings.playerLimit == 0 ? std::to_string(playerCount) : fmt::format("{}/{}", playerCount, rli.settings.playerLimit);

    auto* playerCountLabel = Build<CCLabelBMFont>::create(playerCountText.c_str(), "bigFont.fnt")
        .limitLabelWidth(100.f * 0.35f, 0.35f, 0.15f)
        .parent(playerCountWrapper)
        .collect();

    // player count icon
    auto* playerCountIcon = Build<CCSprite>::createSpriteName("icon-person.png"_spr)
        .with([&](auto* p) {
            util::ui::rescaleToMatch(p, util::ui::BADGE_SIZE_SMALL * 0.9f);
        })
        .parent(playerCountWrapper)
        .collect();

    playerCountWrapper->setContentWidth(1.f + playerCountIcon->getScaledContentSize().width + playerCountLabel->getScaledContentSize().width);

    playerCountWrapper->updateLayout();

    roomSettingsMenu->updateLayout();
    rightMenu->updateLayout();

    // add a bg
    float sizeScale = 3.f;
    auto* settingsBg = Build<CCScale9Sprite>::create("square02_001.png")
        .opacity(67)
        .zOrder(-1)
        .contentSize(roomSettingsMenu->getScaledContentSize() * sizeScale + CCPoint{6.f, 8.f})
        .scaleX(1.f / sizeScale)
        .scaleY(1.f / sizeScale)
        .parent(roomSettingsMenu)
        .anchorPoint(0.5f, 0.5f)
        .pos(roomSettingsMenu->getScaledContentSize() / 2.f)
        .collect();

    Build<CCScale9Sprite>::create("square02_001.png")
        .opacity(67)
        .zOrder(-1)
        .contentSize(playerCountWrapper->getScaledContentSize().width * sizeScale + 8.f, settingsBg->getContentHeight())
        .scaleX(1.f / sizeScale)
        .scaleY(1.f / sizeScale)
        .parent(playerCountWrapper)
        .anchorPoint(0.5f, 0.5f)
        .pos(playerCountWrapper->getScaledContentSize() / 2.f);

    return true;
}

void RoomListingCell::onUser(CCObject* sender) {
    util::gd::openProfile(ownerData.accountId, ownerData.userId, ownerData.name);
}

RoomListingCell* RoomListingCell::create(const RoomListingInfo& rli, RoomListingPopup* parent) {
    auto* ret = new RoomListingCell();
    if (ret->init(rli, parent)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}
