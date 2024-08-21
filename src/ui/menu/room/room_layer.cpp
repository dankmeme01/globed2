#include "room_layer.hpp"

#include "room_settings_popup.hpp"
#include "room_password_popup.hpp"
#include "room_join_popup.hpp"
#include "room_listing_popup.hpp"
#include "invite_popup.hpp"
#include "create_room_popup.hpp"
#include "player_list_cell.hpp"
#include <data/packets/server/room.hpp>
#include <data/packets/client/admin.hpp>
#include <net/manager.hpp>
#include <managers/admin.hpp>
#include <managers/error_queues.hpp>
#include <managers/profile_cache.hpp>
#include <managers/friend_list.hpp>
#include <managers/settings.hpp>
#include <managers/room.hpp>
#include <ui/general/ask_input_popup.hpp>
#include <ui/ui.hpp>
#include <util/ui.hpp>
#include <util/gd.hpp>
#include <util/cocos.hpp>
#include <util/misc.hpp>
#include <util/format.hpp>
#include <util/math.hpp>

#include <util/debug.hpp>

using namespace geode::prelude;

// order of buttons
namespace {
    namespace btnorder {
        constexpr int Invisibility = 3;
        constexpr int Search = 4;
        constexpr int ClearSearch = 5;
        constexpr int Invite = 6;
        constexpr int Refresh = 100; // dead last
    }
}

