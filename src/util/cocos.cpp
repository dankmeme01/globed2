#include "cocos.hpp"

#include <defs/geode.hpp>
#include <managers/settings.hpp>
#include <hooks/game_manager.hpp>
#include <util/format.hpp>
#include <util/debug.hpp>
#include <asp/thread.hpp>

using namespace geode::prelude;

constexpr size_t THREAD_COUNT = 25;

#define preloadLog(...) preloadLogImpl(fmt::format(__VA_ARGS__))

// all of this is needed to disrespect the privacy of ccfileutils
#include <Geode/modify/CCFileUtils.hpp>
class $modify(HookedFileUtils, CCFileUtils) {
    gd::string& getSearchPath(size_t idx) {
        return m_searchPathArray.at(idx);
    }

    gd::map<gd::string, gd::string>& getFullPathCache() {
        return m_fullPathCache;
    }

    // make these 2 public

    gd::string pGetNewFilename(const char* pfn) {
        return this->getNewFilename(pfn);
    }

    gd::string pGetPathForFilename(const gd::string& filename, const gd::string& resdir, const gd::string& sp) {
        return this->getPathForFilename(filename, resdir, sp);
    }

    static HookedFileUtils& get() {
        return *static_cast<HookedFileUtils*>(CCFileUtils::sharedFileUtils());
    }
};

namespace util::cocos {
    // big hack to call a private cocos function
    namespace {
        template <typename TC>
        using priv_method_t = void(TC::*)(CCDictionary*, CCTexture2D*);

        template <typename TC, priv_method_t<TC> func>
        struct priv_caller {
            friend void _addSpriteFramesWithDictionary(CCDictionary* p1, CCTexture2D* p2) {
                auto* obj = CCSpriteFrameCache::sharedSpriteFrameCache();
                (obj->*func)(p1, p2);
            }
        };

        template struct priv_caller<CCSpriteFrameCache, &CCSpriteFrameCache::addSpriteFramesWithDictionary>;

        void _addSpriteFramesWithDictionary(CCDictionary* p1, CCTexture2D* p2);
    }

    void preloadLogImpl(std::string_view message) {
        static bool enabled = geode::Loader::get()->getLaunchFlag("globed-debug-preload");

        if (enabled) {
            geode::log::debug("preload: {}", message);
        }
    }

    struct PersistentPreloadState {
        TextureQuality texQuality;
        bool hasTexturePack;
        size_t gameSearchPathIdx = -1;
        std::vector<size_t> texturePackIndices;
        std::unique_ptr<asp::ThreadPool> threadPool;
        struct _T {
            util::time::time_point start{}, postPreparation{}, postTexCreation{}, finish{};

            _T() = default;

            void reset() {
                *this = {};
            }

            void print() {
                preloadLog("Preload time estimates:");
                preloadLog("-- Preparation: {}", util::format::duration(postPreparation - start));
                preloadLog("-- Image load + texture creation: {}", util::format::duration(postTexCreation - postPreparation));
                preloadLog("-- Creating sprite frame: {}", util::format::duration(finish - postTexCreation));
                preloadLog("- Total: {}", util::format::duration(finish - start));
            }
        } timeMeasurements;

        void ensurePoolExists() {
            if (!threadPool) {
                threadPool = std::make_unique<asp::ThreadPool>(THREAD_COUNT);
            }
        }

        void destroyPool() {
            threadPool = nullptr;
        }
    };

