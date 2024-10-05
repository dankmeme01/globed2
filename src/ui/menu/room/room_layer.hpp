#pragma once

#include "list_cell.hpp"
#include <defs/geode.hpp>
#include <data/types/gd.hpp>
#include <ui/general/list/list.hpp>

class RoomPlayerListPacket;
class RoomCreatedPacket;
class RoomJoinedPacket;
class RoomInfoPacket;
class RoomInfo;

class GLOBED_DLL RoomLayer : public cocos2d::CCLayer, public LevelDownloadDelegate, public LevelManagerDelegate {
public:
    cocos2d::CCSize popupSize;
    cocos2d::CCSize listSize;
    cocos2d::CCSize targetButtonSize;

    static RoomLayer* create();

protected:
    friend class CreateRoomPopup;

    using PlayerList = GlobedListLayer<ListCellWrapper>;

    std::vector<PlayerRoomPreviewAccountData> playerList;
    std::string currentFilter;

    bool justEntered = true;
    Ref<LoadingCircle> loadingCircle;
    Ref<cocos2d::CCMenu> topRightButtons;
    PlayerList* listLayer;
    Ref<CCMenuItemSpriteExtra> btnSearch, btnClearSearch, btnSettings, btnInvite, btnRefresh, btnCloseRoom, btnPrivacySettings;
    Ref<cocos2d::CCMenu> btnRoomId, roomButtonMenu;
    Ref<ListCellWrapper> roomLevelCell;

    bool init() override;
    void update(float) override;

    // show/hide loading circle
    void startLoading();
    void stopLoading();
    bool isLoading();

    void requestPlayerList();
    void recreatePlayerList();
    void setFilter(std::string_view filter);
    void setRoomTitle(std::string_view name, uint32_t id);
    void resetFilter();
    void closeRoom();

    void addRoomButtons();
    void addGlobalRoomButtons();
    void addCustomRoomButtons();

    // packet handlers
    void onPlayerListReceived(const RoomPlayerListPacket&);
    void onRoomCreatedReceived(const RoomCreatedPacket&);
    void onRoomJoinedReceived(const RoomJoinedPacket&);
    void reloadData(const RoomInfo& info, const std::vector<PlayerRoomPreviewAccountData>& players);

    // callbacks
    void onPrivacySettingsClicked(cocos2d::CCObject*);
    void onCopyRoomId(cocos2d::CCObject*);

    bool shouldRemoveRoomLevel();

    ~RoomLayer();
};
