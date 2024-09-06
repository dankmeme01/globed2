#include "gjbasegamelayer.hpp"

#if GLOBED_HAS_KEYBINDS
# include <geode.custom-keybinds/include/Keybinds.hpp>
#endif // GLOBED_HAS_KEYBINDS

#include <Geode/loader/Dispatch.hpp>

#include "game_manager.hpp"
#include "gjgamelevel.hpp"
#include <audio/all.hpp>
#include <managers/block_list.hpp>
#include <managers/error_queues.hpp>
#include <managers/friend_list.hpp>
#include <managers/profile_cache.hpp>
#include <managers/game_server.hpp>
#include <managers/settings.hpp>
#include <managers/room.hpp>
#include <data/packets/client/game.hpp>
#include <data/packets/client/general.hpp>
#include <data/packets/server/game.hpp>
#include <game/module/all.hpp>
#include <game/camera_state.hpp>
#include <hooks/game_manager.hpp>
#include <util/math.hpp>
#include <util/debug.hpp>
#include <util/cocos.hpp>
#include <util/format.hpp>
#include <util/lowlevel.hpp>

using namespace geode::prelude;

// how many units before the voice disappears
constexpr float PROXIMITY_VOICE_LIMIT = 1200.f;

constexpr float VOICE_OVERLAY_PAD_X = 5.f;
constexpr float VOICE_OVERLAY_PAD_Y = 20.f;

// post an event to all modules
#define GLOBED_EVENT(self, code) \
    for (auto& module : self->m_fields->modules) { \
        module->code; \
    }


bool GlobedGJBGL::init() {
    if (!GJBaseGameLayer::init()) return false;

    // yeah so like

    auto* gm = static_cast<HookedGameManager*>(GameManager::get());
    gm->setLastSceneEnum();

    return true;
}

void GlobedGJBGL::onEnterHook() {
    // when unpausing regularly, this is true, otherwise false
    auto weRunningScene = this->getParent() == CCScene::get();

    if (weRunningScene) {
        CCLayer::onEnter();
        return;
    }

    Loader::get()->queueInMainThread([self = Ref(this)] {
        if (!GlobedGJBGL::get()->isPaused(false)) {
            self->CCLayer::onEnter();
        }
    });
}

GlobedGJBGL* GlobedGJBGL::get() {
    return static_cast<GlobedGJBGL*>(GameManager::get()->m_gameLayer);
}

/* Setup */

// Runs before PlayLayer::init
void GlobedGJBGL::setupPreInit(GJGameLevel* level, bool editor) {
    auto& nm = NetworkManager::get();
    auto& settings = GlobedSettings::get();
    auto& fields = this->getFields();

    auto levelId = HookedGJGameLevel::getLevelIDFrom(level);
    fields.globedReady = nm.established() && levelId > 0;

    if (!settings.globed.editorSupport && level->m_levelType == GJLevelType::Editor) {
        fields.globedReady = false;
    }

    if (fields.globedReady) {
        // room settings
        fields.roomSettings = RoomManager::get().getInfo().settings;

        /* initialize modules */

        // discord rpc
        if (Loader::get()->isModLoaded("techstudent10.discord_rich_presence")
            && settings.globed.useDiscordRPC)
        {
            this->addModule<DiscordRpcModule>();
        }

        auto& rs = fields.roomSettings.flags;

        // Collision
        if (rs.collision) {
            bool shouldEnableCollision = true;

            constexpr bool forcedPlatformer = false; // TODO maybe reenable?
            if (!level->isPlatformer() && forcedPlatformer) {
                fields.forcedPlatformer = true;
#ifdef GEODE_IS_ANDROID
                if (this->m_uiLayer) {
                    this->m_uiLayer->togglePlatformerMode(true);
                }
#endif
            } else if (!level->isPlatformer()) {
                // not forced platformer, so disable
                shouldEnableCollision = false;
            }

            if (shouldEnableCollision) {
                this->addModule<CollisionModule>();
            }
        }

        // 2 player mode
        if (rs.twoPlayerMode && !editor) {
            this->addModule<TwoPlayerModeModule>();
        }

        // Deathlink
        if (rs.deathlink && !rs.twoPlayerMode && !editor) {
            this->addModule<DeathlinkModule>();
        }

        GLOBED_EVENT(this, setupPreInit(level));
    }
}

void GlobedGJBGL::setupAll() {
    auto& fields = this->getFields();

    this->setupBare();

    if (!fields.globedReady) return;

    this->setupDeferredAssetPreloading();

    this->setupAudio();

    this->setupCustomKeybinds();

    this->setupMisc();

    this->setupUpdate();

    this->setupUi();
}

// runs even if not connected to a server or in a non-uploaded editor level
void GlobedGJBGL::setupBare() {
    auto& fields = this->getFields();

    Build<GlobedOverlay>::create()
        .scale(0.4f)
        .zOrder(11)
        .id("game-overlay"_spr)
        .parent(this)
        .store(fields.overlay);

    auto& nm = NetworkManager::get();

    // if not authenticated, do nothing
    auto levelId = HookedGJGameLevel::getLevelIDFrom(m_level);

    if (!nm.established()) {
        fields.overlay->updateWithDisconnected();
    } else if (!fields.globedReady) {
        fields.overlay->updateWithEditor();
    } else {
        // else update the overlay with ping
        fields.overlay->updatePing(GameServerManager::get().getActivePing());
    }

    GLOBED_EVENT(this, setupBare());
}