    static void initPreloadState(PersistentPreloadState& state) {
        auto startTime = util::time::now();

        state.texturePackIndices.clear();

        state.texQuality = getTextureQuality();

        std::filesystem::path resourceDir(geode::dirs::getGameDir() / "Resources");
        std::filesystem::path textureLdrUnzipped(dirs::getGeodeDir() / "unzipped" / "geode.texture-loader" / "resources");

        size_t idx = 0;
        for (const auto& path : CCFileUtils::get()->getSearchPaths()) {
            std::string_view sv(path);
            auto fspath = std::filesystem::path(std::string(path));

            if (sv.find("geode.texture-loader") != std::string::npos) {
                // this might be the unzipped/ folder, if so, ignore it
                std::error_code ec;
                bool result = std::filesystem::equivalent(textureLdrUnzipped, fspath, ec);

                // if the check failed, or if the path isn't equivalent to unzipped, count it as a texture pack folder
                if (ec != std::error_code{} || !result) {
                    state.hasTexturePack = true;
                    state.texturePackIndices.push_back(idx);
                }
            }

#ifdef GEODE_IS_ANDROID
            if (sv == "assets/") {
                state.gameSearchPathIdx = idx;
            }
#elif defined(GEODE_IS_MACOS)
            if (sv.empty()) {
                state.gameSearchPathIdx = idx;
            }
#else

            std::error_code ec;
            bool result = std::filesystem::equivalent(fspath, resourceDir, ec);

            if (ec == std::error_code{} && result) {
                state.gameSearchPathIdx = idx;
            }
#endif

            idx++;
        }

        state.threadPool = std::make_unique<asp::ThreadPool>(THREAD_COUNT);

        preloadLog("initialized preload state in {}", util::format::formatDuration(util::time::now() - startTime));
        preloadLog("texture quality: {}", state.texQuality == TextureQuality::High ? "High" : (state.texQuality == TextureQuality::Medium ? "Medium" : "Low"));
        preloadLog("texture packs: {}", state.texturePackIndices.size());
        preloadLog("game resources path ({}): {}", state.gameSearchPathIdx,
            state.gameSearchPathIdx == -1 ? "<not found>" : HookedFileUtils::get().getSearchPath(state.gameSearchPathIdx));
    }

