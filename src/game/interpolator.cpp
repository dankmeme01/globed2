#include "interpolator.hpp"

#include "lerp_logger.hpp"
#include <hooks/play_layer.hpp>
#include <util/math.hpp>
#include <util/debug.hpp>
#include <util/format.hpp>

using namespace geode::prelude;

PlayerInterpolator::PlayerInterpolator(const InterpolatorSettings& settings) : settings(settings) {}

void PlayerInterpolator::addPlayer(int playerId) {
    players.emplace(playerId, PlayerState {});
#ifdef GLOBED_DEBUG_INTERPOLATION
    LerpLogger::get().reset(playerId);
#endif
}

void PlayerInterpolator::removePlayer(int playerId) {
    players.erase(playerId);
}

bool PlayerInterpolator::hasPlayer(int playerId) {
    return players.contains(playerId);
}

void PlayerInterpolator::updatePlayer(int playerId, const PlayerData& data, float updateCounter) {
    auto& player = players.at(playerId);
    player.updateCounter = updateCounter;
    player.pendingRealFrame = true;
    player.totalFrames++;

    if (!util::math::equal(player.lastDeathTimestamp, data.lastDeathTimestamp)) {
        player.lastDeathTimestamp = data.lastDeathTimestamp;
        if (player.totalFrames > 1) {
            player.frameFlags.pendingDeath = true;
        }
    }

    player.frameFlags.pendingP1Teleport = data.player1.spiderTeleportData;
    player.frameFlags.pendingP2Teleport = data.player2.spiderTeleportData;
    player.frameFlags.pendingP1Jump = data.player1.didJustJump;
    player.frameFlags.pendingP2Jump = data.player1.didJustJump;

    LerpLogger::get().logRealFrame(playerId, this->getLocalTs(), data.timestamp, data.player1);

    if (settings.realtime) {
        player.interpolatedState = data;
        return;
    }

    player.olderFrame = player.newerFrame;
    player.newerFrame = data;

    player.timeCounter = player.olderFrame.timestamp;
}

static inline void lerpSpecific(
        const SpecificIconData& older,
        const SpecificIconData& newer,
        SpecificIconData& out,
        float lerpRatio
    ) {

    out.copyFlagsFrom(older);

    // i hate spider
    if (out.iconType == PlayerIconType::Spider && std::abs(older.position.y - newer.position.y) >= 33.f) {
        out.position.x = std::lerp(older.position.x, newer.position.x, lerpRatio);
        out.position.y = older.position.y;
    } else {
        out.position = older.position.lerp(newer.position, lerpRatio);
    }
    out.rotation = std::lerp(older.rotation, newer.rotation, lerpRatio);
}

static inline void lerpPlayer(
        const VisualPlayerState& older,
        const VisualPlayerState& newer,
        VisualPlayerState& out,
        float lerpRatio
    ) {

    lerpSpecific(older.player1, newer.player1, out.player1, lerpRatio);
    lerpSpecific(older.player2, newer.player2, out.player2, lerpRatio);

    out.currentPercentage = older.currentPercentage;
    out.isDead = older.isDead;
    out.isPaused = older.isPaused;
    out.isPracticing = older.isPracticing;
    out.isDualMode = older.isDualMode;
}

void PlayerInterpolator::tick(float dt) {
    if (settings.realtime) return;

    for (auto& [playerId, player] : players) {
        if (player.totalFrames < 2) continue;

        float frameDelta = player.newerFrame.timestamp - player.olderFrame.timestamp;
        if (frameDelta == 0.f) {
            LerpLogger::get().logLerpSkip(playerId, this->getLocalTs(), player.timeCounter, player.interpolatedState.player1);
            continue;
        }

        float lerpRatio = (player.timeCounter - player.olderFrame.timestamp) / frameDelta;
        lerpPlayer(player.olderFrame.visual, player.newerFrame.visual, player.interpolatedState, lerpRatio);

        LerpLogger::get().logLerpOperation(playerId, this->getLocalTs(), player.timeCounter, player.interpolatedState.player1);

        player.timeCounter += dt;
    }
}

VisualPlayerState& PlayerInterpolator::getPlayerState(int playerId) {
    return players.at(playerId).interpolatedState;
}

FrameFlags PlayerInterpolator::swapFrameFlags(int playerId) {
    auto& state = players.at(playerId);
    FrameFlags out;
    out.pendingDeath = util::misc::swapFlag(state.frameFlags.pendingDeath);
    out.pendingP1Jump = util::misc::swapFlag(state.frameFlags.pendingP1Jump);
    out.pendingP2Jump = util::misc::swapFlag(state.frameFlags.pendingP2Jump);

    out.pendingP1Teleport = util::misc::swapOptional(state.frameFlags.pendingP1Teleport);
    out.pendingP2Teleport = util::misc::swapOptional(state.frameFlags.pendingP2Teleport);

    return out;
}

bool PlayerInterpolator::isPlayerStale(int playerId, float lastServerPacket) {
    auto uc = players.at(playerId).updateCounter;

    return uc != 0.f && std::abs(uc - lastServerPacket) > 0.5f;
}

float PlayerInterpolator::getLocalTs() {
    auto* gpl = static_cast<GlobedPlayLayer*>(PlayLayer::get());
    return gpl->m_fields->timeCounter;
}

PlayerInterpolator::LerpFrame::LerpFrame() {
    timestamp = 0.f;
    visual = {};
}

PlayerInterpolator::LerpFrame::LerpFrame(const PlayerData& data) {
    timestamp = data.timestamp;
    visual = data;
}
