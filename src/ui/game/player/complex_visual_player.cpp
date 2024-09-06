#include "complex_visual_player.hpp"

#include "remote_player.hpp"
#include <hooks/game_manager.hpp>
#include <hooks/gjbasegamelayer.hpp>
#include <managers/settings.hpp>
#include <util/gd.hpp>
#include <util/rng.hpp>
#include <util/math.hpp>
#include <util/debug.hpp>
#include <util/ui.hpp>

using namespace geode::prelude;

bool ComplexVisualPlayer::init(RemotePlayer* parent, bool isSecond) {
    if (!CCNode::init()) return false;
    this->parent = parent;
    this->isSecond = isSecond;

    this->gameLayer = GJBaseGameLayer::get();
    this->isPlatformer = gameLayer->m_level->isPlatformer();
    this->isEditor = typeinfo_cast<LevelEditorLayer*>(this->gameLayer) != nullptr;

    auto& data = parent->getAccountData();

    auto& settings = GlobedSettings::get();

    auto* hgm = static_cast<HookedGameManager*>(GameManager::get());

    auto playerOpacity = static_cast<unsigned char>(settings.players.playerOpacity * 255.f);

    // // save the old streak
    // int oldStreak = hgm->getPlayerStreak();
    // int oldShipStreak = hgm->getPlayerShipFire();

    // int streak = 6; // TODO trails: needs protocol change
    // int shipStreak = 6;

    // // set the streak of the player
    // hgm->setPlayerStreak(streak);
    // hgm->setPlayerShipStreak(shipStreak);

    // create the player
    playerIcon = static_cast<ComplexPlayerObject*>(Build<PlayerObject>::create(1, 1, this->gameLayer, this->gameLayer->m_objectLayer, false)
        .opacity(playerOpacity)
        .parent(this)
        .collect());

    playerIcon->setRemotePlayer(this);
    // this->enableTrail();

    // // restore the old streak
    // hgm->setPlayerStreak(oldStreak);
    // hgm->setPlayerShipStreak(oldShipStreak);

    Build<GlobedNameLabel>::create(data.name)
        .visible(settings.players.showNames && (!isSecond || settings.players.dualName))
        .pos(0.f, 25.f)
        .parent(this)
        .store(nameLabel);

    this->updateIcons(data.icons);

    if (!isSecond && settings.players.statusIcons) {
        statusIcons = Build<PlayerStatusIcons>::create(playerOpacity)
            .scale(0.8f)
            .anchorPoint(0.5f, 0.f)
            .pos(0.f, settings.players.showNames ? 40.f : 25.f)
            .parent(this)
            .id("status-icons"_spr)
            .collect();
    }

    // preload the cube icon so the passengers are correct
    this->updateIconType(PlayerIconType::Cube);

    return true;
}

void ComplexVisualPlayer::updateIcons(const PlayerIconData& icons) {
    auto* gm = GameManager::get();
    auto& settings = GlobedSettings::get();

    // update the name and the badge
    const auto& accountData = parent->getAccountData();
    nameLabel->updateData(accountData.name, accountData.specialUserData);
    nameLabel->updateOpacity(settings.players.nameOpacity);

    playerIcon->togglePlatformerMode(gameLayer->m_level->isPlatformer());

    storedIcons = icons;
    if (settings.players.defaultDeathEffect) {
        // set the default one
        storedIcons.deathEffect = 1;
    }

    // android is funny and quirky
    if (static_cast<HookedGameManager*>(gm)->getAssetsPreloaded() GEODE_ANDROID(|| true)) {
        this->updatePlayerObjectIcons(true);
        this->updateIconType(playerIconType);
    } else {
        this->tryLoadIconsAsync();
    }

    // // give ids to some of the related nodes
    // auto& data = parent->getAccountData();
    // if (playerIcon->m_waveTrail) {
    //     playerIcon->m_waveTrail->setID(fmt::format("dankmeme.globed2/wave-trail-{}{}", data.accountId, isSecond ? "-second" : ""));
    // }

    // if (playerIcon->m_regularTrail) {
    //     playerIcon->m_regularTrail->setID(fmt::format("dankmeme.globed2/regular-trail-{}{}", data.accountId, isSecond ? "-second" : ""));
    // }

    // if (playerIcon->m_ghostTrail) {
    //     playerIcon->m_ghostTrail->setID(fmt::format("dankmeme.globed2/ghost-trail-{}{}", data.accountId, isSecond ? "-second" : ""));
    // }

    // if (playerIcon->m_shipStreak) {
    //     playerIcon->m_shipStreak->setID(fmt::format("dankmeme.globed2/ship-trail-{}{}", data.accountId, isSecond ? "-second" : ""));
    // }
}

