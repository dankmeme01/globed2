#pragma once

#include <defs/geode.hpp>
#include <defs/util.hpp>
#include <data/basic.hpp>

#include <data/types/user.hpp>

#include <util/singleton.hpp>

class GlobedSettings : public SingletonBase<GlobedSettings> {
    friend class SingletonBase;
    GlobedSettings();

public:
    // Save all settings to the geode save container
    void save();

    // T -> T, but float -> globed::ConstexprFloat, as floats cant be used as template arguments
    template <typename T>
    using TypeFixup = std::conditional_t<std::is_same_v<T, float>, globed::ConstexprFloat, T>;

    /* Setting class */

    template <
        typename InnerTyRaw,
        TypeFixup<InnerTyRaw> DefaultV
    >
    class Setting {
    protected:
        using InnerTy = decltype(DefaultV);

    public:
        constexpr static bool IsFloat = std::is_same_v<InnerTy, globed::ConstexprFloat>;
        constexpr static bool IsLimited = false;

        using Type = std::conditional_t<IsFloat, float, InnerTy>;
        constexpr static Type Default = DefaultV;

        Setting() : value(Default) {}

        void set(const Type& v) {
            value = v;
        }

        Type get() const {
            return value;
        }

        Type& ref() {
            if constexpr (IsFloat) {
                return value.ref();
            } else {
                return value;
            }
        }

        const Type& ref() const {
            if constexpr (IsFloat) {
                return value.ref();
            } else {
                return value;
            }
        }

        operator Type() {
            return get();
        }

        operator float() const requires (IsFloat) {
            return get().asFloat();
        }

        Setting& operator=(const Type& other) {
            value = other;
            GlobedSettings::get().save();
            return *this;
        }

    protected:
        InnerTy value;
    };

    template <
        typename InnerTyRaw,
        TypeFixup<InnerTyRaw> DefaultV,
        TypeFixup<InnerTyRaw> MinimumV,
        TypeFixup<InnerTyRaw> MaximumV
    >
    class LimitedSetting : public Setting<InnerTyRaw, DefaultV> {
    public:
        using Type = Setting<InnerTyRaw, DefaultV>::Type;
        using InnerTy = Setting<InnerTyRaw, DefaultV>::InnerTy;
        using Setting<InnerTyRaw, DefaultV>::IsFloat;
        using Setting<InnerTyRaw, DefaultV>::Default;

        constexpr static bool IsLimited = true;

        constexpr static Type Minimum = MinimumV;
        constexpr static Type Maximum = MaximumV;

        LimitedSetting() {
            this->value = Default;
        }

        void set(const Type& v) {
            this->value = this->clamp(v);
        }

        operator Type() {
            return this->get();
        }

        operator float() const requires (IsFloat) {
            return this->get().asFloat();
        }

        LimitedSetting& operator=(const Type& other) {
            this->set(other);
            GlobedSettings::get().save();
            return *this;
        }

    private:
        Type clamp(const Type& value) {
            if (value > Maximum) return Maximum;
            if (value < Minimum) return Minimum;
            return value;
        }
    };

    using Float = globed::ConstexprFloat;
    using Flag = Setting<bool, false>;

    // Settings themselves, split into categories
    // when adding settings, please remember to also add them at the bottom of this file

    enum class InvitesFrom : int {
        Everyone = 0,
        Friends = 1,
        Nobody = 2,
    };

    struct Globed {
        Setting<bool, true> autoconnect;
        LimitedSetting<int, 0, 0, 240> tpsCap;
        Setting<bool, true> preloadAssets;
        Setting<bool, false> deferPreloadAssets;
        LimitedSetting<int, (int)InvitesFrom::Friends, 0, 2> invitesFrom;
        Setting<bool, true> editorSupport;
        Setting<bool, false> increaseLevelList;
        Setting<int, 60000> fragmentationLimit;
        Setting<bool, false> compressedPlayerCount;
        Setting<bool, true> useDiscordRPC;
        Setting<bool, true> playerCountOnLvlPage;

        // hidden settings! no settings ui for them

        Setting<bool, true> changelogPopups;
        Setting<bool, false> pinnedLevelCollapsed;

        /* privacy settings */
        Setting<bool, false> isInvisible;
        Setting<bool, false> noInvites;
        Setting<bool, false> hideInGame;
        Setting<bool, false> hideRoles;
    };

    struct Overlay {
        Setting<bool, true> enabled;
        LimitedSetting<float, 0.3f, 0.f, 1.f> opacity;
        Setting<bool, true> hideConditionally;
        LimitedSetting<int, 3, 0, 3> position; // 0-3 topleft, topright, bottomleft, bottomright
    };

    struct Communication {
        Setting<bool, true> voiceEnabled;
        Setting<bool, true> voiceProximity;
        Setting<bool, false> classicProximity;
        LimitedSetting<float, 1.0f, 0.f, 2.f> voiceVolume;
        Setting<bool, false> onlyFriends;
        Setting<bool, true> lowerAudioLatency;
        Setting<int, 0> audioDevice;
        Setting<bool, true> deafenNotification;
        Setting<bool, false> voiceLoopback; // TODO unimpl
    };

