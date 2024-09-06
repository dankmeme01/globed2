#include <defs/all.hpp>

#if GLOBED_HAS_KEYBINDS
# include <geode.custom-keybinds/include/Keybinds.hpp>
#endif

#include <asp/Log.hpp>

#include <hooks/all.hpp>
#include <audio/manager.hpp>
#include <crypto/box.hpp>
#include <managers/settings.hpp>
#include <ui/error_check_node.hpp>
#include <ui/notification/panel.hpp>
#include <util/all.hpp>

using namespace geode::prelude;

void setupErrorCheckNode();
void setupCustomKeybinds();
void printDebugInfo();
void fixCVAbiBreak();

#ifdef GLOBED_VOICE_SUPPORT
// TODO this hook kinda doesnt work bc early load and i dont even know if it would do anything
static void FMODSystemInitHook(FMOD::System* system, int channels, FMOD_INITFLAGS flags, void* dd) {
    log::debug("fmod system init hooked, changing to {} channels", MAX_AUDIO_CHANNELS);
    system->init(MAX_AUDIO_CHANNELS, flags, dd);
}
#endif // GLOBED_VOICE_SUPPORT

$execute {
    using namespace asp;

    asp::setLogFunction([](LogLevel level, auto message) {
        switch (level) {
            case LogLevel::Trace: [[fallthrough]];
            case LogLevel::Debug: log::debug("[asp] {}", message); break;
            case LogLevel::Info: log::info("[asp] {}", message); break;
            case LogLevel::Warn: log::warn("[asp] {}", message); break;
            case LogLevel::Error: log::error("[asp] {}", message); break;
        }
    });

    // auto& rt = asp::async::Runtime::get();
    // rt.launch();
}

$on_mod(Loaded) {
#ifdef GLOBED_VOICE_SUPPORT
    // (void) Mod::get()->hook(
    //     reinterpret_cast<void*>(
    //         geode::addresser::getNonVirtual(
    //             &FMOD::System::init
    //         )
    //     ),
    //     &FMODSystemInitHook,
    //     "FMOD::System::init",
    //     tulip::hook::TulipConvention::Stdcall
    // ).expect("failed to hook fmod").unwrap();
#endif // GLOBED_VOICE_SUPPORT

    CryptoBox::initLibrary();
    setupErrorCheckNode();
    setupCustomKeybinds();

#ifdef GLOBED_VOICE_SUPPORT
    GlobedAudioManager::get().preInitialize();
#endif

#if defined(GLOBED_DEBUG) && GLOBED_DEBUG
    printDebugInfo();
#endif

    fixCVAbiBreak();
}

// error check node runs on every scene and shows popups/notifications if an error has occured in another thread
void setupErrorCheckNode() {
    auto ecn = ErrorCheckNode::create();
    ecn->setID("error-check-node"_spr);
    ecn->retain();
    ecn->onEnter(); // needed so scheduled methods get called

    auto notif = GlobedNotificationPanel::create();
    notif->setID("notification-panel"_spr);
    notif->persist();
}

void setupCustomKeybinds() {
#if GLOBED_HAS_KEYBINDS
    using namespace keybinds;

    BindManager::get()->registerBindable({
        "voice-activate"_spr,
        "Voice",
        "Records audio from your microphone and sends it off to other users on the level.",
        { Keybind::create(KEY_V, Modifier::None) },
        Category::PLAY,
    });

    BindManager::get()->registerBindable({
        "voice-deafen"_spr,
        "Deafen",
        "Mutes voices of other players when toggled.",
        { Keybind::create(KEY_B, Modifier::None) },
        Category::PLAY,
    });
#endif // GLOBED_HAS_KEYBINDS
}