void GlobedGJBGL::setupDeferredAssetPreloading() {
    GlobedSettings& settings = GlobedSettings::get();

    auto* gm = static_cast<HookedGameManager*>(GameManager::get());

    if (util::cocos::shouldTryToPreload(false)) {
        log::info("Preloading assets (deferred)");

        auto start = util::time::now();
        util::cocos::preloadAssets(util::cocos::AssetPreloadStage::AllWithoutDeathEffects);
        auto took = util::time::now() - start;

        log::info("Asset preloading took {}", util::format::formatDuration(took));

        gm->setAssetsPreloaded(true);
    }

    // load death effects if those were deferred too
    bool shouldLoadDeaths = settings.players.deathEffects && !settings.players.defaultDeathEffect;

    if (!util::cocos::forcedSkipPreload() && shouldLoadDeaths && !gm->getDeathEffectsPreloaded()) {
        log::info("Preloading death effects (deferred)");

        auto start = util::time::now();
        util::cocos::preloadAssets(util::cocos::AssetPreloadStage::DeathEffect);
        auto took = util::time::now() - start;

        log::info("Death effect preloading took {}", util::format::formatDuration(took));
        gm->setDeathEffectsPreloaded(true);
    }

    util::cocos::cleanupThreadPool();
}

void GlobedGJBGL::setupAudio() {
#ifdef GLOBED_VOICE_SUPPORT

    GlobedSettings& settings = GlobedSettings::get();
    auto winSize = CCDirector::get()->getWinSize();

    if (settings.communication.voiceEnabled) {
        auto& vm = GlobedAudioManager::get();
# ifdef GLOBED_VOICE_CAN_TALK
        // set the audio device
        try {
            vm.setActiveRecordingDevice(settings.communication.audioDevice);
        } catch (const std::exception& e) {
            // try default device, if we have no mic then just do nothing
            try {
                vm.setActiveRecordingDevice(0);
            } catch (const std::exception& _e) {
                settings.communication.audioDevice = -1;
            }

            settings.communication.audioDevice = 0;
        }

        // set the record buffer size
        vm.setRecordBufferCapacity(settings.communication.lowerAudioLatency ? EncodedAudioFrame::LIMIT_LOW_LATENCY : EncodedAudioFrame::LIMIT_REGULAR);

        // start passive voice recording
        auto& vrm = VoiceRecordingManager::get();
        vrm.startRecording();
# endif // GLOBED_VOICE_CAN_TALK

        if (settings.levelUi.voiceOverlay) {
            m_fields->voiceOverlay = Build<GlobedVoiceOverlay>::create()
                .parent(m_uiLayer)
                .pos(winSize.width - VOICE_OVERLAY_PAD_X, VOICE_OVERLAY_PAD_Y)
                .anchorPoint(1.f, 0.f)
                .collect();
        }
    }

    GLOBED_EVENT(this, setupAudio());

#endif // GLOBED_VOICE_SUPPORT

        /*m_fields->chatOverlay = Build<GlobedChatOverlay>::create()
            .parent(m_uiLayer)
            .pos(winSize.width + 35.f, winSize.height / 2.f)
            .scale(0.7f)
            .anchorPoint(0.f, 0.f)
            .collect();*/
}

void GlobedGJBGL::setupPacketListeners() {
    auto& nm = NetworkManager::get();

    nm.addListener<PlayerProfilesPacket>(this, [](std::shared_ptr<PlayerProfilesPacket> packet) {
        auto& pcm = ProfileCacheManager::get();
        for (auto& player : packet->players) {
            pcm.insert(player);
        }
    });

    nm.addListener<LevelDataPacket>(this, [this](std::shared_ptr<LevelDataPacket> packet){
        auto& fields = this->getFields();

        fields.lastServerUpdate = fields.timeCounter;

        for (const auto& player : packet->players) {
            if (!fields.players.contains(player.accountId)) {
                // new player joined
                this->handlePlayerJoin(player.accountId);
            }

            fields.interpolator->updatePlayer(player.accountId, player.data, fields.lastServerUpdate);
        }
    });

    nm.addListener<LevelPlayerMetadataPacket>(this, [this](std::shared_ptr<LevelPlayerMetadataPacket> packet) {
        for (const auto& player : packet->players) {
            this->m_fields->playerStore->insertOrUpdate(player.accountId, player.data.attempts, player.data.localBest);
        }
    });

    nm.addListener<ChatMessageBroadcastPacket>(this, [this](std::shared_ptr<ChatMessageBroadcastPacket> packet) {
        this->m_fields->chatMessages.push_back({packet->sender, packet->message});

        //m_fields->chatOverlay->addMessage(packet->sender, packet->message);
    });

    nm.addListener<VoiceBroadcastPacket>(this, [this](std::shared_ptr<VoiceBroadcastPacket> packet) {
#ifdef GLOBED_VOICE_SUPPORT
        // if deafened or voice is disabled, do nothing
        auto& settings = GlobedSettings::get();

        if (this->m_fields->deafened || !settings.communication.voiceEnabled) return;
        if (!this->shouldLetMessageThrough(packet->sender)) return;

        auto& vpm = VoicePlaybackManager::get();
        try {
            vpm.prepareStream(packet->sender);

            vpm.setVolume(packet->sender, settings.communication.voiceVolume);
            this->updateProximityVolume(packet->sender);
            auto result = vpm.playFrameStreamed(packet->sender, packet->frame);

            if (result.isErr()) {
                ErrorQueues::get().debugWarn(std::string("Failed to play a voice frame: ") + result.unwrapErr());
            }
        } catch(const std::exception& e) {
            ErrorQueues::get().debugWarn(std::string("Failed to play a voice frame: ") + e.what());
        }
#endif // GLOBED_VOICE_SUPPORT
    });

    GLOBED_EVENT(this, setupPacketListeners());
}

