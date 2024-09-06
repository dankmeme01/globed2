#pragma once

#include <vector>
#include <functional>

#include <util/singleton.hpp>
#include <managers/web.hpp>

struct GlobedFeaturedLevel {
    int id;
    int levelId;
    int rateTier;
};

struct GlobedFeaturedLevelPage {
    std::vector<GlobedFeaturedLevel> levels;
    size_t page;
    bool isLastPage;
};

class GLOBED_DLL DailyManager : public SingletonBase<DailyManager> {
public:
    struct Page {
        std::vector<std::pair<GlobedFeaturedLevel, GJGameLevel*>> levels;
        size_t page;
        bool isLastPage;
    };

    // Fetches the level or returns a cached copy
    void getStoredLevel(std::function<void(GJGameLevel*, const GlobedFeaturedLevel&)>&& callback, bool force = false);
    void resetStoredLevel();
    void clearSingleWebCallback();
    void clearMultiWebCallback();
    void clearMetaWebCallback();

    // Fetches the level only from the central server
    void getCurrentLevelMeta(std::function<void(const GlobedFeaturedLevel&)>&& callback, bool force = false);

    // force clears all pages
    void getFeaturedLevels(int page, std::function<void(const Page&)>&& callback, bool force = false);

    void attachRatingSprite(int tier, cocos2d::CCNode* parent);
    cocos2d::CCSprite* createRatingSprite(int tier);
    void attachOverlayToSprite(cocos2d::CCNode* parent);
    GJDifficultySprite* findDifficultySprite(cocos2d::CCNode*);

    int getLastSeenFeaturedLevel();
    void setLastSeenFeaturedLevel(int id);

    int rateTierOpen = -1;

private:
    enum class FetchState {
        NotFetching,
        FetchingId,
        FetchingLevel,
        FetchingFullLevel,
    };

    std::function<void(const GlobedFeaturedLevel&)> levelMetaCallback;
    void onCurrentLevelMetaFetchedCallback(typename WebRequestManager::Event* event);

    // single level fetching

    WebRequestManager::Listener singleReqListener;
    std::function<void(GJGameLevel*, const GlobedFeaturedLevel&)> singleReqCallback;
    FetchState singleFetchState;

    GlobedFeaturedLevel storedLevelMeta;
    geode::Ref<GJGameLevel> storedLevel;

    void onLevelMetaFetchedCallback(typename WebRequestManager::Event* e);
    void onLevelFetchedCallback(geode::Result<cocos2d::CCArray*, int> e);
    void onFullLevelFetchedCallback(geode::Result<GJGameLevel*, int> e);

    // multiple level fetching

    WebRequestManager::Listener multipleReqListener;
    std::function<void(const Page&)> multipleReqCallback;
    FetchState multipleFetchState;
    int multipleFetchPage = 0;

    std::unordered_map<int, Page> storedMultiplePages;

    void onMultipleMetaFetchedCallback(typename WebRequestManager::Event* e);
    void onMultipleFetchedCallback(geode::Result<cocos2d::CCArray*, int> e);
};