void ComplexVisualPlayer::updateData(
        const SpecificIconData& data,
        const VisualPlayerState& playerData,
        const GameCameraState& camState,
        bool isSpeaking,
        float loudness
) {
    auto& settings = GlobedSettings::get();

    wasRotating = data.isRotating;

    bool isNearby = this->isPlayerNearby(camState);
    bool cameNearby = isNearby && !wasNearby;
    wasNearby = isNearby;

    auto displacement = data.position - playerIcon->getPosition();

    // sticky is super broken  lol

    // if (p1sticky) {
    //     auto* p1 = PlayLayer::get()->m_player1;
    //     auto newPos = p1->getPosition() + CCPoint{displacement.x, displacement.y};
    //     p1->m_position = newPos;
    //     p1->setPosition(newPos);
    //     p1->m_lastPosition = newPos;
    //     p1->m_realXPosition = newPos.x;
    // }

    // if (p2sticky) {
    //     auto* p2 = PlayLayer::get()->m_player1;
    //     auto newPos = p2->getPosition() + CCPoint{displacement.x, displacement.y};
    //     p2->m_position = newPos;
    //     p2->setPosition(newPos);
    //     p2->m_lastPosition = newPos;
    //     p2->m_realXPosition = newPos.x;
    // }

    playerIcon->setPosition(data.position);
    playerIcon->setRotation(data.rotation);

    float innerRot = data.isSideways ? (data.isUpsideDown ? 90.f : -90.f) : 0.f;
    playerIcon->m_mainLayer->setRotation(innerRot);

    float distanceTo90deg = std::fmod(std::abs(data.rotation), 90.f);
    if (distanceTo90deg > 45.f) {
        distanceTo90deg = 90.f - distanceTo90deg;
    }

    if (data.isRotating || distanceTo90deg > 1.f) {
        this->cancelPlatformerJumpAnim();
    }

    auto dirVec = GlobedGJBGL::getCameraDirectionVector();
    auto dir = GlobedGJBGL::getCameraDirectionAngle();

    // set the pos for status icons and name (ask rob not me)
    nameLabel->setPosition(data.position + dirVec * CCPoint{25.f, 25.f});
    nameLabel->setRotation(dir);

    if (statusIcons) {
        statusIcons->setPosition(data.position + dirVec * CCPoint{nameLabel->isVisible() ? 40.f : 25.f, nameLabel->isVisible() ? 40.f : 25.f});
        statusIcons->setRotation(dir);
    }

    if (!playerData.isDead && playerIcon->getOpacity() == 0) {
        this->updateOpacity();
    }

    // set position members for collision
    playerIcon->m_startPosition = data.position;
    playerIcon->m_lastPosition = data.position;
    playerIcon->m_positionX = data.position.x;
    playerIcon->m_positionY = data.position.y;

    PlayerIconType iconType = data.iconType;

    // setFlipX doesnt work here for jetpack and stuff
    float mult = data.isMini ? 0.6f : 1.0f;
    playerIcon->setScaleX((data.isLookingLeft ? -1.0f : 1.0f) * mult);

    // swing is not flipped
    if (iconType == PlayerIconType::Swing) {
        playerIcon->setScaleY(mult);
    } else {
        playerIcon->setScaleY((data.isUpsideDown ? -1.0f : 1.0f) * mult);
    }

    // set scale members for collision
    playerIcon->m_scaleX = mult;
    playerIcon->m_scaleY = mult;

    bool switchedMode = iconType != playerIconType;

    bool turningOffSwing = (playerIconType == PlayerIconType::Swing && switchedMode);
    bool turningOffRobot = (playerIconType == PlayerIconType::Robot && switchedMode);

    if (switchedMode) {
        this->updateIconType(iconType);
    }

    if (switchedMode || (settings.players.hideNearby && isNearby)) {
        this->updateOpacity();
    }

    if (statusIcons && isNearby) {
        statusIcons->updateStatus(playerData.isPaused, playerData.isPracticing, isSpeaking, playerData.isInEditor, loudness);
    }

    // animate dashing
    if (data.isDashing != wasDashing) {
        // TODO: dashing animation
        // if (data.isDashing) {
        //     auto kf = ObjectToolbox::sharedState()->intKeyToFrame(0x6d7);
        //     log::debug("kf: {}", kf);
        //     auto* dobj = DashRingObject::create(kf);
        //     dobj->retain();
        //     // rotation
        //     *(float*)((char*)dobj + 0x34c) = 115.f;
        //     *(bool*)((char*)dobj + 0x394) = false;
        //     playerIcon->m_playEffects = true;
        //     playerIcon->startDashing(nullptr);
        // } else {
        //     playerIcon->stopDashing();
        //     playerIcon->setRotation(0.f);
        // }

        wasDashing = data.isDashing;
    }

    // animate robot and spider
    if (iconType == PlayerIconType::Robot || iconType == PlayerIconType::Spider) {
        if (wasGrounded != data.isGrounded || wasStationary != data.isStationary || wasFalling != data.isFalling || switchedMode || cameNearby) {
            wasGrounded = data.isGrounded;
            wasStationary = data.isStationary;
            wasFalling = data.isFalling;

            if (isNearby) {
                iconType == PlayerIconType::Robot ? this->updateRobotAnimation() : this->updateSpiderAnimation();
            }
        }
    }

    // animate swing fire
    else if (iconType == PlayerIconType::Swing) {
        // if we just switched to swing, enable all fires
        if (switchedMode) {
            playerIcon->m_swingFireTop->setVisible(true);
            playerIcon->m_swingFireMiddle->setVisible(true);
            playerIcon->m_swingFireBottom->setVisible(true);

            playerIcon->m_swingFireMiddle->animateFireIn();
        }

        if (cameNearby || ((wasUpsideDown != data.isUpsideDown || switchedMode) && isNearby)) {
            wasUpsideDown = data.isUpsideDown;
            // now depending on the gravity, toggle either the bottom or top fire
            this->animateSwingFire(!wasUpsideDown);
        }
    }

    // remove swing fire
    else if (turningOffSwing) {
        // TODO: arent we immediately setting them to invisible??? the animateFireOut is useless??
        playerIcon->m_swingFireTop->setVisible(false);
        playerIcon->m_swingFireMiddle->setVisible(false);
        playerIcon->m_swingFireBottom->setVisible(false);

        if (isNearby) {
            playerIcon->m_swingFireTop->animateFireOut();
            playerIcon->m_swingFireMiddle->animateFireOut();
            playerIcon->m_swingFireBottom->animateFireOut();
        }
    }

    // remove robot fire
    else if (turningOffRobot) {
        if (isNearby) {
            this->animateRobotFire(false);
        } else {
            // just setVisible false
            this->onAnimateRobotFireOut();
        }
    }

    if (wasPaused != playerData.isPaused) {
        wasPaused = playerData.isPaused;

        if (wasPaused) {
            CCNode::onExit();
        } else {
            CCNode::onEnter();
        }
    }

    bool shouldBeVisible;
    if (isSecond && !playerData.isDualMode) {
        shouldBeVisible = false;
    } else if (settings.players.hidePracticePlayers && playerData.isPracticing) {
        shouldBeVisible = false;
    } else {
        shouldBeVisible = (data.isVisible || settings.players.forceVisibility) && !isForciblyHidden;
    }

    this->setVisible(shouldBeVisible);

    if (!shouldBeVisible) {
        playerIcon->m_playEffects = false;
        if (playerIcon->m_regularTrail) playerIcon->m_regularTrail->setVisible(false);
        if (playerIcon->m_shipStreak) playerIcon->m_shipStreak->setVisible(false);
    }
}

