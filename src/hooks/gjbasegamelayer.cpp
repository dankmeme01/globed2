#include "gjbasegamelayer.hpp"

#include "play_layer.hpp"
#include "game_manager.hpp"

#if GLOBED_HAS_KEYBINDS
# include <geode.custom-keybinds/include/Keybinds.hpp>
#endif // GLOBED_HAS_KEYBINDS

#include "gjgamelevel.hpp"
#include <audio/all.hpp>
#include <managers/block_list.hpp>
#include <managers/error_queues.hpp>
#include <managers/friend_list.hpp>
#include <managers/profile_cache.hpp>
#include <managers/game_server.hpp>
#include <managers/settings.hpp>
#include <managers/room.hpp>
#include <data/packets/all.hpp>
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
void GlobedGJBGL::setupPreInit(GJGameLevel* level) {
    auto& nm = NetworkManager::get();
    auto& settings = GlobedSettings::get();

    auto levelId = HookedGJGameLevel::getLevelIDFrom(level);
    m_fields->globedReady = nm.established() && levelId > 0;

    if (!settings.globed.editorSupport && level->m_levelType == GJLevelType::Editor) {
        m_fields->globedReady = false;
    }

    if (m_fields->globedReady) {
        // room settings
        m_fields->roomSettings = RoomManager::get().getInfo().settings;

        // collision only works in platformer, so force platformer :D
        if (m_fields->roomSettings.flags.collision && !level->isPlatformer()) {
            m_fields->roomSettings.flags.collision = false;

            constexpr bool forcedPlatformer = false; // TODO: maybe reenable?
            if (forcedPlatformer) {
                m_fields->forcedPlatformer = true;
                m_fields->roomSettings.flags.collision = true;
#ifdef GEODE_IS_ANDROID
                if (this->m_uiLayer) {
                    this->m_uiLayer->togglePlatformerMode(true);
                }
#endif
            }
        }

        // if 2 player mode is enabled, we are the primary player if we are the room creator
        if (m_fields->roomSettings.flags.twoPlayerMode) {
            m_fields->twopstate.isPrimary = RoomManager::get().isOwner();
        }
    }
}

void GlobedGJBGL::setupAll() {
    this->setupBare();

    if (!m_fields->globedReady) return;

    this->setupDeferredAssetPreloading();

    this->setupAudio();

    this->setupCustomKeybinds();

    this->setupMisc();

    this->setupUpdate();

    this->setupUi();
}

// runs even if not connected to a server or in a non-uploaded editor level
void GlobedGJBGL::setupBare() {
    Build<GlobedOverlay>::create()
        .scale(0.4f)
        .zOrder(11)
        .id("game-overlay"_spr)
        .parent(this)
        .store(m_fields->overlay);

    auto& nm = NetworkManager::get();

    // if not authenticated, do nothing
    auto levelId = HookedGJGameLevel::getLevelIDFrom(m_level);

    if (!nm.established()) {
        m_fields->overlay->updateWithDisconnected();
    } else if (!m_fields->globedReady) {
        m_fields->overlay->updateWithEditor();
    } else {
        // else update the overlay with ping
        m_fields->overlay->updatePing(GameServerManager::get().getActivePing());
    }
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
        this->m_fields->lastServerUpdate = this->m_fields->timeCounter;

        for (const auto& player : packet->players) {
            if (!this->m_fields->players.contains(player.accountId)) {
                // new player joined
                this->handlePlayerJoin(player.accountId);
            }

            this->m_fields->interpolator->updatePlayer(player.accountId, player.data, this->m_fields->lastServerUpdate);
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

        // TODO: remove legacy packet after min protocol becomes 8
        if (nm.getServerProtocol() >= 8) {
            nm.send(LevelJoinPacket::create(levelId, self->m_level->m_unlisted));
        } else {
            nm.send(LevelJoinLegacyPacket::create(levelId));
        }

        self->rescheduleSelectors();
        self->getParent()->schedule(schedule_selector(GlobedGJBGL::selUpdate), 0.f);

        self->scheduleOnce(schedule_selector(GlobedGJBGL::postInitActions), 0.25f);

        self->updateDRPC();
    });
}