bool RoomLayer::init() {
    if (!CCLayer::init()) return false;

    auto& nm = NetworkManager::get();
    if (!nm.established()) {
        return false;
    }

    // reload friendlist if not loaded yet
    FriendListManager::get().maybeLoad();

    auto winSize = CCDirector::get()->getWinSize();
    popupSize = CCSize{util::ui::capPopupWidth(420.f), 280.f};
    listSize = CCSize{popupSize.width * 0.8f, 180.f};

    this->setContentSize(popupSize);
    this->ignoreAnchorPointForPosition(false);

    auto rlayout = util::ui::getPopupLayoutAnchored(popupSize);

    // blue background
    Build(CCScale9Sprite::create("GJ_square02.png", {0, 0, 80, 80}))
        .zOrder(-1)
        .contentSize(popupSize)
        .pos(rlayout.center)
        .parent(this);

    // player list
    Build<PlayerList>::create(listSize.width, listSize.height, globed::color::DarkBlue, 0.f, GlobedListBorderType::GJCommentListLayerBlue)
        .anchorPoint(0.5f, 1.f)
        .pos(rlayout.fromTop(40.f))
        .id("player-list")
        .parent(this)
        .store(listLayer);

    listLayer->setCellColors(globed::color::DarkerBlue, globed::color::DarkBlue);

    const float sidePadding = (popupSize.width - listSize.width) / 2.f;

    // buttons in top right
    Build<CCMenu>::create()
        .layout(ColumnLayout::create()->setGap(1.f)->setAxisAlignment(AxisAlignment::End)->setAxisReverse(true))
        .scale(0.875f)
        .anchorPoint(0.5f, 1.f)
        .pos(rlayout.right - sidePadding / 2.f, rlayout.top - 6.f)
        .contentSize(30.f, popupSize.height)
        .parent(this)
        .id("top-right-btns")
        .store(topRightButtons);

    // this is grand
    float buttonSize = util::math::min(32.5f, sidePadding - 5.f);
    this->targetButtonSize = CCSize{buttonSize, buttonSize};

    // invisibility button
    auto* invisSprite = CCSprite::createWithSpriteFrameName("status-invisible.png"_spr);
    auto* visSprite = CCSprite::createWithSpriteFrameName("status-visible.png"_spr);
    util::ui::rescaleToMatch(invisSprite, targetButtonSize);
    util::ui::rescaleToMatch(visSprite, targetButtonSize);

    Build<CCMenuItemToggler>(CCMenuItemToggler::create(invisSprite, visSprite, this, menu_selector(RoomLayer::onInvisibleClicked)))
        .id("btn-invisible")
        .parent(topRightButtons)
        .zOrder(btnorder::Invisibility)
        .store(btnInvisible);

    btnInvisible->m_offButton->m_scaleMultiplier = 1.1f;
    btnInvisible->m_onButton->m_scaleMultiplier = 1.1f;

    btnInvisible->toggle(!GlobedSettings::get().globed.isInvisible);

    // search button
    Build<CCSprite>::createSpriteName("gj_findBtn_001.png")
        .with([&](auto* btn) {
            util::ui::rescaleToMatch(btn, targetButtonSize);
        })
        .intoMenuItem([this](auto) {
            AskInputPopup::create("Search Player", [this](const std::string_view input) {
                if (this->isLoading()) return;

                this->setFilter(input);
                this->recreatePlayerList();
            }, 16, "Username", util::misc::STRING_ALPHANUMERIC, 3.f)->show();
        })
        .zOrder(btnorder::Search)
        .scaleMult(1.1f)
        .id("btn-search")
        .parent(topRightButtons)
        .store(btnSearch);

    // clear search button
    Build<CCSprite>::createSpriteName("gj_findBtnOff_001.png")
        .with([&](auto* btn) {
            util::ui::rescaleToMatch(btn, targetButtonSize);
        })
        .intoMenuItem([this](auto) {
            if (this->isLoading()) return;

            this->resetFilter();
            this->recreatePlayerList();
        })
        .zOrder(btnorder::ClearSearch)
        .scaleMult(1.1f)
        .id("search-clear-btn")
        .store(btnClearSearch);

    // invite button
    Build<CCSprite>::createSpriteName("icon-invite-menu.png"_spr)
        .with([this](CCSprite* spr) {
            util::ui::rescaleToMatch(spr, targetButtonSize);
        })
        .intoMenuItem([this](auto) {
            if (this->isLoading()) return;

            InvitePopup::create()->show();
        })
        .zOrder(btnorder::Invite)
        .scaleMult(1.15f)
        .id("invite-btn"_spr)
        .parent(topRightButtons)
        .store(btnInvite);

    // refresh button
    Build<CCSprite>::createSpriteName("icon-refresh-square.png"_spr)
        .with([&](auto* btn) {
            util::ui::rescaleToMatch(btn, targetButtonSize);
        })
        .intoMenuItem([this](auto) {
            if (this->isLoading()) return;

            this->requestPlayerList();
        })
        .zOrder(btnorder::Refresh)
        .scaleMult(1.1f)
        .id("btn-reload")
        .parent(topRightButtons)
        .store(btnRefresh);

    topRightButtons->updateLayout();

    // settings button
    auto* bottomLeftMenu = Build<CCSprite>::createSpriteName("accountBtn_settings_001.png")
        .scale(0.7f)
        .intoMenuItem([this](auto) {
            if (this->isLoading()) return;

            RoomSettingsPopup::create()->show();
        })
        .scaleMult(1.1f)
        .id("btn-settings")
        .visible(false)
        .store(btnSettings)
        .intoNewParent(CCMenu::create())
        .anchorPoint(0.f, 0.f)
        .layout(RowLayout::create()->setGap(3.f)->setAxisAlignment(AxisAlignment::Start))
        .id("bottom-left-menu")
        .pos(rlayout.bottomLeft + CCPoint{7.5f, 7.5f})
        .parent(this)
        .collect();

    // close room button
    Build<CCSprite>::createSpriteName("icon-close-room.png"_spr)
        .scale(0.7f)
        .intoMenuItem([this](auto) {
            geode::createQuickPopup("Close Room?", "Are you sure you want to close this room?", "Cancel", "Ok", [this](auto, bool btn2) {
                if (btn2) {
                    this->closeRoom();
                }
            });
        })
        .scaleMult(1.1f)
        .id("btn-close-room")
        .visible(false)
        .store(btnCloseRoom)
        .parent(bottomLeftMenu);

    bottomLeftMenu->updateLayout();

    // listeners

    nm.addListener<RoomPlayerListPacket>(this, [this](auto packet) {
        this->onPlayerListReceived(*packet);
    });

    nm.addListener<RoomCreatedPacket>(this, [this](auto packet) {
        this->onRoomCreatedReceived(*packet);
    });

    nm.addListener<RoomJoinedPacket>(this, [this](auto packet) {
        this->onRoomJoinedReceived(*packet);
    });

    nm.addListener<RoomCreateFailedPacket>(this, [this](auto packet) {
        ErrorQueues::get().error(fmt::format("Failed to create room: <cy>{}</c>", packet->reason));
        this->stopLoading();
    });

    this->requestPlayerList();

    this->scheduleUpdate();

    return true;
}