void GlobedGJBGL::setupCustomKeybinds() {
#if GLOBED_HAS_KEYBINDS && defined(GLOBED_VOICE_SUPPORT)
    // the only keybinds used are for voice chat, so if voice is disabled, do nothing

    auto& settings = GlobedSettings::get();
    if (!settings.communication.voiceEnabled) {
        return;
    }

    this->addEventListener<keybinds::InvokeBindFilter>([this](keybinds::InvokeBindEvent* event) {
        if (event->isDown()) {
            if (!this->m_fields->deafened) {
                GlobedAudioManager::get().resumePassiveRecording();
            }
        } else {
            GlobedAudioManager::get().pausePassiveRecording();
        }

        return ListenerResult::Stop;
    }, "voice-activate"_spr);

    this->addEventListener<keybinds::InvokeBindFilter>([this, &settings](keybinds::InvokeBindEvent* event) {
        auto& vpm = VoicePlaybackManager::get();

        if (event->isDown()) {
            this->m_fields->deafened = !this->m_fields->deafened;
            if (this->m_fields->deafened) {
                vpm.muteEveryone();
                GlobedAudioManager::get().pausePassiveRecording();
                if (settings.communication.deafenNotification)
                    Notification::create("Deafened Voice Chat", CCSprite::createWithSpriteFrameName("deafen-icon-on.png"_spr), 0.2f)->show();

            } else {
                //before the notification would only show up if you had voice proximity off in a platformer, this fixes that
                if (settings.communication.deafenNotification)
                    Notification::create("Undeafened Voice Chat", CCSprite::createWithSpriteFrameName("deafen-icon-off.png"_spr), 0.2f)->show();
                if (!this->m_fields->isVoiceProximity) {
                    vpm.setVolumeAll(settings.communication.voiceVolume);
                }
            }
        }

        return ListenerResult::Propagate;
    }, "voice-deafen"_spr);

    GLOBED_EVENT(this, setupCustomKeybinds());
#endif // GLOBED_HAS_KEYBINDS && GLOBED_VOICE_SUPPORT
}

void GlobedGJBGL::setupUpdate() {
    auto& nm = NetworkManager::get();

    // update
    Loader::get()->queueInMainThread([&nm] {
        auto self = GlobedGJBGL::get();
        if (!self || !self->established()) return;

        // here we run the stuff that must run on a valid playlayer
        self->setupPacketListeners();

        // send LevelJoinPacket and RequestPlayerProfilesPacket

        auto levelId = HookedGJGameLevel::getLevelIDFrom(self->m_level);

        nm.send(LevelJoinPacket::create(levelId, self->m_level->m_unlisted));

        self->rescheduleSelectors();
        self->getParent()->schedule(schedule_selector(GlobedGJBGL::selUpdate), 0.f);

        self->scheduleOnce(schedule_selector(GlobedGJBGL::postInitActions), 0.25f);
    });
}

void GlobedGJBGL::setupMisc() {
    auto& settings = GlobedSettings::get();
    auto& nm = NetworkManager::get();
    auto& fields = this->getFields();

    fields.isVoiceProximity = m_level->isPlatformer() ? settings.communication.voiceProximity : settings.communication.classicProximity;

    // set the configured tps
    auto tpsCap = settings.globed.tpsCap;
    if (tpsCap != 0) {
        fields.configuredTps = std::min(nm.getServerTps(), (uint32_t)tpsCap);
    } else {
        fields.configuredTps = nm.getServerTps();
    }

    // interpolator
    fields.interpolator = std::make_unique<PlayerInterpolator>(InterpolatorSettings {
        .realtime = false,
        .isPlatformer = m_level->isPlatformer(),
        .expectedDelta = (1.0f / fields.configuredTps)
    });

    // player store
    fields.playerStore = std::make_unique<PlayerStore>();

    // send SyncIconsPacket if our icons have changed since the last time we sent it
    auto& pcm = ProfileCacheManager::get();
    pcm.setOwnDataAuto();
    if (pcm.pendingChanges) {
        pcm.pendingChanges = false;
        nm.send(SyncIconsPacket::create(pcm.getOwnData()));
    }

    // friendlist stuff
    auto& flm = FriendListManager::get();
    flm.maybeLoad();

    // vmt hook
#ifdef GEODE_IS_WINDOWS
    static auto patch = [&]() -> Patch* {
        auto vtableidx = 85;
        auto p = util::lowlevel::vmtHook(&GlobedGJBGL::onEnterHook, this, vtableidx);
        if (p.isErr()) {
            log::warn("GJBGL vmt hook failed: {}", p.unwrapErr());
            return nullptr;
        } else {
            return p.unwrap();
        }
    }();

    if (patch && !patch->isEnabled()) {
        (void) patch->enable();
    }
#endif

    GLOBED_EVENT(this, setupMisc());

    // check if any modules disable progress
    bool shouldSafeMode = false;
    for (auto& module : fields.modules) {
        if (!module->shouldSaveProgress()) {
            shouldSafeMode = true;
            break;
        }
    }

    if (shouldSafeMode) {
        fields.progressForciblyDisabled = true;
        this->toggleSafeMode(true);
    }
}