void ComplexVisualPlayer::updateIconType(PlayerIconType newType) {
    PlayerIconType oldType = playerIconType;
    playerIconType = newType;

    const auto& accountData = parent->getAccountData();
    const auto& icons = accountData.icons;

    this->toggleAllOff();

    if (newType != PlayerIconType::Cube) {
        this->callToggleWith(newType, true, false);
    }

    this->callUpdateWith(newType, util::gd::getIconWithType(icons, newType));
}

void ComplexVisualPlayer::playDeathEffect() {
    // if the player is not nearby, do nothing
    if (!wasNearby) return;

    this->onAnimateRobotFireOut();

    // only play the death effect in playlayer
    if (!PlayLayer::get()) {
        return;
    }

    // keep track of the previous children
    std::unordered_set<CCNode*> prevChildren;

    for (auto* child : CCArrayExt<CCNode*>(playerIcon->m_parentLayer->getChildren())) {
        prevChildren.insert(child);
    }

    playerIcon->m_playEffects = true;
    playerIcon->m_isHidden = false;
    playerIcon->playerDestroyed(false);

    // TODO temp, we remove the small cube pieces because theyre buggy in my testing
    if (auto ein = getChildOfType<ExplodeItemNode>(this, 0)) {
        ein->removeFromParent();
    }

    // now, for each *new* child, we know it's something from the death effect
    for (auto* child : CCArrayExt<CCNode*>(playerIcon->m_parentLayer->getChildren())) {
        if (!prevChildren.contains(child)) {
            child->setTag(DEATH_EFFECT_TAG);
        }
    }
}