void RoomLayer::update(float dt) {
    auto& rm = RoomManager::get();
    auto& am = AdminManager::get();

    btnSettings->setVisible(rm.isInRoom());
    btnCloseRoom->setVisible(rm.isInRoom() && (rm.isOwner() || am.getRole().canModerate()));

    // show/hide the invite button
    bool canInvite = rm.isInRoom() && (rm.isOwner() || rm.getInfo().settings.flags.publicInvites);
    bool isShown = btnInvite->getParent() != nullptr;

    if (isShown != canInvite) {
        if (canInvite) {
            topRightButtons->addChild(btnInvite);
        } else {
            btnInvite->removeFromParent();
        }

        topRightButtons->updateLayout();
    }

    // lazy load the player icons
    if (!this->isLoading()) {
        size_t loaded = 0;
        constexpr size_t perFrame = 10;

        for (auto cell : *listLayer) {
            if (cell->playerCell == nullptr || cell->roomCell != nullptr) continue;
            auto playerCell = cell->playerCell;
            if (!playerCell->isIconLoaded()) {
                playerCell->createPlayerIcon();
                loaded++;
            }

            if (loaded == perFrame) break;
        }
    }

    // check for the room level
    auto level = RoomManager::get().getRoomLevel();

    // only change if the levelcell has not been created, or the level has changed.
    if (level && (!roomLevelCell || roomLevelCell->roomCell->m_level->m_levelID != level->m_levelID)) {
        if (roomLevelCell) {
            listLayer->removeCell(roomLevelCell);
            roomLevelCell = nullptr;
        }

        Build<ListCellWrapper>::create(level, listSize.width, [this](bool) {
            listLayer->forceUpdate();
        }).store(roomLevelCell);

        listLayer->insertCell(roomLevelCell, 0);
        listLayer->forceUpdate();
    }
}

void RoomLayer::requestPlayerList() {
    auto& nm = NetworkManager::get();

    if (!nm.established()) {
        return;
    }

    this->startLoading();

    nm.sendRequestRoomPlayerList();
}

void RoomLayer::recreatePlayerList() {
    int previousCellCount = listLayer->cellCount();
    auto scrollPos = listLayer->getScrollPos();

    listLayer->removeAllCells();

    // room level is first cell always
    if (roomLevelCell) {
        listLayer->addCell(roomLevelCell.data());
    }

    auto selfId = GJAccountManager::get()->m_accountID;
    auto filter = util::format::toLowercase(currentFilter);

    std::vector<PlayerRoomPreviewAccountData> unsortedData;
    for (const auto& data : playerList) {
        // filtering
        if (data.accountId <= 0 || data.accountId == selfId) {
            continue;
        }

        if (!filter.empty()) {
            auto name = util::format::toLowercase(data.name);
            if (name.find(filter) == std::string::npos) {
                continue;
            }
        }

        unsortedData.push_back(data);
        // listLayer->addCellFast(data, listSize.width, false, true);
    }

    // we always come first (or second if there is a room level)
    listLayer->addCellFast(ProfileCacheManager::get().getOwnAccountData().makeRoomPreview(0), listSize.width, false, true);

    // inefficient algoritms go!

    // first, just sort the player list
    auto& flm = FriendListManager::get();
    std::sort(unsortedData.begin(), unsortedData.end(), [&flm](auto& a, auto& b) {
        // force friends at the top
        bool isFriend1 = flm.isFriend(a.accountId);
        bool isFriend2 = flm.isFriend(b.accountId);

        if (isFriend1 != isFriend2) {
            return isFriend1;
        } else {
            // convert both names to lowercase
            std::string name1 = a.name, name2 = b.name;
            std::transform(name1.begin(), name1.end(), name1.begin(), ::tolower);
            std::transform(name2.begin(), name2.end(), name2.begin(), ::tolower);

            // sort alphabetically
            return name1 < name2;
        }
    });

    // if in a global room, shuffle (except friends)!
    bool randomize = RoomManager::get().isInGlobal();

    if (randomize) {
        // find first non-friend element
        decltype(unsortedData)::iterator firstNonFriend = unsortedData.end();

        for (auto it = unsortedData.begin(); it != unsortedData.end(); it++) {
            if (!flm.isFriend(it->accountId)) {
                firstNonFriend = it;
                break;
            }
        }

        // shuffle everything afterwards
        std::shuffle(firstNonFriend, unsortedData.end(), util::rng::Random::get().getEngine());
    }

    for (auto& data : unsortedData) { // really its  sorted by now
        listLayer->addCellFast(data, listSize.width, false, true);
    }

    listLayer->forceUpdate();

    // for prettiness, load first 10 icons
    for (size_t i = 0; i < util::math::min(listLayer->cellCount(), 10); i++) {
        auto cell = listLayer->getCell(i);
        if (cell->playerCell) cell->playerCell->createPlayerIcon();
    }

    listLayer->scrollToPos(scrollPos);
}