void GlobedGJBGL::setupUi() {
    auto& settings = GlobedSettings::get();
    auto& pcm = ProfileCacheManager::get();
    auto& fields = this->getFields();

    // progress icon
    Build<CCNode>::create()
        .id("progress-bar-wrapper"_spr)
        .visible(settings.levelUi.progressIndicators)
        .zOrder(-1)
        .store(fields.progressBarWrapper);

    Build<PlayerProgressIcon>::create()
        .parent(fields.progressBarWrapper)
        .id("self-player-progress"_spr)
        .store(fields.selfProgressIcon);

    fields.selfProgressIcon->updateIcons(pcm.getOwnData());
    fields.selfProgressIcon->setForceOnTop(true);

    // status icons
    if (settings.players.statusIcons) {
        Build<PlayerStatusIcons>::create(255)
            .scale(0.8f)
            .anchorPoint(0.5f, 0.f)
            .pos(0.f, 25.f)
            .parent(m_objectLayer)
            .id("self-status-icon"_spr)
            .store(fields.selfStatusIcons);
    }

    // own name
    if (settings.players.showNames && settings.players.ownName) {
        auto ownData = pcm.getOwnAccountData();
        auto ownSpecial = pcm.getOwnSpecialData();

        Build<GlobedNameLabel>::create(ownData.name, ownSpecial)
            .parent(m_objectLayer)
            .id("self-name"_spr)
            .store(fields.ownNameLabel);

        fields.ownNameLabel->updateOpacity(settings.players.nameOpacity);

        if (settings.players.dualName) {
            Build<GlobedNameLabel>::create(ownData.name, ownSpecial)
                .visible(false)
                .parent(m_objectLayer)
                .id("self-name-p2"_spr)
                .store(fields.ownNameLabel2);

            fields.ownNameLabel2->updateOpacity(settings.players.nameOpacity);
        }
    }

    GLOBED_EVENT(this, setupUi());
}

void GlobedGJBGL::postInitActions(float) {
    auto& nm = NetworkManager::get();
    nm.send(RequestPlayerProfilesPacket::create(0));

    m_fields->shouldRequestMeta = true;

    GLOBED_EVENT(this, postInitActions());
}

/* Selectors */

// selSendPlayerData - runs tps (default 30) times per second
void GlobedGJBGL::selSendPlayerData(float) {
    auto self = GlobedGJBGL::get();

    if (!self || !self->established()) return;
    auto& fields = self->getFields();

    // if (!self->isCurrentPlayLayer()) return;
    if (!self->accountForSpeedhack(0, 1.0f / fields.configuredTps, 0.8f)) return;

    fields.totalSentPackets++;
    // additionally, if there are no players on the level, we drop down to 1 time per second as an optimization
    // or if we are quitting the level
    if ((fields.players.empty() && fields.totalSentPackets % 30 != 15) || fields.quitting) return;

    auto data = self->gatherPlayerData();
    std::optional<PlayerMetadata> meta;
    if (util::misc::swapFlag(fields.shouldRequestMeta)) {
        meta = self->gatherPlayerMetadata();
    }

    NetworkManager::get().send(PlayerDataPacket::create(data, meta));
}

// selSendPlayerMetadata - runs every 10 seconds
void GlobedGJBGL::selSendPlayerMetadata(float) {
    auto self = GlobedGJBGL::get();

    if (!self || !self->established()) return;
    auto& fields = self->getFields();

    // if there are no players or we are quitting from the level, don't send the packet
    if (fields.players.empty() || fields.quitting) return;

    m_fields->shouldRequestMeta = true;
}

// selPeriodicalUpdate - runs 4 times a second, does various stuff
void GlobedGJBGL::selPeriodicalUpdate(float dt) {
    auto self = GlobedGJBGL::get();

    if (!self || !self->established()) return;
    auto& fields = self->getFields();
    // if (!self->isCurrentPlayLayer()) return;

    // update the overlay
    fields.overlay->updatePing(GameServerManager::get().getActivePing());

    auto& pcm = ProfileCacheManager::get();

    util::collections::SmallVector<int, 32> toRemove;

    // if more than a second passed and there was only 1 player, they probably left
    if (fields.timeCounter - fields.lastServerUpdate > 1.0f && fields.players.size() < 2) {
        for (const auto& [playerId, _] : fields.players) {
            toRemove.push_back(playerId);
        }

        for (int id : toRemove) {
            self->handlePlayerLeave(id);
        }
    } else {
        util::collections::SmallVector<int, 256> ids;

        // kick players that have left the level
        for (const auto& [playerId, remotePlayer] : fields.players) {
            // if the player doesnt exist in last LevelData packet, they have left the level
            if (fields.interpolator->isPlayerStale(playerId, fields.lastServerUpdate)) {
                toRemove.push_back(playerId);
                continue;
            }

            auto data = pcm.getData(playerId);

            if (!remotePlayer->isValidPlayer()) {
                if (data.has_value()) {
                    // if the profile data already exists in cache, use it
                    remotePlayer->updateAccountData(data.value(), true);
                    continue;
                }

                // request again if it has either been 5 seconds, or if the player just joined
                if (remotePlayer->getDefaultTicks() == 20) {
                    remotePlayer->setDefaultTicks(0);
                }

                if (remotePlayer->getDefaultTicks() == 0) {
                    ids.push_back(playerId);
                }

                remotePlayer->incDefaultTicks();
            } else if (data.has_value()) {
                // still try to see if the cache has changed
                remotePlayer->updateAccountData(data.value());
            }
        }

        if (ids.size() > 3) {
            NetworkManager::get().send(RequestPlayerProfilesPacket::create(0));
        } else {
            for (int id : ids) {
                NetworkManager::get().send(RequestPlayerProfilesPacket::create(id));
            }
        }

        for (int id : toRemove) {
            self->handlePlayerLeave(id);
        }
    }

    // update the ping to the server if overlay is enabled
    if (GlobedSettings::get().overlay.enabled) {
        NetworkManager::get().updateServerPing();
    }

    GLOBED_EVENT(self, selPeriodicalUpdate(dt));
}

