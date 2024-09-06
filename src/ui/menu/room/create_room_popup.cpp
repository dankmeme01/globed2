#include "create_room_popup.hpp"

#include <Geode/ui/TextInput.hpp>

#include "room_layer.hpp"
#include <data/packets/client/room.hpp>
#include <net/manager.hpp>
#include <util/format.hpp>
#include <util/math.hpp>
#include <util/ui.hpp>
#include <util/gd.hpp>

using namespace geode::prelude;

constexpr int TAG_PRIVATE = 1021;
constexpr int TAG_OPEN_INV = 1022;
constexpr int TAG_COLLISION = 1023;
constexpr int TAG_2P = 1024;
constexpr int TAG_DEATHLINK = 1025;

bool CreateRoomPopup::setup(RoomLayer* parent) {
    this->setID("CreateRoomPopup"_spr);

    this->setTitle("Create Room", "goldFont.fnt", 1.0f);

    auto rlayout = util::ui::getPopupLayoutAnchored(m_size);

    auto inputsWrapper = Build<CCNode>::create()
        .id("inputs-wrapper")
        .anchorPoint(0.f, 0.5f)
        .pos(rlayout.centerLeft + CCPoint{15.f, 10.f})
        .layout(ColumnLayout::create()->setAutoScale(false)->setGap(3.f))
        .contentSize(0.f, POPUP_HEIGHT * 0.6f)
        .parent(m_mainLayer)
        .collect();

    auto smallInputsWrapper = Build<CCNode>::create()
        .id("small-wrapper")
        .layout(RowLayout::create()->setAutoScale(false))
        .parent(inputsWrapper)
        .collect();

    // room name
    auto roomNameWrapper = Build<CCNode>::create()
        .id("name-wrapper")
        .layout(ColumnLayout::create()->setAxisReverse(true)->setAutoScale(false))
        .contentSize(0.f, 55.f)
        .intoNewChild(CCLabelBMFont::create("Room Name", "bigFont.fnt"))
        .scale(0.5f)
        .intoParent()
        .parent(inputsWrapper)
        .intoNewChild(TextInput::create(POPUP_WIDTH * 0.515f, "", "chatFont.fnt"))
        .with([&](TextInput* input) {
            input->setCommonFilter(CommonFilter::Any);
            input->setMaxCharCount(32);
        })
        .store(roomNameInput)
        .intoParent()
        .updateLayout()
        .collect();

    // password
    Build<CCNode>::create()
        .id("pw-wrapper")
        .layout(ColumnLayout::create()->setAxisReverse(true)->setAutoScale(false))
        .contentSize(0.f, 50.f)
        .intoNewChild(CCLabelBMFont::create("Password", "bigFont.fnt"))
        .scale(0.35f)
        .intoParent()
        .parent(smallInputsWrapper)
        .intoNewChild(TextInput::create(POPUP_WIDTH * 0.25f, "", "chatFont.fnt"))
        .with([&](TextInput* input) {
            input->setFilter(std::string(util::misc::STRING_ALPHANUMERIC));
            input->setMaxCharCount(16);
        })
        .store(passwordInput)
        .intoParent()
        .updateLayout();

    // player limit
    Build<CCNode>::create()
        .id("limit-wrapper")
        .layout(ColumnLayout::create()->setAxisReverse(true)->setAutoScale(false))
        .contentSize(0.f, 50.f)
        .intoNewChild(CCLabelBMFont::create("Player limit", "bigFont.fnt"))
        .scale(0.35f)
        .intoParent()
        .parent(smallInputsWrapper)
        .intoNewChild(TextInput::create(POPUP_WIDTH * 0.25f, "", "chatFont.fnt"))
        .with([&](TextInput* input) {
            input->setFilter(std::string(util::misc::STRING_DIGITS));
            input->setMaxCharCount(6);
        })
        .store(playerLimitInput)
        .intoParent()
        .updateLayout();

    // cancel/create buttons
    Build<CCMenu>::create()
        .id("btn-wrapper")
        .layout(RowLayout::create()->setAutoScale(false))
        .parent(m_mainLayer)
        .child(
            Build<ButtonSprite>::create("Cancel", "goldFont.fnt", "GJ_button_05.png", 0.8f)
                .intoMenuItem([this, parent](auto) {
                    this->onClose(nullptr);
                })
                .collect()
        )
        .child(
            Build<ButtonSprite>::create("Create", "goldFont.fnt", "GJ_button_04.png", 0.8f)
                .intoMenuItem([this, parent](auto) {
                    std::string roomName = roomNameInput->getString();
                    if (roomName.empty()) {
                        roomName = fmt::format("{}'s room", GJAccountManager::get()->m_username);
                    }

                    roomName = util::format::trim(roomName);

                    // parse as a 32-bit int but cap at 10000
                    // this is so that if a user inputs a number like 99999 (doesnt fit),
                    // instead of making it 0, it makes it 10000

                    uint32_t playerCount = util::format::parse<uint32_t>(playerLimitInput->getString()).value_or(0);
                    playerCount = util::math::min(playerCount, 10000);

                    NetworkManager::get().send(CreateRoomPacket::create(roomName, passwordInput->getString(), RoomSettings {
                        .flags = settingFlags,
                        .playerLimit = static_cast<uint16_t>(playerCount),
                        .fasterReset = util::gd::variable(util::gd::GameVariable::FastRespawn)
                    }));

                    parent->startLoading();
                    this->onClose(nullptr);
                })
                .collect()
        )
        .pos(rlayout.centerBottom + CCPoint{0.f, 25.f})
        .updateLayout();

    smallInputsWrapper->setContentWidth(roomNameWrapper->getScaledContentWidth());
    smallInputsWrapper->updateLayout();
    inputsWrapper->updateLayout();


    // list of settings
    const float gap = 3.f;
    auto* settingsList = Build<CCNode>::create()
        .id("settings")
        .anchorPoint(1.f, 0.5f)
        .pos(rlayout.centerRight - CCPoint{15.f, 0.f})
        .layout(ColumnLayout::create()->setAxisReverse(true)->setGap(gap))
        .contentSize(0.f, POPUP_HEIGHT * 0.65f)
        .parent(m_mainLayer)
        .collect()
        ;

    constexpr auto settings = std::to_array<std::pair<const char*, int>>({
        {"Hidden Room", TAG_PRIVATE},
        {"Open Invites", TAG_OPEN_INV},
        {"Collision", TAG_COLLISION},
#ifdef GLOBED_DEBUG
        {"2-Player Mode", TAG_2P},
#endif
        {"Death Link", TAG_DEATHLINK}
    });

    float totalHeight = 0.f;

    for (const auto& entry : settings) {
        const float height = 15.5f;
        const float width = 110.5f;

        if (totalHeight != 0.f) {
            // gap between settings
            totalHeight += gap;
        }

        totalHeight += height;

        Build(CCMenuItemToggler::create(
            Build<CCSprite>::createSpriteName("GJ_checkOff_001.png").scale(0.5f),
            Build<CCSprite>::createSpriteName("GJ_checkOn_001.png").scale(0.5f),
            this, menu_selector(CreateRoomPopup::onCheckboxToggled)
        ))
            .pos(width - 11.f, height / 2.f)
            .tag(entry.second)
            .intoNewParent(CCMenu::create())
            .contentSize(width, height)
            .parent(settingsList)
            .intoNewChild(CCLabelBMFont::create(entry.first, "bigFont.fnt"))
            .limitLabelWidth(width - 30.f, 0.35f, 0.1f)
            .anchorPoint(0.f, 0.5f)
            .pos(3.f, height / 2.f)
            .intoParent()
            .updateLayout();
    }

    settingsList->setContentHeight(totalHeight + 5.f);
    settingsList->updateLayout();

    // add bg
    float sizeScale = 3.f;
    Build<CCScale9Sprite>::create("square02_001.png")
        .opacity(67)
        .zOrder(-1)
        .contentSize(settingsList->getScaledContentSize() * sizeScale + CCPoint{8.f, 8.f})
        .scaleX(1.f / sizeScale)
        .scaleY(1.f / sizeScale)
        .parent(settingsList)
        .anchorPoint(0.5f, 0.5f)
        .pos(settingsList->getScaledContentSize() / 2.f);

    return true;
}

void CreateRoomPopup::onCheckboxToggled(cocos2d::CCObject* p) {
    auto* btn = static_cast<CCMenuItemToggler*>(p);
    bool state = !btn->isOn();

    switch (p->getTag()) {
#ifdef GLOBED_DEBUG
        case TAG_2P: settingFlags.twoPlayerMode = state; break;
#endif
        case TAG_COLLISION: settingFlags.collision = state; break;
        case TAG_OPEN_INV: settingFlags.publicInvites = state; break;
        case TAG_PRIVATE: settingFlags.isHidden = state; break;
        case TAG_DEATHLINK: settingFlags.deathlink = state; break;
    }
}

CreateRoomPopup* CreateRoomPopup::create(RoomLayer* parent) {
    auto ret = new CreateRoomPopup;
    if (ret->initAnchored(POPUP_WIDTH, POPUP_HEIGHT, parent)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}