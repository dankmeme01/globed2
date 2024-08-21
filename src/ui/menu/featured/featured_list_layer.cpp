#include "featured_list_layer.hpp"

#include <hooks/level_cell.hpp>
#include <hooks/gjgamelevel.hpp>
#include <data/packets/client/general.hpp>
#include <data/packets/server/general.hpp>
#include <managers/error_queues.hpp>
#include <managers/daily_manager.hpp>
#include <net/manager.hpp>
#include <util/ui.hpp>
#include <util/gd.hpp>

using namespace geode::prelude;

bool GlobedFeaturedListLayer::init() {
    if (!CCLayer::init()) return false;

    auto winSize = CCDirector::get()->getWinSize();

    auto listview = Build<ListView>::create(CCArray::create(), 0.f, LIST_WIDTH, LIST_HEIGHT)
        .collect();

    Build<GJListLayer>::create(listview, "", globed::color::Brown, LIST_WIDTH, LIST_HEIGHT, 0)
        .zOrder(2)
        .anchorPoint(0.f, 0.f)
        .parent(this)
        .id("level-list"_spr)
        .store(listLayer);

    auto listTitle = listLayer->getChildByID("title");
    if (!listTitle) {
        listTitle = getChildOfType<CCLabelBMFont>(listLayer, 0);
    }

    auto titlePos = listTitle->getPosition();

    Build<CCSprite>::createSpriteName("icon-featured-label.png"_spr)
        .zOrder(10)
        .pos({titlePos.x, titlePos.y + 4.f})
        .parent(listLayer);

    // refresh button
    Build<CCSprite>::createSpriteName("GJ_updateBtn_001.png")
        .intoMenuItem([this](auto) {
            this->refreshLevels(true);
        })
        .id("btn-refresh")
        .pos(winSize.width - 35.f, 35.f)
        .intoNewParent(CCMenu::create())
        .pos(0.f, 0.f)
        .zOrder(2)
        .parent(this);

    // levels label
    Build<CCLabelBMFont>::create("", "goldFont.fnt")
        .id("level-count-label")
        .pos(winSize.width - 7, winSize.height - 2)
        .scale(0.45f)
        .zOrder(2)
        .anchorPoint({1, 1})
        .parent(this)
        .store(levelsCount);

    constexpr float pageBtnPadding = 20.f;

    // pages buttons
    Build<CCSprite>::createSpriteName("GJ_arrow_03_001.png")
        .intoMenuItem([this](auto) {
            this->currentPage--;
            this->refreshLevels(false);
        })
        .id("btn-prev-page")
        .pos(pageBtnPadding, winSize.height / 2)
        .store(btnPagePrev)
        .intoNewParent(CCMenu::create())
        .id("prev-page-menu")
        .pos(0.f, 0.f)
        .parent(this);

    CCSprite* btnSprite;
    Build<CCSprite>::createSpriteName("GJ_arrow_03_001.png")
        .store(btnSprite)
        .intoMenuItem([this](auto) {
            this->currentPage++;
            this->refreshLevels(false);
        })
        .id("btn-next-page")
        .pos(winSize.width - pageBtnPadding, winSize.height / 2)
        .store(btnPageNext)
        .intoNewParent(CCMenu::create())
        .id("next-page-menu")
        .pos(0.f, 0.f)
        .parent(this);

    btnSprite->setFlipX(true);

    // side art
    geode::addSideArt(this, SideArt::Bottom);

    listLayer->setPosition(winSize / 2 - listLayer->getScaledContentSize() / 2);

    util::ui::prepareLayer(this);

    NetworkManager::get().addListener<LevelPlayerCountPacket>(this, [this](auto packet) {
        this->playerCounts.clear();

        for (auto& [id, count] : packet->levels) {
            this->playerCounts[id] = count;
        }
    });

    this->refreshLevels();
    this->scheduleUpdate();

    return true;
}

GlobedFeaturedListLayer::~GlobedFeaturedListLayer() {
    DailyManager::get().clearMultiWebCallback();
}

void GlobedFeaturedListLayer::update(float dt) {
    if (!listLayer || !listLayer->m_listView) return;

    for (auto entry : CCArrayExt<CCNode*>(listLayer->m_listView->m_tableView->m_cellArray)) {
        auto cell = typeinfo_cast<LevelCell*>(entry);
        static_cast<GlobedLevelCell*>(cell)->updatePlayerCount(playerCounts.contains(cell->m_level->m_levelID) ? playerCounts.at(cell->m_level->m_levelID) : 0);
    }
}

