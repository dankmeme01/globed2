#include "play_layer.hpp"

#include "gjbasegamelayer.hpp"
#include "level_editor_layer.hpp"
#include <game/module/all.hpp>
#include <managers/settings.hpp>
#include <util/lowlevel.hpp>
#include <util/cocos.hpp>
#include <util/gd.hpp>

using namespace geode::prelude;

// post an event to all modules
#define GLOBED_EVENT(self, code) \
    for (auto& module : self->m_fields->modules) { \
        module->code; \
    }

// post an event to all modules
#define GLOBED_EVENT_O(self, code) \
    for (auto& module : self->m_fields->modules) { \
        auto _mo = module->code; \
        if (_mo == BaseGameplayModule::EventOutcome::Halt) return; \
    }

/* Hooks */

bool GlobedPlayLayer::init(GJGameLevel* level, bool p1, bool p2) {
    GlobedLevelEditorLayer::fromEditor = false;

    auto gjbgl = static_cast<GlobedGJBGL*>(static_cast<GJBaseGameLayer*>(this));

    gjbgl->setupPreInit(level, false);

    auto& settings = GlobedSettings::get();
    if (settings.levelUi.forceProgressBar && gjbgl->m_fields->globedReady) {
        auto gm = GameManager::sharedState();
        m_fields->oldShowProgressBar = gm->m_showProgressBar;
        gm->m_showProgressBar = true;
    }

    if (!PlayLayer::init(level, p1, p2)) return false;

    gjbgl->setupAll();

    return true;
}

void GlobedPlayLayer::setupHasCompleted() {
    // loadDeathEffect is inlined on win64, we want to avoid unloading anything
    auto* gm = GameManager::get();
    int lastLoadedEffect = gm->m_loadedDeathEffect;
    int lastDeathEffect = gm->m_playerDeathEffect;

    gm->m_loadedDeathEffect = 0;
    gm->m_playerDeathEffect = 0;

    PlayLayer::setupHasCompleted();

    gm->m_loadedDeathEffect = lastLoadedEffect;
    gm->m_playerDeathEffect = lastDeathEffect;

    util::cocos::tryLoadDeathEffect(lastDeathEffect);

    auto gjbgl = static_cast<GlobedGJBGL*>(static_cast<GJBaseGameLayer*>(this));
    gjbgl->m_fields->setupWasCompleted = true;
}

void GlobedPlayLayer::onQuit() {
    auto gjbgl = GlobedGJBGL::get();
    auto& fields = this->getFields();

    if (fields.oldShowProgressBar) {
        auto gm = GameManager::sharedState();
        gm->m_showProgressBar = fields.oldShowProgressBar.value();
    }
    gjbgl->onQuitActions();

    PlayLayer::onQuit();
}

void GlobedPlayLayer::fullReset() {
    auto gjbgl = GlobedGJBGL::get();

    GLOBED_EVENT_O(gjbgl, fullResetLevel());

    PlayLayer::fullReset();

    // turn off safe mode
    GlobedGJBGL::get()->toggleSafeMode(false);
}

void GlobedPlayLayer::resetLevel() {
    auto gjbgl = GlobedGJBGL::get();
    auto& fields = this->getFields();

    GLOBED_EVENT_O(gjbgl, resetLevel());

    if (fields.insideDestroyPlayer) {
        fields.insideDestroyPlayer = false;
    }

    bool lastTestMode = m_isTestMode;

    if (GlobedGJBGL::get()->isSafeMode()) {
        m_isTestMode = true;
    }

    PlayLayer::resetLevel();

    m_isTestMode = lastTestMode;

    // this is also called upon init, so bail out if we are too early
    if (!gjbgl->m_fields->setupWasCompleted) return;

    if (!m_level->isPlatformer()) {
        // turn off safe mode in non-platformer levels (as this counts as a full reset)
        GlobedGJBGL::get()->toggleSafeMode(false);
    }
}

void GlobedPlayLayer::showNewBest(bool p0, int p1, int p2, bool p3, bool p4, bool p5) {
    // doesn't actually change any progress but this stops the NEW BEST popup from showing up while cheating/jumping to a player
    if (!GlobedGJBGL::get()->isSafeMode()) PlayLayer::showNewBest(p0, p1, p2, p3, p4, p5);
}

void GlobedPlayLayer::levelComplete() {
    if (!GlobedGJBGL::get()->isSafeMode()) PlayLayer::levelComplete();
    else GlobedPlayLayer::onQuit();
}

void GlobedPlayLayer::destroyPlayer(PlayerObject* player, GameObject* object) {
    auto& fields = this->getFields();

    if (!fields.antiCheat) {
        fields.antiCheat = object;
    }

    auto* pl = GlobedGJBGL::get();

    GLOBED_EVENT_O(pl, destroyPlayerPre(player, object));

    // safe mode stuff yeah

    bool lastTestMode = m_isTestMode;

    if (GlobedGJBGL::get()->isSafeMode()) {
        m_isTestMode = true;
    }

    fields.insideDestroyPlayer = true;

#ifdef GEODE_IS_ARM_MAC
# if GEODE_COMP_GD_VERSION != 22060
#  error "update this patch for new gd"
# else
    static auto armpatch = [] {
        return util::lowlevel::nop(0xa73e4, 0x4);
    }();

    if (armpatch) {
        if (GlobedGJBGL::get()->m_fields->shouldStopProgress) {
            (void) armpatch->enable();
        } else {
            (void) armpatch->disable();
        }
    }

    PlayLayer::destroyPlayer(player, object);

    if (armpatch && armpatch->isEnabled()) {
        (void) armpatch->disable();
    }
# endif
#else
    PlayLayer::destroyPlayer(player, object);
#endif


    GLOBED_EVENT(pl, destroyPlayerPost(player, object));

    m_isTestMode = lastTestMode;
}

void GlobedPlayLayer::forceKill(PlayerObject* p) {
    auto& fields = this->getFields();

    fields.ignoreNoclip = true;
    this->PlayLayer::destroyPlayer(p, nullptr);
    fields.ignoreNoclip = false;
}

GlobedPlayLayer::Fields& GlobedPlayLayer::getFields() {
    return *m_fields.self();
}