void RoomLayer::setFilter(std::string_view filter) {
    this->currentFilter = filter;

    if (filter.empty()) {
        if (btnClearSearch->getParent()) btnClearSearch->removeFromParent();
    } else {
        if (!btnClearSearch->getParent()) {
            topRightButtons->addChild(btnClearSearch);
        }
    }

    topRightButtons->updateLayout();
}

void RoomLayer::resetFilter() {
    this->setFilter("");

    // re-add the cell if it was created before
    if (roomLevelCell) {
        listLayer->insertCell(roomLevelCell, 0);
        listLayer->forceUpdate();
    }
}

void RoomLayer::onPlayerListReceived(const RoomPlayerListPacket& packet) {
    this->reloadData(packet.info, packet.players);
}

void RoomLayer::onRoomCreatedReceived(const RoomCreatedPacket& packet) {
    this->reloadData(packet.info, {});
}

void RoomLayer::onRoomJoinedReceived(const RoomJoinedPacket& packet) {
    this->reloadData(packet.info, packet.players);

    // if we have any room listing popup opened, close them

    auto nodes = CCScene::get()->getChildren();
    for (int i = nodes->count() - 1; i >= 0; i--) {
        auto node = static_cast<CCNode*>(nodes->objectAtIndex(i));
        if (typeinfo_cast<RoomPasswordPopup*>(node) || typeinfo_cast<RoomListingPopup*>(node) || typeinfo_cast<RoomJoinPopup*>(node)) {
            node->removeFromParent();
        }
    }
}

void RoomLayer::reloadData(const RoomInfo& info, const std::vector<PlayerRoomPreviewAccountData>& players) {
    this->stopLoading();

    // if room is empty, means we just created it ourselves
    if (players.empty()) {
        auto& pcm = ProfileCacheManager::get();
        auto ownData = pcm.getOwnData();
        auto ownSpecial = pcm.getOwnSpecialData();

        auto* gjam = GJAccountManager::get();

        this->playerList = std::vector({
            PlayerRoomPreviewAccountData(
                gjam->m_accountID,
                GameManager::get()->m_playerUserID.value(),
                gjam->m_username,
                PlayerIconDataSimple(ownData),
                0,
                ownSpecial
            )
        });
    } else {
        this->playerList = players;
    }

    auto& rm = RoomManager::get();

    bool changedRoom = rm.getId() != info.id;
    rm.setInfo(info);

    // if we are not refreshing, but rather joining/leaving/creating a room, reset the filter
    if (changedRoom) {
        this->resetFilter();
    }

    // create the player list
    this->recreatePlayerList();

    // create buttons and room title
    if (changedRoom || justEntered) {
        justEntered = false;

        this->setRoomTitle(rm.getInfo().name, rm.getId());
        this->addRoomButtons();

        // if we left the room, clear the pinned level
        if (this->shouldRemoveRoomLevel()) {
            listLayer->removeCell(roomLevelCell);
            roomLevelCell = nullptr;
        }
    }

    topRightButtons->updateLayout();
}

void RoomLayer::closeRoom() {
    if (this->isLoading()) return;

    NetworkManager::get().sendCloseRoom();

    this->startLoading();
}

void RoomLayer::setRoomTitle(std::string_view name, uint32_t id) {
    if (btnRoomId) btnRoomId->removeFromParent();

    std::string title = (id == 0) ? "Global Room" : fmt::format("{} ({})", name, id);

    auto rlayout = util::ui::getPopupLayoutAnchored(popupSize);

    Build<CCLabelBMFont>::create(title.c_str(), "goldFont.fnt")
        .limitLabelWidth(listSize.width, 0.7f, 0.2f)
        .intoMenuItem(this, menu_selector(RoomLayer::onCopyRoomId))
        .scaleMult(1.1f)
        .intoNewParent(CCMenu::create())
        .id("title-menu")
        .pos(rlayout.fromTop(17.f))
        .parent(this)
        .store(btnRoomId);
}