void ComplexVisualPlayer::playSpiderTeleport(const SpiderTeleportData& data) {
    // spider teleport effect is only played in play layer, and when nearby
    if (!PlayLayer::get() || !wasNearby) return;

    playerIcon->m_playEffects = true;
    playerIcon->stopActionByTag(SPIDER_TELEPORT_COLOR_ACTION);

    auto* arr = PlayLayer::get()->m_circleWaveArray;
    size_t countBefore = arr ? arr->count() : 0;
    playerIcon->playSpiderDashEffect(data.from, data.to);
    size_t countAfter = arr ? arr->count() : 0;

    if (countBefore != countAfter) {
        for (size_t i = countBefore; i < countAfter; i++) {
            static_cast<CCNode*>(arr->objectAtIndex(i))->setTag(SPIDER_DASH_CIRCLE_WAVE_TAG);
        }
    }

    // name the sprite too
    for (auto* child : CCArrayExt<CCNode*>(playerIcon->m_parentLayer->getChildren())) {
        if (child->getZOrder() != 40) continue;
        if (!child->getID().empty()) continue;

        auto* sprite = typeinfo_cast<CCSprite*>(child);
        if (!sprite) continue;

        auto* sfc = CCSpriteFrameCache::get();
        auto* spdash1 = sfc->spriteFrameByName("spiderDash_001.png")->getTexture();
        auto* tex = sprite->getTexture();

        if (tex == spdash1) {
            sprite->setTag(SPIDER_DASH_SPRITE_TAG);
        }
    }

    tpColorDelta = 0.f;

    this->spiderTeleportUpdateColor();
}

void ComplexVisualPlayer::playJump() {
    if (!wasNearby) return;

    if (gameLayer->m_level->isPlatformer() && playerIconType == PlayerIconType::Cube && !wasRotating) {
        playerIcon->animatePlatformerJump(1.0f);
        didPerformPlatformerJump = true;
    }
}

