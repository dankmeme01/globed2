#include "settings_layer.hpp"

#include "setting_header_cell.hpp"
#include "setting_cell.hpp"
#include <managers/settings.hpp>
#include <util/ui.hpp>
#include <util/cocos.hpp>

using namespace geode::prelude;
using SettingType = GlobedSettingCell::Type;

static constexpr float TAB_SCALE = 0.85f;

bool GlobedSettingsLayer::init() {
    if (!CCLayer::init()) return false;

    this->setID("GlobedSettingsLayer"_spr);

    auto winsize = CCDirector::get()->getWinSize();

    auto* tabButtonMenu = Build<CCMenu>::create()
        .id("tab-button-menu")
        .zOrder(5)
        .layout(RowLayout::create()->setAutoScale(false)->setGap(-1.f))
        .pos(winsize.width / 2, winsize.height / 2 + LIST_HEIGHT / 2 + 26.f)
        .contentSize(LIST_WIDTH + 40.f, 0.f)
        .parent(this)
        .collect();

    // tab buttons
    Build<TabButton>::create("Globed", this, menu_selector(GlobedSettingsLayer::onTab))
        .scale(TAB_SCALE)
        .tag(TAG_TAB_GLOBED)
        .store(tabBtn1)
        .parent(tabButtonMenu);

    Build<TabButton>::create("Menus", this, menu_selector(GlobedSettingsLayer::onTab))
        .scale(TAB_SCALE)
        .tag(TAG_TAB_MENUS)
        .store(tabBtn2)
        .parent(tabButtonMenu);

    Build<TabButton>::create("Voice", this, menu_selector(GlobedSettingsLayer::onTab))
        .scale(TAB_SCALE)
        .tag(TAG_TAB_COMMUNICATION)
        .store(tabBtn3)
        .parent(tabButtonMenu);

    Build<TabButton>::create("Level UI", this, menu_selector(GlobedSettingsLayer::onTab))
        .scale(TAB_SCALE)
        .tag(TAG_TAB_LEVELUI)
        .store(tabBtn4)
        .parent(tabButtonMenu);

    Build<TabButton>::create("Players", this, menu_selector(GlobedSettingsLayer::onTab))
        .scale(TAB_SCALE)
        .tag(TAG_TAB_PLAYERS)
        .store(tabBtn5)
        .parent(tabButtonMenu);

    tabButtonMenu->updateLayout();

    // tabs gradient (copied from geode)

#ifdef GEODE_IS_IOS
    tabsGradientNode = CCNode::create();
#else
    tabsGradientNode = CCClippingNode::create();
#endif
    tabsGradientNode->setID("gradient-clipping-node");
    tabsGradientNode->setContentSize(this->getContentSize());
    tabsGradientNode->setAnchorPoint({0.5f, 0.5f});
    tabsGradientNode->ignoreAnchorPointForPosition(true);
    tabsGradientNode->setZOrder(4);
#ifndef GEODE_IS_IOS
    tabsGradientNode->setInverted(false);
    tabsGradientNode->setAlphaThreshold(0.7f);
#endif

    tabsGradientSprite = CCSprite::createWithSpriteFrameName("tab-gradient.png"_spr);
    tabsGradientSprite->setPosition(tabButtonMenu->getPosition());
    tabsGradientNode->addChild(tabsGradientSprite);

    tabsGradientStencil = CCSprite::createWithSpriteFrameName("tab-gradient-mask.png"_spr);
    tabsGradientStencil->setScale(TAB_SCALE);
    tabsGradientStencil->setAnchorPoint({0.f, 0.f});
#ifndef GEODE_IS_IOS
    tabsGradientNode->setStencil(tabsGradientStencil);
#endif

    this->addChild(tabsGradientNode);
    this->addChild(tabsGradientStencil);

    Build<CCSprite>::createSpriteName("GJ_deleteBtn_001.png")
        .intoMenuItem([this](auto) {
            geode::createQuickPopup("Reset all settings", "Are you sure you want to reset all settings? This action is <cr>irreversible.</c>", "Cancel", "Ok", [this](auto, bool accepted) {
                if (accepted) {
                    GlobedSettings::get().reset();
                    this->remakeList();
                }
            });
        })
        .id("btn-reset")
        .pos(winsize.width - 30.f, 30.f)
        .intoNewParent(CCMenu::create())
        .id("reset-menu")
        .pos(0.f, 0.f)
        .parent(this);

    util::ui::prepareLayer(this);

    this->remakeList();

    return true;
}