// selUpdate - runs every frame, increments the non-decreasing time counter, interpolates and updates players
void GlobedGJBGL::selUpdate(float timescaledDt) {
    // timescale silently changing dt isn't very good when doing network interpolation >_>
    // since timeCounter needs to agree with everyone else on how long a second is!
    float dt = timescaledDt / CCScheduler::get()->getTimeScale();

    auto self = GlobedGJBGL::get();

    if (!self) return;

    auto& fields = self->getFields();

    fields.camState.visibleOrigin = CCPoint{0.f, 0.f};
    fields.camState.visibleCoverage = CCDirector::get()->getWinSize();

    fields.camState.cameraOrigin = self->m_gameState.m_cameraPosition;
    fields.camState.zoom = self->m_objectLayer->getScale();

    // update ourselves
    auto accountId = GJAccountManager::get()->m_accountID;
    fields.playerStore->insertOrUpdate(
        accountId,
        self->m_level->m_attempts,
        static_cast<uint32_t>(self->m_level->isPlatformer() ? self->m_level->m_bestTime : self->m_level->m_normalPercent)
    );

    fields.timeCounter += dt;

    fields.interpolator->tick(dt);

    if (auto pl = PlayLayer::get()) {
        if (fields.progressBarWrapper->getParent() != nullptr) {
            fields.selfProgressIcon->updatePosition(pl->getCurrentPercent() / 100.f, self->m_isPracticeMode);
        } else if (pl->m_progressBar) {
            // for some reason, the progressbar is sometimes initialized later than PlayLayer::init
            // it always should exist, even in levels with no actual progress bar (i.e. platformer levels)
            // but it can randomly get initialized late.
            // why robtop????????
            pl->m_progressBar->addChild(fields.progressBarWrapper);
        }
    }

    auto& bl = BlockListManager::get();
    auto& vpm = VoicePlaybackManager::get();
    auto& settings = GlobedSettings::get();

    for (const auto [playerId, remotePlayer] : fields.players) {
        const auto& vstate = fields.interpolator->getPlayerState(playerId);

        auto frameFlags = fields.interpolator->swapFrameFlags(playerId);

        bool isSpeaking = vpm.isSpeaking(playerId);
        remotePlayer->updateData(
            vstate,
            frameFlags,
            isSpeaking,
            isSpeaking ? vpm.getLoudness(playerId) : 0.f
        );

        // update progress icons
        if (auto self = PlayLayer::get()) {
            // dont update if we are in a normal level and without a progressbar
            if (self->m_level->isPlatformer() || self->m_progressBar->isVisible()) {
                remotePlayer->updateProgressIcon();
            }
        }

        // update voice proximity
        self->updateProximityVolume(playerId);

        GLOBED_EVENT(self, onUpdatePlayer(playerId, remotePlayer, frameFlags));
    }

    if (fields.selfStatusIcons) {
        float pos = (fields.ownNameLabel && fields.ownNameLabel->isVisible()) ? 40.f : 25.f;
        fields.selfStatusIcons->setPosition(self->m_player1->getPosition() + CCPoint{0.f, pos});
        bool recording = VoiceRecordingManager::get().isRecording();

        fields.selfStatusIcons->updateStatus(false, false, recording, false, 0.f);
    }

    if (fields.voiceOverlay) {
        fields.voiceOverlay->updateOverlaySoft();
    }

    // update self names
    if (fields.ownNameLabel) {
        auto dirVec = GlobedGJBGL::getCameraDirectionVector();
        auto dir = GlobedGJBGL::getCameraDirectionAngle();

        if (self->m_player1->m_isHidden) {
            fields.ownNameLabel->setVisible(false);
        } else {
            fields.ownNameLabel->setVisible(true);
            fields.ownNameLabel->setPosition(self->m_player1->getPosition() + dirVec * CCPoint{25.f, 25.f});
            fields.ownNameLabel->setRotation(dir);
        }

        if (fields.ownNameLabel2) {
            if (self->m_player2->m_isHidden || !self->m_gameState.m_isDualMode) {
                fields.ownNameLabel2->setVisible(false);
            } else {
                fields.ownNameLabel2->setVisible(true);
                fields.ownNameLabel2->setPosition(self->m_player2->getPosition() + dirVec * CCPoint{25.f, 25.f});
                fields.ownNameLabel->setRotation(dir);
            }
        }
    }

    GLOBED_EVENT(self, selUpdate(dt));
}

// selUpdateEstimators - runs 30 times a second, updates audio stuff
void GlobedGJBGL::selUpdateEstimators(float dt) {
    auto* self = GlobedGJBGL::get();

    // update volume estimators
    VoicePlaybackManager::get().updateAllEstimators(dt);

    if (auto overlay = self->m_fields->voiceOverlay) {
        overlay->updateOverlay();
    }

    GLOBED_EVENT(self, selUpdateEstimators(dt));
}

/* Player related functions */

