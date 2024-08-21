#include "two_player_mode.hpp"

#include <managers/room.hpp>
#include <hooks/gjbasegamelayer.hpp>
#include <hooks/play_layer.hpp>
#include <ui/game/userlist/user_cell.hpp>
#include <util/gd.hpp>

using EventOutcome = BaseGameplayModule::EventOutcome;
using UserCellButton = BaseGameplayModule::UserCellButton;
using util::gd::GameVariable;
using namespace geode::prelude;

TwoPlayerModeModule::TwoPlayerModeModule(GlobedGJBGL* gameLayer) : BaseGameplayModule(gameLayer) {
    this->isPrimary = RoomManager::get().isOwner();
}

void TwoPlayerModeModule::mainPlayerUpdate(PlayerObject* player, float dt) {
    if (!player->getUserObject(LOCKED_TO_KEY)) return;

    PlayerObject* noclipFor = this->isPrimary ? gameLayer->m_player2 : gameLayer->m_player1;

    if (player == noclipFor && this->linked) {
        this->updateFromLockedPlayer(player, !isPrimary && gameLayer->m_gameState.m_isDualMode);
        player->setVisible(false);
        // if (bgl->m_gameState.m_isDualMode) {
        //     this->m_isHidden = true;
        // }

        player->m_playEffects = false;
        if (player->m_regularTrail) player->m_regularTrail->setVisible(false);
        if (player->m_waveTrail) player->m_waveTrail->setVisible(false);
        if (player->m_ghostTrail) player->m_ghostTrail->setVisible(false);
        if (player->m_trailingParticles) player->m_trailingParticles->setVisible(false);
        if (player->m_shipStreak) player->m_shipStreak->setVisible(false);
    }
}

void TwoPlayerModeModule::updateFromLockedPlayer(PlayerObject* player, bool ignorePos) {
    ComplexVisualPlayer* cvp = static_cast<ComplexVisualPlayer*>(player->getUserObject(LOCKED_TO_KEY));
    if (!cvp) return;

    RemotePlayer* rp = cvp->getRemotePlayer();
    log::debug("update from locked {}, death: {}", rp->getAccountData().name, rp->lastFrameFlags.pendingDeath);

    if (rp->lastFrameFlags.pendingDeath) {
        Loader::get()->queueInMainThread([] {
            static_cast<GlobedPlayLayer*>(PlayLayer::get())->forceKill(PlayLayer::get()->m_player1);
        });
    }

    if (!ignorePos && !rp->lastVisualState.isDead) {
        player->setPosition(cvp->getPlayerPosition());
    }
}

EventOutcome TwoPlayerModeModule::resetLevel() {
    auto pl = this->getPlayLayer();
    if (!pl || !this->linked) return EventOutcome::Continue;

    bool indp = pl->m_fields->insideDestroyPlayer;

    if (gameLayer->m_fields->setupWasCompleted && !indp) {
        pl->forceKill(isPrimary ? pl->m_player1 : pl->m_player2);
        return EventOutcome::Halt;
    }

    return EventOutcome::Continue;
}

EventOutcome TwoPlayerModeModule::destroyPlayerPre(PlayerObject* player, GameObject* object) {
    auto pl = this->getPlayLayer();

    if (!pl->m_fields->ignoreNoclip && this->linked) {
        PlayerObject* noclipFor = this->isPrimary ? gameLayer->m_player2 : gameLayer->m_player1;

        if (pl->m_fields->antiCheat != object && player == noclipFor) {
            // noclip
            // log::debug("noclipping for {}", noclipFor == m_player1 ? "player 1" : "player 2");
            return EventOutcome::Halt;
        }
    }

    // if deathlink or 2p mode is enabled, toggle faster reset off
    this->oldFastReset = util::gd::variable(GameVariable::FastRespawn);
    util::gd::setVariable(GameVariable::FastRespawn, false);

    return EventOutcome::Continue;
}

void TwoPlayerModeModule::destroyPlayerPost(PlayerObject* player, GameObject* object) {
    util::gd::setVariable(GameVariable::FastRespawn, this->oldFastReset);
}

void TwoPlayerModeModule::linkPlayerTo(int accountId) {
    log::debug("Link attempt to {}", accountId);
    if (!gameLayer->m_fields->players.contains(accountId)) return;

    RemotePlayer* rp = gameLayer->m_fields->players.at(accountId);

    log::debug("Linking to {}", rp->getAccountData().name);

    PlayerObject* ignored = this->isPrimary ? gameLayer->m_player2 : gameLayer->m_player1;

    ignored->setUserObject(LOCKED_TO_KEY, isPrimary ? rp->player2 : rp->player1);

    this->linked = true;
}

// TODO: test if still needed for 2 player mode
// void TwoPlayerModeModule::updateCamera(float dt) {
//     if (!m_fields->twopstate.active || m_fields->twopstate.isPrimary || !m_gameState.m_isDualMode) {
//         GJBaseGameLayer::updateCamera(dt);
//         return;
//     }

//     auto lastPos = m_player1->getPosition();
//     m_player1->setPosition({m_player2->getPositionX(), lastPos.y});
//     GJBaseGameLayer::updateCamera(dt);
//     m_player1->setPosition(lastPos);
//

std::vector<UserCellButton> TwoPlayerModeModule::onUserActionsPopup(int accountId, bool self) {
    if (self) return {};

    return std::vector({UserCellButton {
        .spriteName = "gj_linkBtn_001.png",
        .id = "2p-link-btn"_spr,
        .callback = [this, accountId](CCObject* sender){
            this->linkPlayerTo(accountId);
        }
    }});
}