    void loadAssetsParallel(const std::vector<std::string>& images) {
        auto& state = getPreloadState();
        state.ensurePoolExists();
        state.timeMeasurements.reset();

        auto& threadPool = *state.threadPool.get();

        preloadLog("preparing {} textures", images.size());
        state.timeMeasurements.start = util::time::now();

        static asp::Mutex<> cocosWorkMutex;

        auto textureCache = CCTextureCache::sharedTextureCache();
        auto sfCache  = CCSpriteFrameCache::sharedSpriteFrameCache();

        auto& fileUtils = HookedFileUtils::get();

        struct ImageLoadState {
            std::string key;
            gd::string path;
            CCTexture2D* texture = nullptr;
        };

        asp::Mutex<std::vector<ImageLoadState>> imgStates;

        auto _imguard = imgStates.lock();
        for (const auto& imgkey : images) {
            auto pathKey = fmt::format("{}.png", imgkey);

            gd::string fullpath = fullPathForFilename(pathKey);

            if (fullpath.empty()) {
                continue;
            }

            if (textureCache->m_pTextures->objectForKey(fullpath) != nullptr) {
                continue;
            }

            _imguard->emplace_back(ImageLoadState {
                .key = imgkey,
                .path = fullpath,
                .texture = nullptr
            });
        }

        size_t imgCount = _imguard->size();
        _imguard.unlock();

        if (imgCount == 0) {
            preloadLog("all textures already loaded, skipping pass");
            state.timeMeasurements.finish = util::time::now();
            return;
        }

        preloadLog("loading images ({} total)", imgCount);
        state.timeMeasurements.postPreparation = util::time::now();

        asp::Channel<std::pair<size_t, CCImage*>> textureInitRequests;

        for (size_t i = 0; i < imgCount; i++) {
            threadPool.pushTask([i, &fileUtils, &textureInitRequests, &imgStates] {
                // this is a dangling reference, but we do not modify imgStates in any way, so it's not a big deal.
                auto& imgState = imgStates.lock()->at(i);

                // on android, resources are read from the apk file, so it's NOT thread safe. add a lock.
#ifdef GEODE_IS_ANDROID
                auto _rguard = cocosWorkMutex.lock();
#endif

                unsigned long filesize = 0;
                unsigned char* buffer = fileUtils.getFileData(imgState.path.c_str(), "rb", &filesize);

#ifdef GEODE_IS_ANDROID
                _rguard.unlock();
#endif

                std::unique_ptr<unsigned char[]> buf(buffer);

                if (!buffer || filesize == 0) {
                    log::warn("preload: failed to read image file: {}", imgState.path);
                    return;
                }

                auto* image = new CCImage;
                if (!image->initWithImageData(buf.get(), filesize, cocos2d::CCImage::kFmtPng)) {
                    delete image;
                    log::warn("preload: failed to init image: {}", imgState.path);
                    return;
                }

                textureInitRequests.push(std::make_pair(i, image));
            });
        }

        preloadLog("initializing gl textures");

        // initialize all the textures (must be done on the main thread)

        size_t initedTextures = 0;
        while (true) {
            if (textureInitRequests.empty()) {
                if (threadPool.isDoingWork()) {
                    std::this_thread::yield();
                    continue;
                } else if (textureInitRequests.empty()) {
                    break;
                }
            }

            auto [idx, image] = textureInitRequests.popNow();

            auto texture = new CCTexture2D;
            if (!texture->initWithImage(image)) {
                delete texture;
                image->release();
                log::warn("preload: failed to init CCTexture2D: {}", imgStates.lock()->at(idx).path);
                continue;
            }

            auto imguard = imgStates.lock();
            imguard->at(idx).texture = texture;
            textureCache->m_pTextures->setObject(texture, imguard->at(idx).path);
            imguard.unlock();

            texture->release(); // bring refcount back to 1
            image->release(); // bring refcount to 0, releasing it

            initedTextures++;
        }

        preloadLog("initialized {} textures, adding sprite frames", initedTextures);
        state.timeMeasurements.postTexCreation = util::time::now();

        // now, add sprite frames
        for (size_t i = 0; i < imgCount; i++) {
            // this is the slow code but is essentially equivalent to the code below
            // auto imgState = imgStates.lock()->at(i);
            // auto plistKey = fmt::format("{}.plist", imgState.key);
            // auto fp = CCFileUtils::sharedFileUtils()->fullPathForFilename(plistKey.c_str(), false);
            // sfCache->addSpriteFramesWithFile(fp.c_str());

            threadPool.pushTask([i, textureCache, &imgStates] {
                // this is a dangling reference, but we do not modify imgStates in any way, so it's not a big deal.
                auto& imgState = imgStates.lock()->at(i);

                if (!imgState.texture) return;

                auto plistKey = fmt::format("{}.plist", imgState.key);

                {
                    auto _ = cocosWorkMutex.lock();

                    if (static_cast<HookedGameManager*>(GameManager::get())->fields()->loadedFrames.contains(plistKey)) {
                        preloadLog("already contains, skipping {}", plistKey);
                        return;
                    }
                }

                auto pathsv = std::string_view(imgState.path);
                std::string fullPlistPath = std::string(pathsv.substr(0, pathsv.find(".png"))) + ".plist";

                // file reading is not thread safe on android.

#ifdef GEODE_IS_ANDROID
                auto _ccdlock = cocosWorkMutex.lock();
#endif

                CCDictionary* dict = CCDictionary::createWithContentsOfFileThreadSafe(fullPlistPath.c_str());
                if (!dict) {
                    preloadLog("dict is nullptr for: {}, trying slower fallback option", fullPlistPath);
                    gd::string fallbackPath;
                    {
#ifndef GEODE_IS_ANDROID
                        auto _ = cocosWorkMutex.lock();
#endif
                        fallbackPath = fullPathForFilename(plistKey.c_str());
                    }

                    preloadLog("attempted fallback: {}", fallbackPath);
                    dict = CCDictionary::createWithContentsOfFileThreadSafe(fallbackPath.c_str());
                }

                if (!dict) {
                    log::warn("preload: failed to find the plist for {}.", imgState.path);

#ifndef GEODE_IS_ANDROID
                    auto _ = cocosWorkMutex.lock();
#endif

                    // remove the texture.
                    textureCache->m_pTextures->removeObjectForKey(imgState.path);
                    return;
                }

#ifdef GEODE_IS_ANDROID
                _ccdlock.unlock();
#endif

                {
                    auto _ = cocosWorkMutex.lock();

                    _addSpriteFramesWithDictionary(dict, imgState.texture);
                    static_cast<HookedGameManager*>(GameManager::get())->fields()->loadedFrames.insert(plistKey);
                }

                dict->release();
            });
        }

        // wait for the creation of sprite frames to finish.
        threadPool.join();

        preloadLog("initialized sprite frames. done.");
        state.timeMeasurements.finish = util::time::now();

#ifdef GLOBED_DEBUG
        state.timeMeasurements.print();
#endif
    }