SpecificIconData GlobedGJBGL::gatherSpecificIconData(PlayerObject* player) {
    PlayerIconType iconType = PlayerIconType::Cube;
    if (player->m_isShip) iconType = m_level->isPlatformer() ? PlayerIconType::Jetpack : PlayerIconType::Ship;
    else if (player->m_isBird) iconType = PlayerIconType::Ufo;
    else if (player->m_isBall) iconType = PlayerIconType::Ball;
    else if (player->m_isDart) iconType = PlayerIconType::Wave;
    else if (player->m_isRobot) iconType = PlayerIconType::Robot;
    else if (player->m_isSpider) iconType = PlayerIconType::Spider;
    else if (player->m_isSwing) iconType = PlayerIconType::Swing;

    auto* pobjInner = static_cast<CCNode*>(player->getChildren()->objectAtIndex(0));

    float rot = player->getRotation();

    bool isPlayer1 = player == m_player1;

    auto spiderTeleportData = isPlayer1 ? util::misc::swapOptional(m_fields->spiderTp1) : util::misc::swapOptional(m_fields->spiderTp2);
    bool didJustJump = isPlayer1 ? util::misc::swapFlag(m_fields->didJustJumpp1) : util::misc::swapFlag(m_fields->didJustJumpp2);

    bool isStationary = false;
    if (m_level->isPlatformer()) {
        isStationary = std::abs(player->m_platformerXVelocity) < 0.1;
    }

    return SpecificIconData {
        .position = player->getPosition(),
        .rotation = rot,

        .iconType = iconType,
        .isVisible = player->isVisible(),
        .isLookingLeft = player->m_isGoingLeft,
        .isUpsideDown = player->m_isSwing ? player->m_isUpsideDown : pobjInner->getScaleY() == -1.0f,
        .isDashing = player->m_isDashing,
        .isMini = player->m_vehicleSize != 1.0f,
        .isGrounded = player->m_isOnGround,
        .isStationary = isStationary,
        .isFalling = player->m_yVelocity < 0.0,
        .didJustJump = didJustJump,
        .isRotating = player->m_isRotating,
        .isSideways = player->m_isSideways,

        .spiderTeleportData = spiderTeleportData
    };
}

PlayerData GlobedGJBGL::gatherPlayerData() {
    bool isDead = m_player1->m_isDead || m_player2->m_isDead;
    m_fields->isCurrentlyDead = isDead;

    // this function (getCurrentPercent) only exists in playlayer and not the editor, so reimpl it
    auto getPercent = [&](){
        float percent;

        if (m_level->m_timestamp > 0) {
            percent = static_cast<float>(m_gameState.m_currentProgress) / m_level->m_timestamp * 100.f;
        } else {
            percent = m_player1->getPosition().x / m_levelLength * 100.f;
        }

        if (percent >= 100.f) {
            return 100.f;
        } else if (percent <= 0.f) {
            return 0.f;
        } else {
            return percent;
        }
    };

    float currentPercentage = getPercent() / 100.f;
    if (std::isnan(currentPercentage)) {
        currentPercentage = 0.f;
    }

    bool isInEditor = this->isEditor();
    bool isEditorBuilding = false;

    if (isInEditor) {
        isEditorBuilding = this->m_playbackMode == PlaybackMode::Not;
    }

    return PlayerData {
        .timestamp = m_fields->timeCounter,

        .player1 = this->gatherSpecificIconData(m_player1),
        .player2 = this->gatherSpecificIconData(m_player2),

        .lastDeathTimestamp = m_fields->lastDeathTimestamp,

        .currentPercentage = currentPercentage,

        .isDead = isDead,
        .isPaused = this->isPaused(),
        .isPracticing = m_isPracticeMode,
        .isDualMode = m_gameState.m_isDualMode,
        .isInEditor = isInEditor,
        .isEditorBuilding = isEditorBuilding,
        .isLastDeathReal = m_fields->isLastDeathReal
    };
}

PlayerMetadata GlobedGJBGL::gatherPlayerMetadata() {
    uint32_t localBest;
    if (m_level->isPlatformer()) {
        localBest = static_cast<uint32_t>(m_level->m_bestTime);
    } else {
        localBest = static_cast<uint32_t>(m_level->m_normalPercent);
    }

    return PlayerMetadata {
        .localBest = localBest,
        .attempts = m_level->m_attempts,
    };
}

CCPoint GlobedGJBGL::getCameraDirectionVector() {
    float dir = GlobedGJBGL::getCameraDirectionAngle();
    float rads = CC_DEGREES_TO_RADIANS(dir);
    return CCPoint{std::sin(rads), std::cos(rads)};
}

float GlobedGJBGL::getCameraDirectionAngle() {
    bool rotateNames = GlobedSettings::get().players.rotateNames;
    float dir = GJBaseGameLayer::get() && rotateNames ? -GJBaseGameLayer::get()->m_gameState.m_cameraAngle : 0;
    return dir;
}

bool GlobedGJBGL::shouldLetMessageThrough(int playerId) {
    auto& settings = GlobedSettings::get();
    auto& flm = FriendListManager::get();
    auto& bl = BlockListManager::get();

    if (bl.isExplicitlyBlocked(playerId)) return false;
    if (bl.isExplicitlyAllowed(playerId)) return true;

    if (settings.communication.onlyFriends && !flm.isFriend(playerId)) return false;

    return true;
}