void ComplexVisualPlayer::setForciblyHidden(bool state) {
    if (state) {
        if (playerIcon->m_regularTrail) playerIcon->m_regularTrail->setVisible(false);
        if (playerIcon->m_waveTrail) playerIcon->m_waveTrail->setVisible(false);
        if (playerIcon->m_ghostTrail) playerIcon->m_ghostTrail->setVisible(false);
        if (playerIcon->m_shipStreak) playerIcon->m_shipStreak->setVisible(false);
    }

    isForciblyHidden = state;
}

static inline ccColor3B lerpColor(ccColor3B from, ccColor3B to, float delta) {
    delta = std::clamp(delta, 0.f, 1.f);

    ccColor3B out;
    out.r = std::lerp(from.r, to.r, delta);
    out.g = std::lerp(from.g, to.g, delta);
    out.b = std::lerp(from.b, to.b, delta);

    return out;
}

void ComplexVisualPlayer::spiderTeleportUpdateColor() {
    constexpr float MAX_TIME = 0.4f;

    tpColorDelta += (1.f / 60.f);

    float delta = tpColorDelta / MAX_TIME;

    if (delta >= 1.f) {
        playerIcon->stopActionByTag(SPIDER_TELEPORT_COLOR_ACTION);
        playerIcon->setColor(storedMainColor);
        playerIcon->setSecondColor(storedSecondaryColor);
        return;
    }

    auto main = lerpColor(ccColor3B{255, 255, 255}, storedMainColor, delta);
    auto secondary = lerpColor(ccColor3B{255, 255, 255}, storedSecondaryColor, delta);

    playerIcon->setColor(main);
    playerIcon->setSecondColor(secondary);

    auto* seq = CCSequence::create(
        CCDelayTime::create(1.f / 60.f),
        CCCallFunc::create(this, callfunc_selector(ComplexVisualPlayer::spiderTeleportUpdateColor)),
        nullptr
    );
    seq->setTag(SPIDER_TELEPORT_COLOR_ACTION);

    this->runAction(seq);
}

void ComplexVisualPlayer::updateRobotAnimation() {
    if (wasGrounded && wasStationary) {
        // if on ground and not moving, play the idle animation
        playerIcon->m_robotSprite->tweenToAnimation("idle01", 0.1f);
        this->animateRobotFire(false);
    } else if (wasGrounded && !wasStationary) {
        // if on ground and moving, play the running animation
        playerIcon->m_robotSprite->tweenToAnimation("run", 0.1f);
        this->animateRobotFire(false);
    } else if (wasFalling) {
        // if in the air and falling, play falling animation
        playerIcon->m_robotSprite->tweenToAnimation("fall_loop", 0.1f);
        this->animateRobotFire(false);
    } else if (!wasFalling) {
        // if in the air and not falling, play jumping animation
        playerIcon->m_robotSprite->tweenToAnimation("jump_loop", 0.1f);
        this->animateRobotFire(true);
    }
}

void ComplexVisualPlayer::updateSpiderAnimation() {
    // this is practically the same as the robot animation

    if (!wasGrounded && wasFalling) {
        playerIcon->m_spiderSprite->tweenToAnimation("fall_loop", 0.1f);
    } else if (!wasGrounded && !wasFalling) {
        playerIcon->m_spiderSprite->tweenToAnimation("jump_loop", 0.1f);
    } else if (wasGrounded && wasStationary) {
        playerIcon->m_spiderSprite->tweenToAnimation("idle01", 0.1f);
    } else if (wasGrounded && !wasStationary) {
        playerIcon->m_spiderSprite->tweenToAnimation("run", 0.1f);
    }
}