void GlobedFeaturedListLayer::reloadPage() {
    loading = true;

    this->showLoadingUi();

    btnPagePrev->setVisible(false);
    btnPageNext->setVisible(false);

    if (currentPage >= levelPages.size()) {
        this->createLevelList({});
    } else {
        this->createLevelList(levelPages[currentPage]);
    }

    int highest = DailyManager::get().getLastSeenFeaturedLevel();
    int pageMin = currentPage * 10 + 1;
    int pageMax = (currentPage + 1) * 10;
    if (pageMax > highest) pageMax = pageMin + (highest % 10) - 1;

    levelsCount->setString(fmt::format("{} to {} of {}", pageMin, pageMax, highest).c_str());
}

void GlobedFeaturedListLayer::loadListCommon() {
    loading = false;
    this->removeLoadingCircle();
}

void GlobedFeaturedListLayer::removeLoadingCircle() {
    if (loadingCircle) {
        loadingCircle->fadeAndRemove();
        loadingCircle = nullptr;
    }
}

void GlobedFeaturedListLayer::showLoadingUi() {
    if (!loadingCircle) {
        Build<LoadingCircle>::create()
            .pos(0.f, 0.f)
            .store(loadingCircle);

        loadingCircle->setParentLayer(this);
        loadingCircle->show();
    }

    if (listLayer->m_listView) listLayer->m_listView->removeFromParent();
    listLayer->m_listView = Build<ListView>::create(CCArray::create(), 0.f, LIST_WIDTH, LIST_HEIGHT)
        .parent(listLayer)
        .collect();
}

void GlobedFeaturedListLayer::createLevelList(const DailyManager::Page& page) {
    this->loadListCommon();

    std::unordered_map<int, int> levelToRateTier;
    std::vector<LevelId> levelIds;

    CCArray* finalArray = CCArray::create();
    for (auto& entry : page.levels) {
        // if the level is invalid, skip it
        if (!entry.second) continue;

        util::gd::reorderDownloadedLevel(entry.second);

        if (entry.second) {
            finalArray->addObject(entry.second);
        } else {
            log::warn("Skipping missing level: {} (level id {})", entry.first.id, entry.first.levelId);
        }

        levelToRateTier[entry.first.levelId] = entry.first.rateTier;
        levelIds.push_back(entry.first.levelId);
    }

    if (listLayer->m_listView) listLayer->m_listView->removeFromParent();

    listLayer->m_listView = Build<CustomListView>::create(finalArray, BoomListType::Level, LIST_HEIGHT, LIST_WIDTH)
        .parent(listLayer)
        .collect();

    // guys we are about to do a funny
    for (auto* cell : CCArrayExt<GlobedLevelCell*>(listLayer->m_listView->m_tableView->m_contentLayer->getChildren())) {
        cell->modifyToFeaturedCell(levelToRateTier[cell->m_level->m_levelID]);
        if (levelToRateTier.contains(cell->m_level->m_levelID)) {
            static_cast<GlobedLevelCell*>(cell)->m_fields->rateTier = levelToRateTier[cell->m_level->m_levelID];
        }
    }

    // show the buttons
    if (currentPage > 0) {
        btnPagePrev->setVisible(true);
    }

    size_t pageSize = LIST_PAGE_SIZE;

    if (!page.levels.empty() && (lastPage == -1 || currentPage < lastPage)) {
        btnPageNext->setVisible(true);
    }

    NetworkManager::get().sendRequestPlayerCount(levelIds);
}

void GlobedFeaturedListLayer::refreshLevels(bool force) {
    if (loading) return;

    loading = true;
    btnPagePrev->setVisible(false);
    btnPageNext->setVisible(false);

    // remove existing listview and put a loading circle
    this->showLoadingUi();

    auto& dm = DailyManager::get();
    dm.getFeaturedLevels(currentPage, [this](const DailyManager::Page& page) {
        while (levelPages.size() <= currentPage) {
            levelPages.push_back({});
        }

        levelPages[currentPage] = page;

        if (page.isLastPage) {
            lastPage = page.page;
        } else {
            lastPage = page.page + 1;
        }

        this->reloadPage();
    }, force);
}

void GlobedFeaturedListLayer::keyBackClicked() {
    util::ui::navigateBack();
}

GlobedFeaturedListLayer* GlobedFeaturedListLayer::create() {
    auto ret = new GlobedFeaturedListLayer;
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}