void GlobedSettingsLayer::onTab(CCObject* sender) {
    for (const auto& [_, tab] : storedTabs) {
        if (tab->getParent()) {
            tab->removeFromParent();
        }
    }

    if (!storedTabs.contains(sender->getTag())) {
        return;
    }

    currentTab = sender->getTag();
    this->addChild(storedTabs[currentTab]);

    // i stole this from geode
    auto toggleTab = [this](CCMenuItemToggler* member) {
        auto isSelected = member->getTag() == currentTab;
        member->toggle(isSelected);

        if (isSelected && tabsGradientStencil) {
            tabsGradientStencil->setPosition(member->m_onButton->convertToWorldSpace({0.f, 0.f}));
        }
    };

    toggleTab(tabBtn1);
    toggleTab(tabBtn2);
    toggleTab(tabBtn3);
    toggleTab(tabBtn4);
    toggleTab(tabBtn5);

    cocos::handleTouchPriority(this, true);
}

void GlobedSettingsLayer::keyBackClicked() {
    util::ui::navigateBack();
}

void GlobedSettingsLayer::remakeList() {
    if (currentTab != -1 && storedTabs.contains(currentTab)) {
        storedTabs[currentTab]->removeFromParent();
    }

    currentTab = TAG_TAB_GLOBED;

    storedTabs.clear();

    storedTabs[TAG_TAB_GLOBED] = this->makeListLayer(TAG_TAB_GLOBED);
    storedTabs[TAG_TAB_MENUS] = this->makeListLayer(TAG_TAB_MENUS);
    storedTabs[TAG_TAB_COMMUNICATION] = this->makeListLayer(TAG_TAB_COMMUNICATION);
    storedTabs[TAG_TAB_LEVELUI] = this->makeListLayer(TAG_TAB_LEVELUI);
    storedTabs[TAG_TAB_PLAYERS] = this->makeListLayer(TAG_TAB_PLAYERS);

    this->onTab(tabBtn1);
}

template <typename T>
constexpr static GlobedSettingCell::Type getCellType() {
    using Type = GlobedSettingCell::Type;

    if constexpr (std::is_same_v<T, bool>) {
        return Type::Bool;
    } else if constexpr (std::is_same_v<T, float>) {
        return Type::Float;
    } else if constexpr (std::is_same_v<T, int>) {
        return Type::Int;
    } else {
        static_assert(std::is_same_v<T, void>, "invalid type for getCellType");
    }
}

template <typename SetTy>
static void registerSetting(
        CCArray* cells,
        SetTy& setting,
        const char* name,
        const char* description,
        SettingType stype = getCellType<typename SetTy::Type>()
) {
    GlobedSettingCell::Limits limits = {};

    // check for limits
    if constexpr (SetTy::IsLimited) {
        if constexpr (std::is_same_v<typename SetTy::Type, float>) {
            limits.floatMin = SetTy::Minimum;
            limits.floatMax = SetTy::Maximum;
        } else if constexpr (std::is_same_v<typename SetTy::Type, int>) {
            limits.intMin = SetTy::Minimum;
            limits.intMax = SetTy::Maximum;
        }
    }

    cells->addObject(GlobedSettingCell::create(&setting.ref(), stype, name, description, limits));
}