    struct LevelUI {
        Setting<bool, true> progressIndicators;
        Setting<bool, true> progressPointers; // unused
        LimitedSetting<float, 1.0f, 0.f, 1.f> progressOpacity;
        Setting<bool, true> voiceOverlay;
        Setting<bool, false> forceProgressBar;
    };

    struct Players {
        LimitedSetting<float, 1.0f, 0.f, 1.f> playerOpacity;
        Setting<bool, true> showNames;
        Setting<bool, true> dualName;
        LimitedSetting<float, 1.0f, 0.f, 1.f> nameOpacity;
        Setting<bool, true> statusIcons;
        Setting<bool, true> deathEffects;
        Setting<bool, false> defaultDeathEffect;
        Setting<bool, false> hideNearby;
        Setting<bool, false> forceVisibility;
        Setting<bool, false> ownName;
        Setting<bool, false> rotateNames;
        Setting<bool, false> hidePracticePlayers;
    };

    struct Advanced {};

    struct Admin {
        Setting<bool, false> rememberPassword;
    };

    struct Flags {
        Flag seenSignupNotice;       // Obsolete
        Flag seenSignupNoticev2;
        Flag seenVoiceChatPTTNotice;
        Flag seenTeleportNotice;
        Flag seenAprilFoolsNotice;   // Obsolete
        Flag seenStatusNotice;       // Obsolete
    };

    Globed globed;
    Overlay overlay;
    Communication communication;
    LevelUI levelUi;
    Players players;
    Advanced advanced;
    Admin admin;
    Flags flags;

    // Reset everything, including flags
    void hardReset();

    // Reset all settings, excluding flags
    void reset();

    // Reload all settings from the geode save container
    void reload();

    enum class TaskType {
        SaveSettings, LoadSettings, ResetSettings, HardResetSettings
    };

    void reflect(TaskType type);

    bool has(std::string_view key);
    void clear(std::string_view key);

    template <typename T>
    void store(std::string_view key, const T& val) {
        geode::Mod::get()->setSavedValue(key, val);
    }

    template <typename T>
    T load(std::string_view key) {
        return geode::Mod::get()->getSavedValue<T>(key);
    }

    // If setting is present, loads into `into`. Otherwise does nothing.
    template <typename T>
    void loadOptionalInto(std::string_view key, T& into) {
        if (this->has(key)) {
            into = this->load<T>(key);
        }
    }

    template <typename T>
    std::optional<T> loadOptional(std::string_view key) {
        return this->has(key) ? this->load<T>(key) : std::nullopt;
    }

    template <typename T>
    T loadOrDefault(std::string_view key, const T& defaultval) {
        return this->has(key) ? this->load<T>(key) : defaultval;
    }

    UserPrivacyFlags getPrivacyFlags() {
        return UserPrivacyFlags {
            .hideFromLists = globed.isInvisible,
            .noInvites = globed.noInvites,
            .hideInGame = globed.hideInGame,
            .hideRoles = globed.hideRoles,
        };
    }
};

/* Enable reflection */

GLOBED_SERIALIZABLE_STRUCT(GlobedSettings::Globed, (
    autoconnect, tpsCap, preloadAssets, deferPreloadAssets, invitesFrom, editorSupport, increaseLevelList, fragmentationLimit, compressedPlayerCount, useDiscordRPC,
    changelogPopups, pinnedLevelCollapsed, playerCountOnLvlPage,
    isInvisible, noInvites, hideInGame, hideRoles
));

GLOBED_SERIALIZABLE_STRUCT(GlobedSettings::Overlay, (
    enabled, opacity, hideConditionally, position
));

GLOBED_SERIALIZABLE_STRUCT(GlobedSettings::Communication, (
    voiceEnabled, voiceProximity, classicProximity, voiceVolume, onlyFriends, lowerAudioLatency, audioDevice, deafenNotification, voiceLoopback
));

GLOBED_SERIALIZABLE_STRUCT(GlobedSettings::LevelUI, (
    progressIndicators, progressPointers, progressOpacity, voiceOverlay, forceProgressBar
));

GLOBED_SERIALIZABLE_STRUCT(GlobedSettings::Players, (
    playerOpacity, showNames, dualName, nameOpacity, statusIcons, deathEffects, defaultDeathEffect, hideNearby, forceVisibility, ownName, hidePracticePlayers, rotateNames
));

GLOBED_SERIALIZABLE_STRUCT(GlobedSettings::Advanced, ());

GLOBED_SERIALIZABLE_STRUCT(GlobedSettings::Admin, (
    rememberPassword
));

GLOBED_SERIALIZABLE_STRUCT(GlobedSettings::Flags, (
    seenSignupNotice, seenSignupNoticev2, seenVoiceChatPTTNotice, seenTeleportNotice, seenAprilFoolsNotice, seenStatusNotice
));

GLOBED_SERIALIZABLE_STRUCT(GlobedSettings, (
    globed, overlay, communication, levelUi, players, advanced, admin, flags
));