// just debug printing
void printDebugInfo() {
    std::string version = Mod::get()->getVersion().toString();
    unsigned int fv = 0;

#if GLOBED_HAS_FMOD
    FMODAudioEngine::sharedEngine()->m_system->getVersion(&fv);
#endif

    log::warn("=== Globed {} has been loaded in debug mode ===", version.starts_with('v') ? version : ("v" + version));
    log::info("Platform: {} ({}-endian)", GLOBED_PLATFORM_STRING, GLOBED_LITTLE_ENDIAN ? "little" : "big");
    log::info("FMOD linkage: {}, version: {:X}", GLOBED_HAS_FMOD == 0 ? "false" : "true", fv);
#ifdef GLOBED_VOICE_SUPPORT
    log::info("Voice chat support: true (opus version: {})", GlobedAudioManager::getOpusVersion());
#else
    log::info("Voice chat support: false");
#endif
    log::info("Discord RPC support: {}", GLOBED_HAS_DRPC == 0 ? "false" : "true");
    log::info("Libsodium version: {} (CryptoBox algorithm: {})", CryptoBox::sodiumVersion(), CryptoBox::algorithm());
}

// Ok so this is really cursed but let me explain
// somewhere sometime recently microsoft stl brokey the internal mutex or cv structure or whatever
// and wine kinda doesnt wanna work with it (even with most recent redistributable package)
// tbh i dont entirely understand this either but hey it is what it is

// this applies to latest sdk. the --sdk-version argument in xwin does nothing here.
//
// if you want to compile globed on linux without broken conditional variables, the steps are rather simple:
// 1. look up https://aka.ms/vs/17/release/channel on the web archive
// 2. get the entry from july, inside the file you should see version as "17.10", not "17.11" or later
// 3. open .xwin-cache/dl/manifest_17.json and replace the contents
// 4. run xwin install command again and it should work now!

// ALTERNATIVELY

// if you are very pissed, just change the '#if 0' to '#if 1'.
// it is a temp fix and it WILL break things. but it does not crash on startup anymore! (most of the time)

#if 0

static bool isWine() {
    return GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version");
}

static inline bool _Primitive_wait_for(const _Cnd_t cond, const _Mtx_t mtx, unsigned int timeout) noexcept {
    const auto pcv  = reinterpret_cast<PCONDITION_VARIABLE>((uintptr_t)cond + 8);
    const auto psrw = reinterpret_cast<PSRWLOCK>(&mtx->_Critical_section._Unused); // changed from &mtx->_Critical_section._M_srw_lock
    return SleepConditionVariableSRW(pcv, psrw, timeout, 0) != 0;
}

static _Thrd_result __stdcall _Cnd_timedwait_for_reimpl(_Cnd_t cond, _Mtx_t mtx, unsigned int target_ms) noexcept {
    _Thrd_result res            = _Thrd_result::_Success;
    unsigned long long start_ms = 0;

    start_ms = GetTickCount64();

    // TRANSITION: replace with _Mtx_clear_owner(mtx);
    mtx->_Thread_id = -1;
    --mtx->_Count;

    if (!_Primitive_wait_for(cond, mtx, target_ms)) { // report timeout
        if (GetTickCount64() - start_ms >= target_ms) {
            res = _Thrd_result::_Timedout;
        }
    }
    // TRANSITION: replace with _Mtx_reset_owner(mtx);
    mtx->_Thread_id = static_cast<long>(GetCurrentThreadId());
    ++mtx->_Count;

    return res;
}

void fixCVAbiBreak() {
    if (!isWine()) return;

    // TODO: im not sure tbh does this bug exist when compiled on windows too?

#ifdef GLOBED_LINUX_COMPILATION
    if (Loader::get()->getLaunchFlag("globed-crt-fix")) {
        (void) Mod::get()->hook(
            reinterpret_cast<void*>(addresser::getNonVirtual(&_Cnd_timedwait_for)),
            _Cnd_timedwait_for_reimpl,
            "_Cnd_timedwait_for",
            tulip::hook::TulipConvention::Default
        );
    }
#endif
}

#else // GEODE_IS_WINDOWS

void fixCVAbiBreak() {} // nothing.

#endif // GEODE_IS_WINDOWS
