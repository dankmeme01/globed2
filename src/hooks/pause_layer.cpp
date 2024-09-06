#include "pause_layer.hpp"

#include <hooks/gjbasegamelayer.hpp>
#include <ui/game/userlist/userlist.hpp>
#include <ui/game/chat/chatlist.hpp>
#include <ui/game/chat/unread_badge.hpp>
#include <util/lowlevel.hpp>

using namespace geode::prelude;

void GlobedPauseLayer::customSetup() {
    PauseLayer::customSetup();

    auto* gpl = GlobedGJBGL::get();

    if (!gpl || !gpl->established()) return;

    auto winSize = CCDirector::get()->getWinSize();

    auto* menu = Build<CCMenu>::create()
        .id("playerlist-menu"_spr)
        .pos(0.f, 0.f)
        .parent(this)
        .collect();

    Build<CCSprite>::createSpriteName("icon-players.png"_spr)
        .scale(0.9f)
        .intoMenuItem([](auto) {
            GlobedUserListPopup::create()->show();
        })
        .pos(winSize.width - 50.f, 50.f)
        .id("btn-open-playerlist"_spr)
        .parent(menu);

    // TODO chat: bring back when it works properly
    // auto* chatIcon = Build<CCSprite>::createSpriteName("icon-chat.png"_spr)
    //     .scale(0.9f)
    //     .intoMenuItem([](auto) {
    //         GlobedChatListPopup::create()->show();
    //     })
    //     .pos(winSize.width - 50.f, 90.f)
    //     .id("btn-open-chatlist"_spr)
    //     .parent(menu)
    //     .collect()
    //     ;

    // // todo make it not 3
    // Build<UnreadMessagesBadge>::create(3)
    //     .pos(chatIcon->getScaledContentSize() - CCPoint{5.f, 5.f})
    //     .scale(0.7f)
    //     .id("unread-messages-icon"_spr)
    //     .parent(chatIcon)
    //     ;

    this->schedule(schedule_selector(GlobedPauseLayer::selUpdate), 0.f);
}

void GlobedPauseLayer::selUpdate(float dt) {
    auto* pl = GlobedGJBGL::get();

    if (pl) {
        pl->pausedUpdate(dt);
    }
}

void GlobedPauseLayer::goEdit() {
    auto* pl = GlobedGJBGL::get();
    if (pl) {
        // make sure we remove listeners and stuff
        pl->onQuitActions();
    }

    PauseLayer::goEdit();
}

bool GlobedPauseLayer::hasPopup() {
    if (!this->getParent()) return false;

    return getChildOfType<GlobedUserListPopup>(this->getParent(), 0) != nullptr;
}

#define REPLACE(method) \
    void GlobedPauseLayer::method(CCObject* s) {\
        if (!this->hasPopup()) { \
            PauseLayer::method(s); \
        } \
    }

REPLACE(onQuit);
REPLACE(onResume);
REPLACE(onEdit);
REPLACE(onNormalMode);
REPLACE(onPracticeMode);

/* deathlink */

void GlobedPauseLayer::onRestart(CCObject* s) {
    if (this->hasPopup()) return;

    GlobedGJBGL::get()->m_fields->isManuallyResettingLevel = true;
    PauseLayer::onRestart(s);
    GlobedGJBGL::get()->m_fields->isManuallyResettingLevel = false;
}

void GlobedPauseLayer::onRestartFull(CCObject* s) {
    if (this->hasPopup()) return;

    GlobedGJBGL::get()->m_fields->isManuallyResettingLevel = true;
    PauseLayer::onRestartFull(s);
    GlobedGJBGL::get()->m_fields->isManuallyResettingLevel = false;
}