void GlobedGJBGL::setupMisc() {
    auto& settings = GlobedSettings::get();
    auto& nm = NetworkManager::get();

    m_fields->isVoiceProximity = m_level->isPlatformer() ? settings.communication.voiceProximity : settings.communication.classicProximity;

    // set the configured tps
    auto tpsCap = settings.globed.tpsCap;
    if (tpsCap != 0) {
        m_fields->configuredTps = std::min(nm.getServerTps(), (uint32_t)tpsCap);
    } else {
        m_fields->configuredTps = nm.getServerTps();
    }

    // interpolator
    m_fields->interpolator = std::make_unique<PlayerInterpolator>(InterpolatorSettings {
        .realtime = false,
        .isPlatformer = m_level->isPlatformer(),
        .expectedDelta = (1.0f / m_fields->configuredTps)
    });

    // player store
    m_fields->playerStore = std::make_unique<PlayerStore>();

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

    // toggle safe mode if collision is enabled
    if (m_fields->roomSettings.flags.collision) {
        m_fields->progressForciblyDisabled = true;
        this->toggleSafeMode(true);
    }

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
}

void GlobedGJBGL::setupUi() {
    auto& settings = GlobedSettings::get();
    auto& pcm = ProfileCacheManager::get();

    // progress icon
    Build<CCNode>::create()
        .id("progress-bar-wrapper"_spr)
        .visible(settings.levelUi.progressIndicators)
        .zOrder(-1)
        .store(m_fields->progressBarWrapper);

    Build<PlayerProgressIcon>::create()
        .parent(m_fields->progressBarWrapper)
        .id("self-player-progress"_spr)
        .store(m_fields->selfProgressIcon);

    m_fields->selfProgressIcon->updateIcons(pcm.getOwnData());
    m_fields->selfProgressIcon->setForceOnTop(true);

    // status icons
    if (settings.players.statusIcons) {
        Build<PlayerStatusIcons>::create(255)
            .scale(0.8f)
            .anchorPoint(0.5f, 0.f)
            .pos(0.f, 25.f)
            .parent(m_objectLayer)
            .id("self-status-icon"_spr)
            .store(m_fields->selfStatusIcons);
    }

    // own name
    if (settings.players.showNames && settings.players.ownName) {
        auto ownData = pcm.getOwnAccountData();
        auto ownSpecial = pcm.getOwnSpecialData();

        Build<GlobedNameLabel>::create(ownData.name, ownSpecial)
            .parent(m_objectLayer)
            .id("self-name"_spr)
            .store(m_fields->ownNameLabel);

        m_fields->ownNameLabel->updateOpacity(settings.players.nameOpacity);

        if (settings.players.dualName) {
            Build<GlobedNameLabel>::create(ownData.name, ownSpecial)
                .visible(false)
                .parent(m_objectLayer)
                .id("self-name-p2"_spr)
                .store(m_fields->ownNameLabel2);

            m_fields->ownNameLabel2->updateOpacity(settings.players.nameOpacity);
        }
    }
}

void GlobedGJBGL::postInitActions(float) {
    auto& nm = NetworkManager::get();
    nm.send(RequestPlayerProfilesPacket::create(0));

    auto data = this->gatherPlayerMetadata();
    nm.send(PlayerMetadataPacket::create(data));
}

/* Selectors */

// selSendPlayerData - runs tps (default 30) times per second
void GlobedGJBGL::selSendPlayerData(float) {
    auto self = GlobedGJBGL::get();

    if (!self || !self->established()) return;
    // if (!self->isCurrentPlayLayer()) return;
    if (!self->accountForSpeedhack(0, 1.0f / self->m_fields->configuredTps, 0.8f)) return;

    self->m_fields->totalSentPackets++;
    // additionally, if there are no players on the level, we drop down to 1 time per second as an optimization
    // or if we are quitting the level
    if ((self->m_fields->players.empty() && self->m_fields->totalSentPackets % 30 != 15) || self->m_fields->quitting) return;

    auto data = self->gatherPlayerData();
    NetworkManager::get().send(PlayerDataPacket::create(data));
}

// selSendPlayerMetadata - runs every 10 seconds
void GlobedGJBGL::selSendPlayerMetadata(float) {
    auto self = GlobedGJBGL::get();

    if (!self || !self->established()) return;
    // if there are no players or we are quitting from the level, don't send the packet
    if (self->m_fields->players.empty() || self->m_fields->quitting) return;

    auto data = self->gatherPlayerMetadata();
    NetworkManager::get().send(PlayerMetadataPacket::create(data));
}

