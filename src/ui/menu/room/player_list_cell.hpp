#pragma once

#include <defs/geode.hpp>
#include <data/types/gd.hpp>

class GlobedSimplePlayer;

class GLOBED_DLL PlayerListCell : public cocos2d::CCLayer {
public:
    static constexpr float CELL_HEIGHT = 30.0f;

    static PlayerListCell* create(const PlayerRoomPreviewAccountData& data, float cellWidth, bool forInviting, bool isIconLazyLoad);

    bool isIconLoaded();
    void createPlayerIcon();
    void createPlaceholderPlayerIcon();

protected:
    friend class RoomLayer;

    bool init(const PlayerRoomPreviewAccountData& data, float cellWidth, bool forInviting, bool isIconLazyLoad);
    void onOpenProfile(cocos2d::CCObject*);

    void sendInvite();
    void enableInvites();

    void createInviteButton();
    void createJoinButton();
    void createAdminButton();

    PlayerRoomPreviewAccountData playerData;
    float cellWidth;

    cocos2d::CCMenu* rightButtonMenu;
    cocos2d::CCMenu* leftSideLayout;
    CCMenuItemSpriteExtra* joinButton = nullptr;
    CCMenuItemSpriteExtra* inviteButton = nullptr;
    GlobedSimplePlayer* simplePlayer = nullptr;
    cocos2d::CCSprite* placeholderIcon = nullptr;
};