// adds room join/create/leave buttons
void RoomLayer::addRoomButtons() {
    if (roomButtonMenu) roomButtonMenu->removeFromParent();

    auto& rm = RoomManager::get();

    if (rm.isInGlobal()) {
        this->addGlobalRoomButtons();
    } else {
        this->addCustomRoomButtons();
    }

    roomButtonMenu->updateLayout();
}

void RoomLayer::addGlobalRoomButtons() {
    auto rlayout = util::ui::getPopupLayoutAnchored(popupSize);

    Build<CCMenu>::create()
        .layout(
            RowLayout::create()
                ->setAxisAlignment(AxisAlignment::Center)
                ->setGap(5.0f)
        )
        .contentSize(listSize.width, 0.f)
        .id("btn-menu")
        .pos(rlayout.fromBottom(30.f))
        .parent(this)
        .store(roomButtonMenu);

    // join room button
    Build<ButtonSprite>::create("Join room", "bigFont.fnt", "GJ_button_01.png", 0.8f)
        .intoMenuItem([this](auto) {
            if (this->isLoading()) return;

            RoomListingPopup::create()->show();
        })
        .scaleMult(1.05f)
        .id("join-room-btn")
        .parent(roomButtonMenu)
        .collect();

    // create room button
    Build<ButtonSprite>::create("Create room", "bigFont.fnt", "GJ_button_01.png", 0.8f)
        .intoMenuItem([this](auto) {
            if (this->isLoading()) return;

            CreateRoomPopup::create(this)->show();
        })
        .scaleMult(1.05f)
        .id("create-room-btn")
        .parent(roomButtonMenu)
        .collect();
}

void RoomLayer::addCustomRoomButtons() {
    auto rlayout = util::ui::getPopupLayoutAnchored(popupSize);

    Build<CCMenu>::create()
        .id("btn-menu")
        .pos(rlayout.center.width, rlayout.bottom + 30.f)
        .parent(this)
        .store(roomButtonMenu);

    Build<ButtonSprite>::create("Leave room", "bigFont.fnt", "GJ_button_01.png", 0.8f)
        .intoMenuItem([this](auto) {
            if (this->isLoading()) return;

            NetworkManager::get().sendLeaveRoom();
            this->startLoading();
        })
        .scaleMult(1.1f)
        .id("leave-room-btn")
        .parent(roomButtonMenu)
        .collect();
}

// show loading circle
void RoomLayer::startLoading() {
    // remove it just in case there was already one
    this->stopLoading();

    Build<LoadingCircle>::create()
        .ignoreAnchorPointForPos(false)
        .pos(listLayer->getScaledContentSize() / 2.f)
        .store(loadingCircle);

    loadingCircle->setParentLayer(listLayer);
    loadingCircle->show();
}

// hide loading circle
void RoomLayer::stopLoading() {
    if (loadingCircle) {
        loadingCircle->removeFromParent();
        loadingCircle = nullptr;
    }
}

bool RoomLayer::isLoading() {
    return loadingCircle != nullptr;
}

void RoomLayer::onInvisibleClicked(CCObject*) {
    GlobedSettings& settings = GlobedSettings::get();

    if (!settings.flags.seenStatusNotice) {
        settings.flags.seenStatusNotice = true;

        FLAlertLayer::create(
            "Invisibility",
            "This button toggles whether you want to be visible on the global player list or not. You will still be visible to other players on the same level as you.",
            "OK"
        )->show();
    }

    bool invisible = btnInvisible->isOn();
    settings.globed.isInvisible = invisible;

    NetworkManager::get().sendUpdatePlayerStatus(settings.getPrivacyFlags());
}

void RoomLayer::onCopyRoomId(CCObject*) {
    auto id = RoomManager::get().getId();

    utils::clipboard::write(std::to_string(id));

    Notification::create("Copied room ID to clipboard", NotificationIcon::Success)->show();
}

bool RoomLayer::shouldRemoveRoomLevel() {
    if (!roomLevelCell) return false;

    auto level = RoomManager::get().getRoomLevel();

    if (!level) return true;

    return roomLevelCell->roomCell->m_level->m_levelID != level->m_levelID;
}

RoomLayer::~RoomLayer() {
    auto* glm = GameLevelManager::sharedState();
    glm->m_levelManagerDelegate = nullptr;
    glm->m_levelDownloadDelegate = nullptr;
}

RoomLayer* RoomLayer::create() {
    auto ret = new RoomLayer();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}