void GlobedGJBGL::updateProximityVolume(int playerId) {
    if (m_fields->deafened || !m_fields->isVoiceProximity) return;

    auto& vpm = VoicePlaybackManager::get();

    if (!m_fields->interpolator->hasPlayer(playerId)) {
        // if we have no knowledge on the player, set volume to 0
        vpm.setVolume(playerId, 0.f);
        return;
    }

    auto& vstate = m_fields->interpolator->getPlayerState(playerId);

    float distance = cocos2d::ccpDistance(m_player1->getPosition(), vstate.player1.position);
    float volume = 1.f - std::clamp(distance, 0.01f, PROXIMITY_VOICE_LIMIT) / PROXIMITY_VOICE_LIMIT;
    if (vstate.isInEditor) {
        volume = 1.f;
    }

    auto& settings = GlobedSettings::get();

    if (this->shouldLetMessageThrough(playerId)) {
        vpm.setVolume(playerId, volume * settings.communication.voiceVolume);
    }
}

void GlobedGJBGL::notifyDeath() {
    auto& fields = this->getFields();

    fields.lastDeathTimestamp = fields.timeCounter;
    fields.isLastDeathReal = !fields.isFakingDeath;
}

void GlobedGJBGL::handlePlayerJoin(int playerId) {
    auto& settings = GlobedSettings::get();

    PlayerProgressIcon* progressIcon = nullptr;
    PlayerProgressArrow* progressArrow = nullptr;

    bool platformer = m_level->isPlatformer();

    if (!platformer && settings.levelUi.progressIndicators) {
        Build<PlayerProgressIcon>::create()
            .zOrder(2)
            .id(util::cocos::spr(fmt::format("remote-player-progress-{}", playerId)))
            .parent(m_fields->progressBarWrapper)
            .store(progressIcon);
    } else if (platformer && settings.levelUi.progressIndicators) {
        Build<PlayerProgressArrow>::create()
            .zOrder(2)
            .id(util::cocos::spr(fmt::format("remote-player-progress-{}", playerId)))
            .parent(this)
            .store(progressArrow);
    }

    // if we are in the editor, hide the progress indicators
    if (this->isEditor()) {
        if (progressArrow) {
            progressArrow->setVisible(false);
        }

        if (progressIcon) {
            progressIcon->setVisible(false);
        }
    }

    auto* rp = Build<RemotePlayer>::create(&m_fields->camState, progressIcon, progressArrow)
        .zOrder(10)
        .id(util::cocos::spr(fmt::format("remote-player-{}", playerId)))
        .collect();

    auto& pcm = ProfileCacheManager::get();
    auto pcmData = pcm.getData(playerId);
    if (pcmData.has_value()) {
        rp->updateAccountData(pcmData.value(), true);
    }

    auto& bl = BlockListManager::get();
    if (bl.isHidden(playerId)) {
        rp->setForciblyHidden(true);
    }

    m_objectLayer->addChild(rp);
    m_fields->players.emplace(playerId, rp);
    m_fields->interpolator->addPlayer(playerId);

    GLOBED_EVENT(this, onPlayerJoin(rp));
}

void GlobedGJBGL::handlePlayerLeave(int playerId) {
    VoicePlaybackManager::get().removeStream(playerId);

    if (!m_fields->players.contains(playerId)) return;


    auto rp = m_fields->players.at(playerId);

    GLOBED_EVENT(this, onPlayerLeave(rp));

    rp->removeProgressIndicators();
    rp->removeFromParent();

    m_fields->players.erase(playerId);
    m_fields->interpolator->removePlayer(playerId);
    m_fields->playerStore->removePlayer(playerId);
}

bool GlobedGJBGL::established() {
    // the 2nd check is in case we disconnect while being in a level somehow
    return m_fields->globedReady && NetworkManager::get().established();
}

bool GlobedGJBGL::isCurrentPlayLayer() {
    auto playLayer = geode::cocos::getChildOfType<PlayLayer>(cocos2d::CCScene::get(), 0);
    return static_cast<GJBaseGameLayer*>(playLayer) == this;
}

bool GlobedGJBGL::isPaused(bool checkCurrent) {
    if (this->isEditor()) {
        return this->m_playbackMode == PlaybackMode::Paused;
    }

    if (PlayLayer::get()) {
        if (checkCurrent && !isCurrentPlayLayer()) return false;

        for (CCNode* child : CCArrayExt<CCNode*>(this->getParent()->getChildren())) {
            if (typeinfo_cast<PauseLayer*>(child)) {
                return true;
            }
        }
    }

    return false;
}

bool GlobedGJBGL::isEditor() {
    return (void*)LevelEditorLayer::get() == (void*)this;
}

bool GlobedGJBGL::isSafeMode() {
    return m_fields->shouldStopProgress;
}

void GlobedGJBGL::toggleSafeMode(bool enabled) {
    if (m_fields->progressForciblyDisabled) {
        enabled = true;
    }

    m_fields->shouldStopProgress = enabled;
}

void GlobedGJBGL::onQuitActions() {
    auto& nm = NetworkManager::get();

    m_fields->quitting = true;

    if (m_fields->globedReady) {
        if (nm.established()) {
            // send LevelLeavePacket
            nm.send(LevelLeavePacket::create());
        }

#ifdef GLOBED_VOICE_SUPPORT
        // stop voice recording and playback
        GlobedAudioManager::get().haltRecording();
        VoicePlaybackManager::get().stopAllStreams();
#endif // GLOBED_VOICE_SUPPORT

        GLOBED_EVENT(this, onQuit());
    }
}

