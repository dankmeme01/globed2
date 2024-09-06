#pragma once
#include <defs/all.hpp>
#include "player_list_cell.hpp"

class CollapsableLevelCell : public cocos2d::CCNode {
public:
    Ref<GJGameLevel> m_level;

    LevelCell* m_levelCell = nullptr;
    CCNode* m_collapsedCell = nullptr;

    bool m_isCollapsed;

    static constexpr float HEIGHT = 90.f;
    static constexpr float COLLAPSED_HEIGHT = 30.f;

    static CollapsableLevelCell* create(GJGameLevel* level, float width);

    void setIsCollapsed(bool isCollapsed);
    void onOpenLevel(cocos2d::CCObject* sender);

protected:
    bool init(GJGameLevel* level, float width);
};

// a wrapper for both PlayerListCell and CollapsableLevelCell

class ListCellWrapper : public cocos2d::CCNode {
public:
    using CollapsedCallback = std::function<void(bool)>;

    PlayerListCell* playerCell = nullptr;
    CollapsableLevelCell* roomCell = nullptr;
    cocos2d::CCMenu* rightMenu = nullptr;
    CollapsedCallback callback;

    static ListCellWrapper* create(const PlayerRoomPreviewAccountData& data, float cellWidth, bool forInviting, bool isIconLazyLoad);
    bool init(const PlayerRoomPreviewAccountData& data, float cellWidth, bool forInviting, bool isIconLazyLoad);

    static ListCellWrapper* create(GJGameLevel* level, float width, CollapsedCallback&& callback);
    bool init(GJGameLevel* level, float width, CollapsedCallback&& callback);

protected:
    void onCollapse(bool state);
};