    void preloadAssets(AssetPreloadStage stage) {
        using BatchedIconRange = HookedGameManager::BatchedIconRange;

        preloadLog("preloadAssets stage: {}", (int)stage);

        auto* gm = static_cast<HookedGameManager*>(GameManager::get());

        switch (stage) {
            case AssetPreloadStage::DeathEffect: {
                std::vector<std::string> images;

                for (size_t i = 1; i < 20; i++) {
                    auto key = fmt::format("PlayerExplosion_{:02}", i);
                    images.push_back(key);
                }

                loadAssetsParallel(images);
            } break;
            case AssetPreloadStage::Cube: gm->loadIconsBatched((int)IconType::Cube, 0, 484); break;

            // There are actually 169 ship icons, but for some reason, loading the last icon causes
            // a very strange bug when you have the Default mini icons option enabled.
            // I have no idea how loading a ship icon can cause a ball icon to become a cube,
            // and honestly I don't care enough.
            // https://github.com/dankmeme01/globed2/issues/93
            case AssetPreloadStage::Ship: gm->loadIconsBatched((int)IconType::Ship, 1, 168); break;
            case AssetPreloadStage::Ball: gm->loadIconsBatched((int)IconType::Ball, 0, 118); break;
            case AssetPreloadStage::Ufo: gm->loadIconsBatched((int)IconType::Ufo, 1, 149); break;
            case AssetPreloadStage::Wave: gm->loadIconsBatched((int)IconType::Wave, 1, 96); break;
            case AssetPreloadStage::Other: {
                std::vector<BatchedIconRange> ranges = {
                    BatchedIconRange{
                        .iconType = (int)IconType::Robot,
                        .startId = 1,
                        .endId = 68
                    },
                    BatchedIconRange{
                        .iconType = (int)IconType::Spider,
                        .startId = 1,
                        .endId = 69
                    },
                    BatchedIconRange{
                        .iconType = (int)IconType::Swing,
                        .startId = 1,
                        .endId = 43
                    },
                    BatchedIconRange{
                        .iconType = (int)IconType::Jetpack,
                        .startId = 1,
                        .endId = 5
                    },
                };

                gm->loadIconsBatched(ranges);

            } break;
            case AssetPreloadStage::AllWithoutDeathEffects: [[fallthrough]];
            case AssetPreloadStage::All: {
                if (stage != AssetPreloadStage::AllWithoutDeathEffects) {
                    preloadAssets(AssetPreloadStage::DeathEffect);
                }
                preloadAssets(AssetPreloadStage::Cube);
                preloadAssets(AssetPreloadStage::Ship);
                preloadAssets(AssetPreloadStage::Ball);
                preloadAssets(AssetPreloadStage::Ufo);
                preloadAssets(AssetPreloadStage::Wave);
                preloadAssets(AssetPreloadStage::Other);
            } break;
        }
    }

    bool forcedSkipPreload() {
        auto& settings = GlobedSettings::get();

        return !settings.globed.preloadAssets || Loader::get()->getLaunchFlag("globed-skip-preload") || Mod::get()->getSettingValue<bool>("force-skip-preload");
    }