void GlobedGJBGL::pausedUpdate(float dt) {
    // unpause dash effects and death effects
    for (auto* child : CCArrayExt<CCNode*>(m_objectLayer->getChildren())) {
        int tag1 = ComplexVisualPlayer::SPIDER_DASH_CIRCLE_WAVE_TAG;
        int tag2 = ComplexVisualPlayer::SPIDER_DASH_SPRITE_TAG;
        int tag3 = ComplexVisualPlayer::DEATH_EFFECT_TAG;

        int ctag = child->getTag();

        if (ctag == tag1 || ctag == tag2 || ctag == tag3) {
            child->resumeSchedulerAndActions();
        }
    }
}

bool GlobedGJBGL::accountForSpeedhack(int uniqueKey, float cap, float allowance) {
    auto* sched = CCScheduler::get();
    auto& fields = this->getFields();
    auto ts = sched->getTimeScale();

    if (!util::math::equal(ts, fields.lastKnownTimeScale)) {
        this->unscheduleSelectors();
        this->rescheduleSelectors();
    }

    auto now = util::time::now();

    if (!fields.lastSentPacket.contains(uniqueKey)) {
        fields.lastSentPacket.emplace(uniqueKey, now);
        return true;
    }

    auto lastSent = fields.lastSentPacket.at(uniqueKey);

    auto passed = util::time::asMillis(now - lastSent);

    if (passed < cap * 1000 * allowance) {
        // log::warn("dropping a packet (speedhack?), passed time is {}ms", passed);
        return false;
    }

    fields.lastSentPacket[uniqueKey] = now;

    return true;
}

void GlobedGJBGL::unscheduleSelectors() {
    auto* sched = CCScheduler::get();

    this->unscheduleSelector(schedule_selector(GlobedGJBGL::selSendPlayerData));
    this->unscheduleSelector(schedule_selector(GlobedGJBGL::selSendPlayerMetadata));
    this->unscheduleSelector(schedule_selector(GlobedGJBGL::selPeriodicalUpdate));
    this->unscheduleSelector(schedule_selector(GlobedGJBGL::selUpdateEstimators));

    GLOBED_EVENT(this, onUnscheduleSelectors());
}

void GlobedGJBGL::unscheduleSelector(cocos2d::SEL_SCHEDULE selector) {
    m_pScheduler->unscheduleSelector(selector, this->getParent());
}

void GlobedGJBGL::rescheduleSelectors() {
    auto* sched = CCScheduler::get();
    float timescale = sched->getTimeScale();
    m_fields->lastKnownTimeScale = timescale;

    float pdInterval = (1.0f / m_fields->configuredTps) * timescale;
    float pmdInterval = 10.f * timescale;
    float updpInterval = 0.25f * timescale;
    float updeInterval = (1.0f / 30.f) * timescale;

    this->customSchedule(schedule_selector(GlobedGJBGL::selSendPlayerData), pdInterval);
    this->customSchedule(schedule_selector(GlobedGJBGL::selSendPlayerMetadata), pmdInterval);
    this->customSchedule(schedule_selector(GlobedGJBGL::selPeriodicalUpdate), updpInterval);
    this->customSchedule(schedule_selector(GlobedGJBGL::selUpdateEstimators), updeInterval);

    GLOBED_EVENT(this, onRescheduleSelectors(timescale));
}

void GlobedGJBGL::customSchedule(cocos2d::SEL_SCHEDULE selector, float interval) {
    this->getParent()->schedule(selector, interval);
}

GlobedGJBGL::Fields& GlobedGJBGL::getFields() {
    return *m_fields.self();
}

// hooks

int GlobedGJBGL::checkCollisions(PlayerObject* player, float dt, bool p2) {
    int retval = GJBaseGameLayer::checkCollisions(player, dt, p2);

    if ((void*)this != GJBaseGameLayer::get()) return retval;
    if (!this->established()) return retval;

    GLOBED_EVENT(this, checkCollisions(player, dt, p2));

    return retval;
}

// TODO: this crashes when megahack force platformer is enabled
// go beg absolute to use geode patches.

// void GlobedGJBGL::loadLevelSettings() {
//     if (!GJBaseGameLayer::get()) {
//         GJBaseGameLayer::loadLevelSettings();
//         return;
//     }

//     GLOBED_EVENT(this, loadLevelSettingsPre());

//     GJBaseGameLayer::loadLevelSettings();

//     GLOBED_EVENT(this, loadLevelSettingsPost());
// }

class $modify(PlayerObject) {
    static void onModify(auto& self) {
        (void) self.setHookPriority("PlayerObject::playerDestroyed", 99999999).unwrap();
    }

    void update(float dt) {
        PlayerObject::update(dt);

        auto pl = GlobedGJBGL::get();
        if ((void*)m_gameLayer != pl || !pl) return;

        if (pl->m_player1 == this || pl->m_player2 == this) {
            GLOBED_EVENT(pl, mainPlayerUpdate(this, dt));
        } else {
            GLOBED_EVENT(pl, onlinePlayerUpdate(this, dt));
        }
    }

    void playerDestroyed(bool p0) {
        PlayerObject::playerDestroyed(p0);

        auto* gjbgl = GlobedGJBGL::get();
        if (gjbgl && (this == gjbgl->m_player1 || this == gjbgl->m_player2)) {
            GLOBED_EVENT(gjbgl, playerDestroyed(this, p0));
            gjbgl->notifyDeath();
        }
    }
};

void GlobedGJBGL::updateCamera(float dt) {
    GLOBED_EVENT(this, updateCameraPre(dt));
    GJBaseGameLayer::updateCamera(dt);
    GLOBED_EVENT(this, updateCameraPost(dt));
}