void ComplexVisualPlayer::animateRobotFire(bool enable) {
    playerIcon->m_robotFire->stopActionByTag(ROBOT_FIRE_ACTION);

    CCSequence* seq;
    if (enable) {
        seq = CCSequence::create(
            CCDelayTime::create(0.15f),
            CCCallFunc::create(this, callfunc_selector(ComplexVisualPlayer::onAnimateRobotFireIn)),
            nullptr
        );

        playerIcon->m_robotFire->setVisible(true);
    } else {
        seq = CCSequence::create(
            CCDelayTime::create(0.1f),
            CCCallFunc::create(this, callfunc_selector(ComplexVisualPlayer::onAnimateRobotFireOut)),
            nullptr
        );

        playerIcon->m_robotFire->animateFireOut();
    }

    seq->setTag(ROBOT_FIRE_ACTION);
    playerIcon->m_robotFire->runAction(seq);
}

void ComplexVisualPlayer::onAnimateRobotFireIn() {
    playerIcon->m_robotFire->animateFireIn();
}

void ComplexVisualPlayer::animateSwingFire(bool goingDown) {
    if (goingDown) {
        playerIcon->m_swingFireTop->animateFireIn();
        playerIcon->m_swingFireBottom->animateFireOut();
    } else {
        playerIcon->m_swingFireTop->animateFireOut();
        playerIcon->m_swingFireBottom->animateFireIn();
    }
}

void ComplexVisualPlayer::onAnimateRobotFireOut() {
    playerIcon->m_robotFire->setVisible(false);
}

void ComplexVisualPlayer::updatePlayerObjectIcons(bool skipFrames) {
    auto* gm = GameManager::get();

    storedMainColor = gm->colorForIdx(storedIcons.color1);
    storedSecondaryColor = gm->colorForIdx(storedIcons.color2);

    playerIcon->setColor(storedMainColor);
    playerIcon->setSecondColor(storedSecondaryColor);

    if (storedIcons.glowColor != NO_GLOW) {
        playerIcon->m_hasGlow = true;
        playerIcon->enableCustomGlowColor(gm->colorForIdx(storedIcons.glowColor));
    } else {
        playerIcon->m_hasGlow = false;
        playerIcon->disableCustomGlowColor();
    }

    if (!skipFrames) {
        playerIcon->updatePlayerFrame(storedIcons.cube);
        playerIcon->updatePlayerShipFrame(storedIcons.ship);
        playerIcon->updatePlayerRollFrame(storedIcons.ball);
        playerIcon->updatePlayerBirdFrame(storedIcons.ufo);
        playerIcon->updatePlayerDartFrame(storedIcons.wave);
        playerIcon->updatePlayerRobotFrame(storedIcons.robot);
        playerIcon->updatePlayerSpiderFrame(storedIcons.spider);
        playerIcon->updatePlayerSwingFrame(storedIcons.swing);
        playerIcon->updatePlayerJetpackFrame(storedIcons.jetpack);
    }

    playerIcon->updateGlowColor();
    playerIcon->updatePlayerGlow();

    // set opacities
    this->updateOpacity();
}

void ComplexVisualPlayer::updateOpacity() {
    auto& settings = GlobedSettings::get();

    float mult = 1.f;
    if (settings.players.hideNearby) {
        // calculate distance
        auto p1pos = this->gameLayer->m_player1->getPosition();
        auto p2pos = this->gameLayer->m_player2->getPosition();
        auto ourPos = this->getPlayerPosition();

        float distance = std::min(cocos2d::ccpDistance(ourPos, p1pos), cocos2d::ccpDistance(ourPos, p2pos));

        // range of 150 units (5 blocks)
        distance = std::clamp(distance, 0.f, 150.f);
        mult = distance / 150.f;
    }

    unsigned char opacity = static_cast<unsigned char>(settings.players.playerOpacity * mult * 255.f);

    playerIcon->setOpacity(opacity);
    playerIcon->m_spiderSprite->GJRobotSprite::setOpacity(opacity);
    playerIcon->m_robotSprite->GJRobotSprite::setOpacity(opacity);
    if (playerIcon->m_shipStreak) playerIcon->m_shipStreak->setOpacity(opacity);
    if (playerIcon->m_regularTrail) playerIcon->m_regularTrail->setOpacity(opacity);

    // set name opacity too if hideNearby is enabled
    if (settings.players.hideNearby) {
        nameLabel->updateOpacity(settings.players.nameOpacity * mult);
    }
}

