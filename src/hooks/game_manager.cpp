#include "game_manager.hpp"

#include "level_editor_layer.hpp"
#include "gjbasegamelayer.hpp"
#include "gjgamelevel.hpp"

#include <managers/settings.hpp>
#include <util/debug.hpp>
#include <util/cocos.hpp>

using namespace geode::prelude;

void HookedGameManager::returnToLastScene(GJGameLevel* level) {
    if (GlobedLevelEditorLayer::fromEditor) {
        auto* pl = GlobedGJBGL::get();
        if (pl) pl->onQuitActions();
        GlobedLevelEditorLayer::fromEditor = false;
    }

    auto hooklevel = static_cast<HookedGJGameLevel*>(level);
    if (hooklevel->m_fields->shouldTransitionWithPopScene) {
        CCDirector::get()->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
    } else {
        m_sceneEnum = m_fields->lastSceneEnum;
        GameManager::returnToLastScene(level);
    }
}

CCTexture2D* HookedGameManager::loadIcon(int iconId, int iconType, int iconRequestId) {
    CCTexture2D* texture = this->getCachedIcon(iconId, iconType);

    if (texture) {
        return texture;
    }

    texture = GameManager::loadIcon(iconId, iconType, -1);

    if (texture) {
        m_fields->iconCache[iconType][iconId] = texture;
    }

    if (!texture) {
        log::warn("loadIcon returned a null texture, icon ID = {}, icon type = {}", iconId, iconType);
    }

    return texture;
}

void HookedGameManager::unloadIcon(int iconId, int iconType, int idk) {
    // do nothing.
}

#ifndef GEODE_IS_WINDOWS
void HookedGameManager::loadDeathEffect(int id) {
    util::cocos::tryLoadDeathEffect(id);
}
#endif

void HookedGameManager::reloadAllStep2() {
    this->resetAssetPreloadState();
    GameManager::reloadAllStep2();
}

void HookedGameManager::loadIconsBatched(int iconType, int startId, int endId) {
    auto ranges = std::vector<BatchedIconRange>({BatchedIconRange {
        .iconType = iconType,
        .startId = startId,
        .endId = endId
    }});

    this->loadIconsBatched(ranges);
}

void HookedGameManager::loadIconsBatched(const std::vector<BatchedIconRange>& ranges) {
    std::vector<std::string> toLoad;
    std::unordered_map<int, std::unordered_map<int, size_t>> toLoadMap;

    for (const auto& range : ranges) {
        for (int id = range.startId; id <= range.endId; id++) {
            auto sheetName = this->sheetNameForIcon(id, range.iconType);
            if (sheetName.empty()) continue;

            toLoad.push_back(sheetName);
            toLoadMap[range.iconType][id] = toLoad.size() - 1;
        }
    }

    util::cocos::loadAssetsParallel(toLoad);

    auto* tc = CCTextureCache::sharedTextureCache();

    for (const auto& [iconType, map] : toLoadMap) {
        for (const auto& [iconId, idx] : map) {
            const auto& sheetName = toLoad[idx];

            auto fullpath = util::cocos::fullPathForFilename(fmt::format("{}.png", sheetName));
            if (fullpath.empty()) {
                log::warn("path empty: {}", sheetName);
                continue;
            }

            auto* tex = static_cast<CCTexture2D*>(tc->m_pTextures->objectForKey(fullpath));

            if (!tex) {
                log::warn("icon failed to preload: type {}, id {}", iconType, iconId);
                continue;
            }

            auto* existing = this->getCachedIcon(iconId, iconType);
            if (existing && existing != tex) {
                log::warn("icon already exists, overwriting (id: {}, type: {})", iconId, iconType);
            }

            m_fields->iconCache[iconType][iconId] = tex;
        }
    }
}

CCTexture2D* HookedGameManager::getCachedIcon(int iconId, int iconType) {
    if (m_fields->iconCache.contains(iconType)) {
        if (m_fields->iconCache.at(iconType).contains(iconId)) {
            auto* tex = *m_fields->iconCache.at(iconType).at(iconId);
            return tex;
        }
    }

    return nullptr;
}

bool HookedGameManager::getAssetsPreloaded() {
    return m_fields->assetsPreloaded;
}

void HookedGameManager::setAssetsPreloaded(bool state) {
    m_fields->assetsPreloaded = state;
}

bool HookedGameManager::getDeathEffectsPreloaded() {
    return m_fields->deathEffectsPreloaded;
}

void HookedGameManager::setDeathEffectsPreloaded(bool state) {
    m_fields->deathEffectsPreloaded = state;
}

void HookedGameManager::resetAssetPreloadState() {
    m_fields->iconCache.clear();
    m_fields->loadedFrames.clear();

    this->setAssetsPreloaded(false);
    this->setDeathEffectsPreloaded(false);
    util::cocos::resetPreloadState();
}

void HookedGameManager::setLastSceneEnum(int n) {
    if (n == -1) {
        n = m_sceneEnum;
    }

    m_fields->lastSceneEnum = n;
}

HookedGameManager::Fields* HookedGameManager::fields() {
    // gamemanager is a singleton so this is safe
    static Fields* fields = m_fields.self();
    return fields;
}