// selPeriodicalUpdate - runs 4 times a second, does various stuff
void GlobedGJBGL::selPeriodicalUpdate(float) {
    auto self = GlobedGJBGL::get();

    if (!self || !self->established()) return;
    // if (!self->isCurrentPlayLayer()) return;

    // update the overlay
    self->m_fields->overlay->updatePing(GameServerManager::get().getActivePing());

    auto& pcm = ProfileCacheManager::get();

    util::collections::SmallVector<int, 32> toRemove;

    // if more than a second passed and there was only 1 player, they probably left
    if (self->m_fields->timeCounter - self->m_fields->lastServerUpdate > 1.0f && self->m_fields->players.size() < 2) {
        for (const auto& [playerId, _] : self->m_fields->players) {
            toRemove.push_back(playerId);
        }

        for (int id : toRemove) {
            self->handlePlayerLeave(id);
        }
    } else {
        util::collections::SmallVector<int, 256> ids;

        // kick players that have left the level
        for (const auto& [playerId, remotePlayer] : self->m_fields->players) {
            // if the player doesnt exist in last LevelData packet, they have left the level
            if (self->m_fields->interpolator->isPlayerStale(playerId, self->m_fields->lastServerUpdate)) {
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
}

// selUpdate - runs every frame, increments the non-decreasing time counter, interpolates and updates players
void GlobedGJBGL::selUpdate(float timescaledDt) {
    // timescale silently changing dt isn't very good when doing network interpolation >_>
    // since timeCounter needs to agree with everyone else on how long a second is!
    float dt = timescaledDt / CCScheduler::get()->getTimeScale();

    auto self = GlobedGJBGL::get();

    if (!self) return;

    self->m_fields->camState.visibleOrigin = CCPoint{0.f, 0.f};
    self->m_fields->camState.visibleCoverage = CCDirector::get()->getWinSize();

    self->m_fields->camState.cameraOrigin = self->m_gameState.m_cameraPosition;
    self->m_fields->camState.zoom = self->m_objectLayer->getScale();

    // update ourselves
    auto accountId = GJAccountManager::get()->m_accountID;
    self->m_fields->playerStore->insertOrUpdate(
        accountId,
        self->m_level->m_attempts,
        static_cast<uint32_t>(self->m_level->isPlatformer() ? self->m_level->m_bestTime : self->m_level->m_normalPercent)
    );

    self->m_fields->timeCounter += dt;

    self->m_fields->interpolator->tick(dt);

    if (auto pl = PlayLayer::get()) {
        if (self->m_fields->progressBarWrapper->getParent() != nullptr) {
            self->m_fields->selfProgressIcon->updatePosition(pl->getCurrentPercent() / 100.f, self->m_isPracticeMode);
        } else if (pl->m_progressBar) {
            // for some reason, the progressbar is sometimes initialized later than PlayLayer::init
            // it always should exist, even in levels with no actual progress bar (i.e. platformer levels)
            // but it can randomly get initialized late.
            // why robtop????????
            pl->m_progressBar->addChild(self->m_fields->progressBarWrapper);
        }
    }

    auto& bl = BlockListManager::get();
    auto& vpm = VoicePlaybackManager::get();
    auto& settings = GlobedSettings::get();

    for (const auto [playerId, remotePlayer] : self->m_fields->players) {
        const auto& vstate = self->m_fields->interpolator->getPlayerState(playerId);

        auto frameFlags = self->m_fields->interpolator->swapFrameFlags(playerId);

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
    }

    if (self->m_fields->selfStatusIcons) {
        self->m_fields->selfStatusIcons->setPosition(self->m_player1->getPosition() + CCPoint{0.f, 25.f});
        bool recording = VoiceRecordingManager::get().isRecording();

        self->m_fields->selfStatusIcons->updateStatus(false, false, recording, false, 0.f);
    }

    if (self->m_fields->voiceOverlay) {
        self->m_fields->voiceOverlay->updateOverlaySoft();
    }

    // update self names
    if (self->m_fields->ownNameLabel) {
        if (self->m_player1->m_isHidden) {
            self->m_fields->ownNameLabel->setVisible(false);
        } else {
            self->m_fields->ownNameLabel->setVisible(true);
            self->m_fields->ownNameLabel->setPosition(self->m_player1->getPosition() + CCPoint{0.f, 25.f});
        }

        if (self->m_fields->ownNameLabel2) {
            if (self->m_player2->m_isHidden || !self->m_gameState.m_isDualMode) {
                self->m_fields->ownNameLabel2->setVisible(false);
            } else {
                self->m_fields->ownNameLabel2->setVisible(true);
                self->m_fields->ownNameLabel2->setPosition(self->m_player2->getPosition() + CCPoint{0.f, 25.f});
            }
        }
    }
}

// selUpdateEstimators - runs 30 times a second, updates audio stuff
void GlobedGJBGL::selUpdateEstimators(float dt) {
    auto* self = GlobedGJBGL::get();

    // update volume estimators
    VoicePlaybackManager::get().updateAllEstimators(dt);

    if (self->m_fields->voiceOverlay) {
        self->m_fields->voiceOverlay->updateOverlay();
    }
}

void GlobedGJBGL::updateDRPC() {
    if (!Loader::get()->isModLoaded("techstudent10.discord_rich_presence")) return;

    if (!GlobedSettings::get().globed.useDiscordRPC) return;

    // taken from drpc
    bool isRobTopLevel = (
        (m_level->m_levelID.value() < 128 && m_level->m_levelID.value() != 0) ||
            m_level->m_levelID.value() == 3001 || m_level->m_levelID.value() < 5003 ||
            m_level->m_levelID.value() > 5000
    ) && m_level->m_creatorName.empty();

    auto state = fmt::format("{} by {}", std::string(m_level->m_levelName), (isRobTopLevel) ? "RobTopGames" : std::string(m_level->m_creatorName));

    using SetRPCEvent = geode::DispatchEvent<bool>;
    SetRPCEvent("techstudent10.discord_rich_presence/set_default_rpc_enabled", false).post();

    using UpdateRPCEvent = geode::DispatchEvent<std::string>;
    auto json = matjson::Value(matjson::Object({
        {"modID", ""_spr},
        {"details", "Playing on Globed!"},
        {"state", state},
        {"smallImageKey", ""},
        {"smallImageText", ""},
        {"useTime", true},
        {"shouldResetTime", false},
        {"largeImageKey", "https://raw.githubusercontent.com/dankmeme01/globed2/main/logo.png"},
        {"largeImageText", ""},
        {"joinSecret", std::to_string(m_level->m_levelID.value())},
        {"partyMax", m_fields->players.size() + 1}
    })).dump();
    UpdateRPCEvent("techstudent10.discord_rich_presence/update_rpc", json).post();
}

// selUpdateDRPC - runs every 10 seconds, updates Discord RPC
void GlobedGJBGL::selUpdateDRPC(float dt) {
    auto self = GlobedGJBGL::get();

    if (!self || !self->established()) return;

    self->updateDRPC();
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
    if (isDead && !m_fields->isCurrentlyDead) {
        m_fields->isCurrentlyDead = true;
        m_fields->lastDeathTimestamp = m_fields->timeCounter;
    } else if (!isDead) {
        m_fields->isCurrentlyDead = false;
    }

    // TODO: move the inline impl to GJBaseGameLayer bindings
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

    // log::debug("Player joined: {}", playerId);
}

void GlobedGJBGL::handlePlayerLeave(int playerId) {
    VoicePlaybackManager::get().removeStream(playerId);

    if (!m_fields->players.contains(playerId)) return;

    auto rp = m_fields->players.at(playerId);
    rp->removeProgressIndicators();
    rp->removeFromParent();

    m_fields->players.erase(playerId);
    m_fields->interpolator->removePlayer(playerId);
    m_fields->playerStore->removePlayer(playerId);

    // log::debug("Player removed: {}", playerId);
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
    }

    // report back to the RPC mod that it can takeover again
    using SetRPCEvent = geode::DispatchEvent<bool>;
    SetRPCEvent("techstudent10.discord_rich_presence/set_default_rpc_enabled", true).post();
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
    auto ts = sched->getTimeScale();
    if (!util::math::equal(ts, m_fields->lastKnownTimeScale)) {
        this->unscheduleSelectors();
        this->rescheduleSelectors();
    }

    auto now = util::time::now();

    if (!m_fields->lastSentPacket.contains(uniqueKey)) {
        m_fields->lastSentPacket.emplace(uniqueKey, now);
        return true;
    }

    auto lastSent = m_fields->lastSentPacket.at(uniqueKey);

    auto passed = util::time::asMillis(now - lastSent);

    if (passed < cap * 1000 * allowance) {
        // log::warn("dropping a packet (speedhack?), passed time is {}ms", passed);
        return false;
    }

    m_fields->lastSentPacket[uniqueKey] = now;

    return true;
}

void GlobedGJBGL::unscheduleSelectors() {
    auto* sched = CCScheduler::get();
    sched->unscheduleSelector(schedule_selector(GlobedGJBGL::selSendPlayerData), this->getParent());
    sched->unscheduleSelector(schedule_selector(GlobedGJBGL::selSendPlayerMetadata), this->getParent());
    sched->unscheduleSelector(schedule_selector(GlobedGJBGL::selPeriodicalUpdate), this->getParent());
    sched->unscheduleSelector(schedule_selector(GlobedGJBGL::selUpdateEstimators), this->getParent());
    sched->unscheduleSelector(schedule_selector(GlobedGJBGL::selUpdateDRPC), this->getParent());
}

void GlobedGJBGL::rescheduleSelectors() {
    auto* sched = CCScheduler::get();
    float timescale = sched->getTimeScale();
    m_fields->lastKnownTimeScale = timescale;

    float pdInterval = (1.0f / m_fields->configuredTps) * timescale;
    float pmdInterval = 10.f * timescale;
    float updpInterval = 0.25f * timescale;
    float updeInterval = (1.0f / 30.f) * timescale;
    float drpcInterval = 10.f * timescale;

    this->getParent()->schedule(schedule_selector(GlobedGJBGL::selSendPlayerData), pdInterval);
    this->getParent()->schedule(schedule_selector(GlobedGJBGL::selSendPlayerMetadata), pmdInterval);
    this->getParent()->schedule(schedule_selector(GlobedGJBGL::selPeriodicalUpdate), updpInterval);
    this->getParent()->schedule(schedule_selector(GlobedGJBGL::selUpdateEstimators), updeInterval);
    this->getParent()->schedule(schedule_selector(GlobedGJBGL::selUpdateDRPC), drpcInterval);
}

/* Collision stuff */

static bool shouldCorrectCollision(const CCRect& p1, const CCRect& p2, CCPoint& displacement) {
    if (std::abs(displacement.x) > 10.f) {
        displacement.x = -displacement.x;
        displacement.y = 0;
        return true;
    }

    return false;
}

// hooks

int GlobedGJBGL::checkCollisions(PlayerObject* player, float dt, bool p2) {
    int retval = GJBaseGameLayer::checkCollisions(player, dt, p2);

    if ((void*)this != GJBaseGameLayer::get()) return retval;

    auto* gpl = GlobedGJBGL::get();

    if (!gpl->established()) return retval;
    if (!gpl->m_fields->roomSettings.flags.collision) return retval;

    bool isSecond = player == gpl->m_player2;

    for (const auto& [_, rp] : gpl->m_fields->players) {
        auto* p1 = static_cast<PlayerObject*>(rp->player1->getPlayerObject());
        auto* p2 = static_cast<PlayerObject*>(rp->player2->getPlayerObject());

        auto& p1Rect = p1->getObjectRect();
        auto& p2Rect = p2->getObjectRect();

        auto& playerRect = player->getObjectRect();

        CCRect p1CollRect = p1Rect;
        CCRect p2CollRect = p2Rect;

        constexpr float padding = 2.f;

        // p1CollRect.origin += CCPoint{padding, padding};
        // p1CollRect.size -= CCSize{padding * 2, padding * 2};

        // p2CollRect.origin += CCPoint{padding, padding};
        // p2CollRect.size -= CCSize{padding * 2, padding * 2};

        bool intersectsP1 = playerRect.intersectsRect(p1CollRect);
        bool intersectsP2 = playerRect.intersectsRect(p2CollRect);

        if (isSecond) {
            rp->player1->setP2StickyState(false);
            rp->player2->setP2StickyState(false);
        } else {
            rp->player1->setP1StickyState(false);
            rp->player2->setP1StickyState(false);
        }

        if (intersectsP1) {
            auto prev = player->getPosition();
            player->collidedWithObject(dt, p1, p1CollRect, false);
            auto displacement = player->getPosition() - prev;

            // log::debug("p1 intersect, displacement: {}", displacement);

            bool shouldRevert = shouldCorrectCollision(playerRect, p1Rect, displacement);

            if (shouldRevert) {
                player->setPosition(player->getPosition() + displacement);
            }

            if (std::abs(displacement.y) > 0.001f) {
                isSecond ? rp->player1->setP2StickyState(true) : rp->player1->setP1StickyState(true);
            }
        }

        if (intersectsP2) {
            auto prev = player->getPosition();
            player->collidedWithObject(dt, p2, p2CollRect, false);
            auto displacement = player->getPosition() - prev;

            // log::debug("p2 intersect, displacement: {}", displacement);

            bool shouldRevert = shouldCorrectCollision(playerRect, p2Rect, displacement);

            if (shouldRevert) {
                player->setPosition(player->getPosition() + displacement);
            }

            if (std::abs(displacement.y) > 0.001f) {
                isSecond ? rp->player2->setP2StickyState(true) : rp->player2->setP1StickyState(true);
            }
        }
    }

    return retval;
}

void GlobedGJBGL::loadLevelSettings() {
    LevelSettingsObject* lo = m_levelSettings;

    if (!GJBaseGameLayer::get()) {
        GJBaseGameLayer::loadLevelSettings();
        return;
    }

    auto* gpl = GlobedGJBGL::get();
    bool lastPlat = lo->m_platformerMode;
    auto lastLength = m_level->m_levelLength;

    if (gpl->m_fields->forcedPlatformer) {
        lo->m_platformerMode = true;
    }

    GJBaseGameLayer::loadLevelSettings();

    lo->m_platformerMode = lastPlat;
    m_level->m_levelLength = lastLength;
}

/* 2-player mode stuff */

class $modify(TwoPModePlayerObject, PlayerObject) {
    struct Fields {
        Ref<ComplexVisualPlayer> lockedTo;
    };

    void update(float dt) {
        // dont ask
        auto* bgl = static_cast<GlobedGJBGL*>(typeinfo_cast<GJBaseGameLayer*>((CCObject*)m_gameLayer));

        if (!bgl || !bgl->m_fields->twopstate.active || !m_fields->lockedTo) {
            PlayerObject::update(dt);
            return;
        }

        PlayerObject* noclipFor = nullptr;
        if (bgl->m_fields->twopstate.isPrimary) {
            noclipFor = bgl->m_player2;
        } else {
            noclipFor = bgl->m_player1;
        }

        if (this == noclipFor) {
            PlayerObject::update(dt);

            this->updateFromLockedPlayer(!bgl->m_fields->twopstate.isPrimary && bgl->m_gameState.m_isDualMode);
            this->setVisible(false);
            // if (bgl->m_gameState.m_isDualMode) {
            //     this->m_isHidden = true;
            // }

            this->m_playEffects = false;
            if (this->m_regularTrail) this->m_regularTrail->setVisible(false);
            if (this->m_waveTrail) this->m_waveTrail->setVisible(false);
            if (this->m_ghostTrail) this->m_ghostTrail->setVisible(false);
            if (this->m_trailingParticles) this->m_trailingParticles->setVisible(false);
            if (this->m_shipStreak) this->m_shipStreak->setVisible(false);
        } else {
            PlayerObject::update(dt);
        }
    }

    void setLockedTo(ComplexVisualPlayer* player) {
        m_fields->lockedTo = player;
    }

    void clearLockedTo() {
        this->setLockedTo(nullptr);
    }

    void updateFromLockedPlayer(bool ignorePos) {
        RemotePlayer* rp = m_fields->lockedTo->getRemotePlayer();
        if (rp->lastFrameFlags.pendingDeath) {
            Loader::get()->queueInMainThread([] {
                static_cast<GlobedPlayLayer*>(PlayLayer::get())->forceKill(PlayLayer::get()->m_player1);
            });
        }

        if (!ignorePos && !rp->lastVisualState.isDead) {
            this->setPosition(m_fields->lockedTo->getPlayerPosition());
        }
    }
};

// TODO: test if still needed for 2 player mode
// void GlobedGJBGL::updateCamera(float dt) {
//     if (!m_fields->twopstate.active || m_fields->twopstate.isPrimary || !m_gameState.m_isDualMode) {
//         GJBaseGameLayer::updateCamera(dt);
//         return;
//     }

//     auto lastPos = m_player1->getPosition();
//     m_player1->setPosition({m_player2->getPositionX(), lastPos.y});
//     GJBaseGameLayer::updateCamera(dt);
//     m_player1->setPosition(lastPos);
// }

void GlobedGJBGL::linkPlayerTo(int accountId) {
    if (!m_fields->players.contains(accountId)) return;
    if (!m_fields->roomSettings.flags.twoPlayerMode) return;

    RemotePlayer* rp = m_fields->players.at(accountId);

    PlayerObject* ignored = m_fields->twopstate.isPrimary ? m_player2 : m_player1;

    auto* pobj = static_cast<TwoPModePlayerObject*>(ignored);
    pobj->setLockedTo(m_fields->twopstate.isPrimary ? rp->player2 : rp->player1);

    m_fields->twopstate.active = true;
}