void GlobedSettingsLayer::createSettingsCells(int category) {
    using Type = SettingType;

    settingCells[category] = CCArray::create();

    auto& settings = GlobedSettings::get();
    auto& cat = settingCells[category];

    switch (category) {
        case TAG_TAB_GLOBED: {
            registerSetting(cat, settings.globed.autoconnect, "Autoconnect", "Automatically connect to the last connected server on launch.");
            registerSetting(cat, settings.globed.preloadAssets, "Preload assets", "Increases the loading times but prevents most lagspikes in a level.");
            registerSetting(cat, settings.globed.deferPreloadAssets, "Defer preloading", "Instead of making the loading screen longer, load assets only when you join a level while connected.");
            registerSetting(cat, settings.globed.invitesFrom, "Receive invites from", "Controls who can invite you into a room.", Type::InvitesFrom);
            registerSetting(cat, settings.globed.editorSupport, "View players in editor", "Enables the ability to see people playing your level while in the editor. Note: <cy>this does not let you build levels together!</c>");
            registerSetting(cat, settings.globed.fragmentationLimit, "Packet limit", "Press the \"Test\" button to calibrate the maximum packet size. Should fix some of the issues with players not appearing in a level.", Type::PacketFragmentation);
            registerSetting(cat, settings.globed.tpsCap, "TPS cap", "Maximum amount of packets per second sent between the client and the server. Useful only for very silly things.");

#ifndef GEODE_IS_ANDROID
            registerSetting(cat, settings.globed.useDiscordRPC, "Discord RPC", "If you have the Discord Rich Presence standalone mod, this option will toggle a Globed-specific RPC on your profile.", Type::DiscordRPC);
#endif

            registerSetting(cat, settings.globed.changelogPopups, "Changelog Popup", "After every update, show a <cy>popup</c> detailing all the <cl>changes</c> in the <cg>update</c>.");

#ifdef GLOBED_DEBUG
            // advanced settings button
            registerSetting(cat, settings.globed.autoconnect, "Advanced", "Advanced settings", Type::AdvancedSettings);
#endif
            // link code button
            registerSetting(cat, settings.globed.autoconnect, "Discord link code", "Primarily for boosters/supporters/staff, code for linking an account to a Discord account", Type::LinkCode);
        } break;

        case TAG_TAB_MENUS: {
            registerSetting(cat, settings.globed.increaseLevelList, "More Levels Per Page", "Increases the levels per page in the server level list from 30 to 100.");
            registerSetting(cat, settings.globed.compressedPlayerCount, "Compressed Player Count", "Compress the Player Count label to match the Player Count in The Tower.");
            registerSetting(cat, settings.globed.playerCountOnLvlPage, "Player count on level page", "Show the Player Count label on user-made levels.");
        } break;

        case TAG_TAB_COMMUNICATION: {
#ifdef GLOBED_VOICE_SUPPORT
            registerSetting(cat, settings.communication.voiceEnabled, "Voice chat", "Enables in-game voice chat. To talk, hold V when in a level. (keybind can be changed in game settings)");
            registerSetting(cat, settings.communication.voiceProximity, "Voice proximity", "In platformer mode, the loudness of other players will be determined by how close they are to you.");
            registerSetting(cat, settings.communication.classicProximity, "Classic proximity", "Same as voice proximity, but for classic levels (non-platformer).");
            registerSetting(cat, settings.communication.voiceVolume, "Voice volume", "Controls how loud other players are.");
            registerSetting(cat, settings.communication.onlyFriends, "Only friends", "When enabled, you won't hear players that are not on your friend list in-game.");
            registerSetting(cat, settings.communication.lowerAudioLatency, "Lower audio latency", "Decreases the audio buffer size by 2 times, reducing the latency but potentially causing audio issues.");
            registerSetting(cat, settings.communication.deafenNotification, "Deafen notification", "Shows a notification when you deafen & undeafen.");
            registerSetting(cat, settings.communication.audioDevice, "Audio device", "The input device used for recording your voice.", Type::AudioDevice);
            // MAKE_SETTING(communication, voiceLoopback, "Voice loopback", "When enabled, you will hear your own voice as you speak.");
#endif // GLOBED_VOICE_SUPPORT
        } break;

        case TAG_TAB_LEVELUI: {
            registerSetting(cat, settings.levelUi.progressIndicators, "Progress icons", "Show small icons under the progressbar (or at the edge of the screen in platformer), indicating how far other players are in the level.");
            registerSetting(cat, settings.levelUi.progressOpacity, "Indicator opacity", "Changes the opacity of the icons that represent other players.");
            registerSetting(cat, settings.levelUi.voiceOverlay, "Voice overlay", "Show a small overlay in the bottom right indicating currently speaking players.");
            registerSetting(cat, settings.levelUi.forceProgressBar, "Force Progress Bar", "Enables the progress bar when connected to Globed. Useful if you typically leave it off.");

            this->addHeader(category, "Ping overlay");
            registerSetting(cat, settings.overlay.enabled, "Enabled", "Show a small overlay when in a level, displaying the current latency to the server.");
            registerSetting(cat, settings.overlay.opacity, "Opacity", "Opacity of the displayed overlay.");
            registerSetting(cat, settings.overlay.hideConditionally, "Hide conditionally", "Hide the ping overlay when not connected to a server or in a non-uploaded level, instead of showing a substitute message.");
            registerSetting(cat, settings.overlay.position, "Position", "Position of the overlay on the screen.", Type::Corner);
        } break;

        case TAG_TAB_PLAYERS: {
            registerSetting(cat, settings.players.playerOpacity, "Opacity", "Opacity of other players.");
            registerSetting(cat, settings.players.showNames, "Player names", "Show names above players' icons.");
            registerSetting(cat, settings.players.dualName, "Dual name", "Show the name of the player on their secondary icon as well.");
            registerSetting(cat, settings.players.nameOpacity, "Name opacity", "Opacity of player names.");
            registerSetting(cat, settings.players.ownName, "Show own name", "Shows your own name above your icon as well.");
            registerSetting(cat, settings.players.rotateNames, "Rotate names", "Rotates player names with the camera rotation to keep them easily readable.");
            registerSetting(cat, settings.players.deathEffects, "Death effects", "Play a death effect whenever a player dies.");
            registerSetting(cat, settings.players.defaultDeathEffect, "Default death effect", "Replaces the death effects of all players with a default explosion effect.");
            registerSetting(cat, settings.players.hideNearby, "Hide nearby players", "Increases the transparency of players as they get closer to you, so that they don't obstruct your view.");
            registerSetting(cat, settings.players.statusIcons, "Status icons", "Show an icon above a player if they are paused, in practice mode, or currently speaking.");
            registerSetting(cat, settings.players.hidePracticePlayers, "Hide players in practice", "Hide players that are in practice mode.");
        } break;
    }
}

void GlobedSettingsLayer::addHeader(int category, const char* name) {
    settingCells[category]->addObject(GlobedSettingHeaderCell::create(name));
}

GJListLayer* GlobedSettingsLayer::makeListLayer(int category) {
    this->createSettingsCells(category);

    auto listview = Build<ListView>::create(settingCells[category], GlobedSettingCell::CELL_HEIGHT, LIST_WIDTH, LIST_HEIGHT)
        .collect();

    auto* listLayer = Build<GJListLayer>::create(listview, nullptr, globed::color::Brown, LIST_WIDTH, 220.f, 0)
        .zOrder(6)
        .anchorPoint(0.f, 0.f)
        .id(util::cocos::spr(fmt::format("setting-list-{}", category)).c_str())
        .collect();

    auto winSize = CCDirector::get()->getWinSize();
    listLayer->setPosition({winSize / 2 - listLayer->getScaledContentSize() / 2});

    return listLayer;
}

GlobedSettingsLayer* GlobedSettingsLayer::create() {
    auto ret = new GlobedSettingsLayer;
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}
