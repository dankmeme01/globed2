#include "room_settings_popup.hpp"

#include <managers/error_queues.hpp>
#include <data/packets/server/room.hpp>
#include <data/packets/client/room.hpp>
#include <net/manager.hpp>
#include <util/ui.hpp>

using namespace geode::prelude;

enum {
    TAG_COLLISION = 454,
    TAG_TWO_PLAYER,
    TAG_PUBLIC_INVITES,
    TAG_INVITE_ONLY,
    TAG_DEATHLINK
};

#define MAKE_SETTING(name, desc, tag, storage) \
    { \
        storage = RoomSettingCell::create(name, desc, tag, this); \
        cells->addObject(storage); \
    }

bool RoomSettingsPopup::setup() {
    auto popupLayout = util::ui::getPopupLayout(m_size);

    auto* cells = CCArray::create();

    MAKE_SETTING("Hidden Room", "While enabled, the room can not be found on the public room listing and can only be joined by entering the room ID", TAG_INVITE_ONLY, cellInviteOnly);
    MAKE_SETTING("Open Invites", "While enabled, all players in the room can invite players instead of just the room owner", TAG_PUBLIC_INVITES, cellPublicInvites);
    MAKE_SETTING("Collision", "While enabled, players can collide with each other.\n\n<cy>Note: this enables safe mode, making it impossible to make progress on levels.</c>", TAG_COLLISION, cellCollision);

#ifdef GLOBED_DEBUG
    MAKE_SETTING("2-Player Mode", "While enabled, players can link with another player to play a 2-player enabled level together", TAG_TWO_PLAYER, cellTwoPlayer);
#endif

    MAKE_SETTING("Death Link", "Whenever a player dies, everyone on the level dies as well. <cy>Inspired by the mod DeathLink from</c> <cg>Alphalaneous</c>.", TAG_DEATHLINK, cellDeathlink);

    auto* listLayer = Build(SettingList::createForComments(LIST_WIDTH, LIST_HEIGHT, RoomSettingCell::CELL_HEIGHT))
        .scale(0.65f)
        .anchorPoint(0.5f, 1.f)
        .pos(popupLayout.fromTop(18.f))
        .parent(m_mainLayer)
        .collect();

    listLayer->swapCells(cells);

    NetworkManager::get().addListener<RoomInfoPacket>(this, [this](auto packet) {
        RoomManager::get().setInfo(packet->info);
        this->currentSettings = packet->info.settings;
        this->updateCheckboxes();
    });

    currentSettings = RoomManager::get().getInfo().settings;
    this->updateCheckboxes();

    return true;
}

void RoomSettingsPopup::onSettingClicked(cocos2d::CCObject* sender) {
    bool enabled = !static_cast<CCMenuItemToggler*>(sender)->isOn();
    int setting = sender->getTag();

    switch (setting) {
        case TAG_INVITE_ONLY: currentSettings.flags.isHidden = enabled; break;
        case TAG_PUBLIC_INVITES: currentSettings.flags.publicInvites = enabled; break;
        case TAG_COLLISION: currentSettings.flags.collision = enabled; break;
#ifdef GLOBED_DEBUG
        case TAG_TWO_PLAYER: currentSettings.flags.twoPlayerMode = enabled; break;
#endif
        case TAG_DEATHLINK: currentSettings.flags.deathlink = enabled; break;
    }

    // if we are not the room owner, just revert the changes next frame
    if (!RoomManager::get().isOwner()) {
        ErrorQueues::get().warn("Not the room creator");
        Loader::get()->queueInMainThread([self = Ref(this)] {
            self->updateCheckboxes();
        });
    } else {
        // otherwise, actually update the settings
        NetworkManager::get().send(UpdateRoomSettingsPacket::create(currentSettings));
    }
}

void RoomSettingsPopup::updateCheckboxes() {
    cellInviteOnly->setToggled(currentSettings.flags.isHidden);
    cellPublicInvites->setToggled(currentSettings.flags.publicInvites);
    cellCollision->setToggled(currentSettings.flags.collision);
#ifdef GLOBED_DEBUG
    cellTwoPlayer->setToggled(currentSettings.flags.twoPlayerMode);
#endif
    cellDeathlink->setToggled(currentSettings.flags.deathlink);

    this->enableCheckboxes(RoomManager::get().isOwner());
}

void RoomSettingsPopup::enableCheckboxes(bool enabled) {
    for (auto* cell : {
        cellInviteOnly, cellPublicInvites, cellCollision
#ifdef GLOBED_DEBUG
        , cellTwoPlayer
#endif
        , cellDeathlink
    }) {
        cell->setEnabled(enabled);
    }
}

RoomSettingsPopup* RoomSettingsPopup::create() {
    auto ret = new RoomSettingsPopup;
    if (ret->init(POPUP_WIDTH, POPUP_HEIGHT)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

bool RoomSettingCell::init(const char* name, std::string desc, int tag, RoomSettingsPopup* popup) {
    if (!CCLayer::init()) return false;
    this->popup = popup;

    Build(CCMenuItemToggler::createWithStandardSprites(popup, menu_selector(RoomSettingsPopup::onSettingClicked), 0.7f)) \
        .tag(tag)
        .store(button)
        .anchorPoint(0.5f, 0.5f)
        .pos(CELL_WIDTH - 20.f, CELL_HEIGHT / 2)
        .intoNewParent(CCMenu::create())
        .pos(0.f, 0.f)
        .parent(this);

    CCMenu* layout = Build<CCMenu>::create()
        .layout(RowLayout::create()->setGap(5.f)->setAutoScale(false)->setAxisAlignment(AxisAlignment::Start))
        .contentSize({CELL_WIDTH, CELL_HEIGHT})
        .anchorPoint(0.f, 0.5f)
        .pos(10.f, CELL_HEIGHT / 2)
        .parent(this)
        .collect()
        ;

    CCLabelBMFont* nameLabel = Build<CCLabelBMFont>::create(name, "bigFont.fnt")
        .scale(0.6f)
        //.anchorPoint(0.f, 0.5f)
        //.pos(10.f, CELL_HEIGHT / 2)
        .parent(layout)
        .collect()
        ;

    if (!desc.empty()) {
        Build<CCSprite>::createSpriteName("GJ_infoIcon_001.png")
            .scale(0.75f)
            .intoMenuItem([this, name, desc] {
                FLAlertLayer::create(
                    name,
                    desc,
                    "OK"
                )->show();
            })
            .move(ccp(0.f, -3.f))
            //.pos(10.f + (std::string(name).size() * 12.f), CELL_HEIGHT / 2)
            //.intoNewParent(CCMenu::create())
            //.pos(0.f, 0.f)
            .parent(layout)
            ;
    }

    layout->updateLayout();

    return true;
}

void RoomSettingCell::setToggled(bool state) {
    button->toggle(state);
}

void RoomSettingCell::setEnabled(bool state) {
    button->setEnabled(state);
    button->m_offButton->setOpacity(state ? 255 : 167);
    button->m_onButton->setOpacity(state ? 255 : 167);
}

RoomSettingCell* RoomSettingCell::create(const char* name, std::string desc, int tag, RoomSettingsPopup* popup) {
    auto ret = new RoomSettingCell;
    if (ret->init(name, desc, tag, popup)) {
        return ret;
    }

    delete ret;
    return nullptr;
}