    bool shouldTryToPreload(bool onLoading) {
        // if preloading is completely disabled, always return false
        if (forcedSkipPreload()) return false;

        auto* gm = static_cast<HookedGameManager*>(GameManager::get());

        // if already loaded, don't try again
        if (gm->getAssetsPreloaded()) {
            return false;
        }

        auto& settings = GlobedSettings::get();

        // if we are on the loading screen, only load if not deferred
        if (onLoading) {
            return !settings.globed.deferPreloadAssets;
        }

        // if we are in a level and they haven't been loaded yet, load them
        return true;
    }

    TextureQuality getTextureQuality() {
        float sf = CCDirector::get()->getContentScaleFactor();
        if (sf >= 4.f) {
            return TextureQuality::High;
        } else if (sf >= 2.f) {
            return TextureQuality::Medium;
        } else {
            return TextureQuality::Low;
        }
    }

    PersistentPreloadState& getPreloadState() {
        static PersistentPreloadState state;
        util::misc::callOnce("cocos-get-preload-state-init", [&] {
            initPreloadState(state);
        });

        return state;
    }

    void resetPreloadState() {
        auto& state = getPreloadState();
        initPreloadState(state);
    }

    void cleanupThreadPool() {
        getPreloadState().destroyPool();
    }

    // transforms a string like "icon-41" into "icon-41-hd.png" depending on the current texture quality.
    static void appendQualitySuffix(std::string& out, TextureQuality quality, bool plist) {
        switch (quality) {
            case TextureQuality::Low: {
                if (plist) out.append(".plist");
                else out.append(".png");
            } break;
            case TextureQuality::Medium: {
                if (plist) out.append("-hd.plist");
                else out.append("-hd.png");
            } break;
            case TextureQuality::High: {
                if (plist) out.append("-uhd.plist");
                else out.append("-uhd.png");
            } break;
        }
    }

    // slightly faster rewrite of ccfileutils::fullPathForFilename
    gd::string fullPathForFilename(const std::string_view rawfilename) {
        auto& fileUtils = HookedFileUtils::get();
        auto& pstate = getPreloadState();

        // add the quality suffix.
        std::string filename;

        if (rawfilename.ends_with(".plist")) {
            filename = rawfilename.substr(0, rawfilename.find(".plist"));
            appendQualitySuffix(filename, pstate.texQuality, true);
        } else {
            filename = rawfilename.substr(0, rawfilename.find(".png"));
            appendQualitySuffix(filename, pstate.texQuality, false);
        }

        gd::string filenameGd(filename);

        // if absolute path, return it
        if (fileUtils.isAbsolutePath(filenameGd)) {
            return filename;
        }

        // if cached, return the cached path
        auto cachedIt = fileUtils.getFullPathCache().find(filenameGd);
        if (cachedIt != fileUtils.getFullPathCache().end()) {
            return cachedIt->second;
        }

        // Get the new file name.
        gd::string newFilename = fileUtils.pGetNewFilename(filename.c_str());

        std::string fullpath = "";

        const auto& searchPaths = fileUtils.getSearchPaths();
# define TRY_PATH(sp) \
    auto _fp = getPathForFilename(newFilename, sp); \
    if (!_fp.empty()) { \
        fileUtils.getFullPathCache().insert(std::make_pair(std::move(filenameGd), _fp)); \
        return _fp; \
    }

        // check for texture pack overrides
        for (size_t tpidx : pstate.texturePackIndices) {
            const auto& spath = searchPaths.at(tpidx);
            TRY_PATH(spath);
        }

        // try the gd folder
        if (pstate.gameSearchPathIdx != -1) {
            const auto& spath = searchPaths.at(pstate.gameSearchPathIdx);
            TRY_PATH(spath);
        }

        log::warn("fullPathForFilename missed known paths, trying everything: {}", rawfilename);

        // try every single search path
        for (const auto& spath : searchPaths) {
            TRY_PATH(spath);
        }

        // if all else fails, accept our defeat.
        log::warn("fullPathForFilename failed to find full path for: {}", rawfilename);
        log::warn("attempted transformed path was {}", newFilename);

        return filenameGd;
    }