void ComplexVisualPlayer::toggleAllOff() {
    playerIcon->toggleFlyMode(false, false);
    playerIcon->toggleRollMode(false, false);
    playerIcon->toggleBirdMode(false, false);
    playerIcon->toggleDartMode(false, false);
    playerIcon->toggleRobotMode(false, false);
    playerIcon->toggleSpiderMode(false, false);
    playerIcon->toggleSwingMode(false, false);
}

void ComplexVisualPlayer::callToggleWith(PlayerIconType type, bool arg1, bool arg2) {
    switch (type) {
        case PlayerIconType::Ship: playerIcon->toggleFlyMode(arg1, arg2); break;
        case PlayerIconType::Ball: playerIcon->toggleRollMode(arg1, arg2); break;
        case PlayerIconType::Ufo: playerIcon->toggleBirdMode(arg1, arg2); break;
        case PlayerIconType::Wave: playerIcon->toggleDartMode(arg1, arg2); break;
        case PlayerIconType::Robot: playerIcon->toggleRobotMode(arg1, arg2); break;
        case PlayerIconType::Spider: playerIcon->toggleSpiderMode(arg1, arg2); break;
        case PlayerIconType::Swing: playerIcon->toggleSwingMode(arg1, arg2); break;
        case PlayerIconType::Jetpack: playerIcon->toggleFlyMode(arg1, arg2); break;
        default: break;
    }
}

void ComplexVisualPlayer::callUpdateWith(PlayerIconType type, int icon) {
    switch (type) {
        case PlayerIconType::Cube: playerIcon->updatePlayerFrame(icon); break;
        case PlayerIconType::Ship: playerIcon->updatePlayerShipFrame(icon); break;
        case PlayerIconType::Ball: playerIcon->updatePlayerRollFrame(icon); break;
        case PlayerIconType::Ufo: playerIcon->updatePlayerBirdFrame(icon); break;
        case PlayerIconType::Wave: playerIcon->updatePlayerDartFrame(icon); break;
        case PlayerIconType::Robot: playerIcon->updatePlayerRobotFrame(icon); break;
        case PlayerIconType::Spider: playerIcon->updatePlayerSpiderFrame(icon); break;
        case PlayerIconType::Swing: playerIcon->updatePlayerSwingFrame(icon); break;
        case PlayerIconType::Jetpack: playerIcon->updatePlayerJetpackFrame(icon); break;
        case PlayerIconType::Unknown: break;
    }
}

const CCPoint& ComplexVisualPlayer::getPlayerPosition() {
    return playerIcon->getPosition();
}

CCNode* ComplexVisualPlayer::getPlayerObject() {
    return playerIcon;
}

RemotePlayer* ComplexVisualPlayer::getRemotePlayer() {
    return parent;
}

void ComplexVisualPlayer::setP1StickyState(bool state) {
    p1sticky = state;
}

void ComplexVisualPlayer::setP2StickyState(bool state) {
    p2sticky = state;
}

bool ComplexVisualPlayer::getP1StickyState() {
    return p1sticky;
}

bool ComplexVisualPlayer::getP2StickyState() {
    return p2sticky;
}

void ComplexVisualPlayer::tryLoadIconsAsync() {
    if (iconsLoaded != 0) return;
    auto* gm = GameManager::get();
    auto* textureCache = CCTextureCache::sharedTextureCache();
    auto* sfCache = CCSpriteFrameCache::sharedSpriteFrameCache();

    for (auto type = PlayerIconType::Cube; type <= PlayerIconType::Jetpack; type = (PlayerIconType)((int)type + 1)) {
        auto iconId = util::gd::getIconWithType(storedIcons, type);
        std::string sheetName = gm->sheetNameForIcon(storedIcons.cube, (int)IconType::Cube);

        if (!sheetName.empty()) {
            int key = gm->keyForIcon(iconId, (int)globed::into<IconType>(type));
            // essentially addIconDelegate
            gm->m_iconDelegates[key].push_back(this);

            if (!gm->m_isIconBeingLoaded[key]) {
                gm->m_isIconBeingLoaded[key] = true;

                auto uniqueId = util::rng::Random::get().generate<int>();
                asyncLoadRequests[uniqueId] = AsyncLoadRequest {
                    .key = key,
                    .iconId = iconId,
                    .iconType = type
                };

                textureCache->addImageAsync(
                    (sheetName + ".png").c_str(),
                    this,
                    menu_selector(ComplexVisualPlayer::asyncIconLoadedIntermediary),
                    uniqueId,
                    kCCTexture2DPixelFormat_RGBA8888
                );
            }
        }
    }
}

