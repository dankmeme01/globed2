#pragma once

#include <defs/all.hpp>
#include <data/types/misc.hpp>
#include <managers/settings.hpp>
#include <managers/daily_manager.hpp>

class GlobedFeaturedListLayer : public cocos2d::CCLayer {
public:
    static constexpr float LIST_WIDTH = 358.f;
    static constexpr float LIST_HEIGHT = 220.f;
    static constexpr size_t INCREASED_LIST_PAGE_SIZE = 100;
    static constexpr size_t LIST_PAGE_SIZE = 30;

    static GlobedFeaturedListLayer* create();
    ~GlobedFeaturedListLayer();

private:
    cocos2d::CCLabelBMFont* levelsCount;
    GJListLayer* listLayer = nullptr;
    LoadingCircle* loadingCircle = nullptr;
    CCMenuItemSpriteExtra *btnPagePrev = nullptr, *btnPageNext = nullptr;
    std::unordered_map<LevelId, unsigned short> playerCounts;
    std::vector<DailyManager::Page> levelPages;
    int currentPage = 0;
    int lastPage = -1;
    bool loading = false;

    bool init() override;
    void keyBackClicked() override;
    void update(float dt) override;
    void refreshLevels(bool force = false);
    void reloadPage();

    void loadListCommon();
    void removeLoadingCircle();
    void showLoadingUi();

    void createLevelList(const DailyManager::Page& page);
};