    gd::string getPathForFilename(const gd::string& filename, const gd::string& searchPath) {
        // i dont wanna deal with utf8 conversions lol
        return HookedFileUtils::get().pGetPathForFilename(filename, "", searchPath);
        // std::string file(filename);
        // std::string file_path;
        // size_t slashpos = filename.find_last_of("/");
        // if (slashpos != std::string::npos) {
        //     file_path = filename.substr(0, slashpos + 1);
        //     file = filename.substr(slashpos + 1);
        // }

        // std::string path = std::string(searchPath) + file_path + file;

        // if (std::filesystem::exists(path)) {
        //     return path;
        // }

        // return "";
    }

    CCTexture2D* textureFromSpriteName(std::string_view name) {
        auto fullPath = fullPathForFilename(name);
        if (fullPath.empty()) {
            return nullptr;
        }

        auto pathKey = fmt::format("globed-cocos-{}", fullPath);

        auto* tex = static_cast<CCTexture2D*>(CCTextureCache::get()->m_pTextures->objectForKey(pathKey));
        if (!tex) {
            std::string lowercase(fullPath);
            std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(), ::tolower);

            auto img = new CCImage();
            img->initWithImageFile(fullPath.c_str(), CCImage::kFmtPng);

            tex = new CCTexture2D();
            if (tex->initWithImage(img)) {
                CCTextureCache::get()->m_pTextures->setObject(tex, pathKey);
                tex->release();
            }
        }

        return tex;
    }

    CCTexture2D* addTextureFromData(const std::string& textureKey, unsigned char* data, size_t size) {
        auto textureCache = CCTextureCache::get();
        if (auto tex = textureCache->textureForKey(textureKey.c_str())) {
            return tex;
        }

        auto& fileUtils = HookedFileUtils::get();

        if (!data || size == 0) {
            return nullptr;
        }

        auto* image = new CCImage;
        if (!image->initWithImageData(data, size, cocos2d::CCImage::kFmtPng)) {
            delete image;
            log::warn("failed to init image");
            return nullptr;
        }

        auto texture = new CCTexture2D;
        if (!texture->initWithImage(image)) {
            delete texture;
            image->release();
            log::warn("failed to init CCTexture2D");
            return nullptr;
        }

        CCTextureCache::get()->m_pTextures->setObject(texture, textureKey);

        return texture;
    }

    std::string spr(const std::string_view s) {
        static const std::string id = Mod::get()->getID() + "/";

        std::string out(id);
        out.append(s);

        return out;
    }

    bool isValidSprite(CCNode* obj) {
        return obj && !obj->getUserObject("geode.texture-loader/fallback");
    }

    void renderNodeToFile(CCNode* node, const std::filesystem::path& dest) {
        auto tex = CCRenderTexture::create(node->getScaledContentWidth(), node->getScaledContentHeight());
        tex->beginWithClear(0, 0, 0, 0);
        node->visit();
        tex->draw();
        tex->end();

        // idk if this leaks
        auto img = tex->newCCImage();
        img->saveToFile(dest.string().c_str());
    }

    void tryLoadDeathEffect(int id) {
        if (id <= 1) return;

        auto textureCache = CCTextureCache::sharedTextureCache();
        auto sfCache  = CCSpriteFrameCache::sharedSpriteFrameCache();

        auto pngKey = fmt::format("PlayerExplosion_{:02}.png", id - 1);
        auto plistKey = fmt::format("PlayerExplosion_{:02}.plist", id - 1);

        if (textureCache->textureForKey(pngKey.c_str()) == nullptr) {
            textureCache->addImage(pngKey.c_str(), false);
            sfCache->addSpriteFramesWithFile(plistKey.c_str());
        }
    }
}