void ComplexVisualPlayer::onFinishedLoadingIconAsync() {
    iconsLoaded++;

    if (iconsLoaded == (int)PlayerIconType::Jetpack) {
        // if all icons loaded, we good
        iconsLoaded = 0;

        this->updatePlayerObjectIcons(true);
        this->updateIconType(playerIconType);
    }
}

void ComplexVisualPlayer::asyncIconLoadedIntermediary(cocos2d::CCObject* obj) {
    auto* texture = static_cast<CCTexture2D*>(obj);
    int uniqueId = texture->getTag();
    if (!asyncLoadRequests.contains(uniqueId)) {
        log::warn("async load requests does not contain the tag: {}", uniqueId);
        return;
    }

    auto request = asyncLoadRequests[uniqueId];
    asyncLoadRequests.erase(uniqueId);

    auto* gm = GameManager::get();
    gm->loadIcon(request.iconId, (int)globed::into<IconType>(request.iconType), -1);
    gm->m_isIconBeingLoaded[request.key] = 0;

    for (const auto delegate : gm->m_iconDelegates[request.key]) {
        static_cast<ComplexVisualPlayer*>(delegate)->onFinishedLoadingIconAsync();
    }

    gm->m_iconDelegates.erase(request.key);
}

void ComplexVisualPlayer::cancelPlatformerJumpAnim() {
    if (didPerformPlatformerJump) {
        playerIcon->stopPlatformerJumpAnimation();
        didPerformPlatformerJump = false;
    }
}

void ComplexVisualPlayer::enableTrail() {
    // log::debug("wave trail: {}", playerIcon->m_waveTrail);
    // playerIcon->activateStreak();
}

void ComplexVisualPlayer::disableTrail() {
    // inlined bitch
    // playerIcon->deactivateStreak();
    // playerIcon->m_regularTrail->stopStroke();
    // playerIcon->fadeOutStreak2(0.2f);
}

bool ComplexVisualPlayer::isPlayerNearby(const GameCameraState& camState) {
    // always render them in editor (cause im lazy)
    if (isEditor) return true;

    // check if they are inside 3 screens
    constexpr float fullScaleMult = 3.f;
    constexpr float originMoveMult = (fullScaleMult - 1.f) / 2.f; // magic
    CCSize origCoverage = camState.cameraCoverage();
    CCSize cameraCoverage = origCoverage * fullScaleMult;
    CCPoint cameraOrigin = camState.cameraOrigin - origCoverage * originMoveMult;

    float cameraLeft = cameraOrigin.x;
    float cameraRight = cameraOrigin.x + cameraCoverage.width;
    float cameraBottom = cameraOrigin.y;
    float cameraTop = cameraOrigin.y + cameraCoverage.height;

    const auto& playerPosition = this->getPlayerPosition();

    bool inCameraCoverage = (
        playerPosition.x >= cameraLeft &&
        playerPosition.x <= cameraRight &&
        playerPosition.y >= cameraBottom &&
        playerPosition.y <= cameraTop
    );

    return inCameraCoverage;
}

ComplexVisualPlayer* ComplexVisualPlayer::create(RemotePlayer* parent, bool isSecond) {
    auto ret = new ComplexVisualPlayer;
    if (ret->init(parent, isSecond)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}