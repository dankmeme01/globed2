#pragma once

#include "status_icons.hpp"
#include <hooks/player_object.hpp>
#include <game/visual_state.hpp>
#include <game/camera_state.hpp>
#include <data/types/gd.hpp>
#include <data/types/game.hpp>
#include <ui/general/name_label.hpp>

class RemotePlayer;

class GLOBED_DLL ComplexVisualPlayer : public cocos2d::CCNode {
public:
    static constexpr int SPIDER_DASH_CIRCLE_WAVE_TAG = 234562345;
    static constexpr int SPIDER_DASH_SPRITE_TAG = 234562347;
    static constexpr int DEATH_EFFECT_TAG = 234562349;

    bool init(RemotePlayer* parent, bool isSecond);
    void updateIcons(const PlayerIconData& icons);
    void updateData(
        const SpecificIconData& data,
        const VisualPlayerState& playerData,
        const GameCameraState& camState,
        bool isSpeaking,
        float loudness
    );
    void updateIconType(PlayerIconType newType);
    void playDeathEffect();
    void playSpiderTeleport(const SpiderTeleportData& data);
    void playJump();
    void setForciblyHidden(bool state);
    const cocos2d::CCPoint& getPlayerPosition();
    cocos2d::CCNode* getPlayerObject();
    RemotePlayer* getRemotePlayer();

    void setP1StickyState(bool state);
    void setP2StickyState(bool state);

    bool getP1StickyState();
    bool getP2StickyState();

    void updatePlayerObjectIcons(bool skipFrames = false);
    void toggleAllOff();
    void callToggleWith(PlayerIconType type, bool arg1, bool arg2);
    void callUpdateWith(PlayerIconType type, int icon);

    static ComplexVisualPlayer* create(RemotePlayer* parent, bool isSecond);

protected:
    friend class ComplexPlayerObject;
    friend class RemotePlayer;
    RemotePlayer* parent;

    bool isSecond;

    GJBaseGameLayer* gameLayer;
    ComplexPlayerObject* playerIcon;
    Ref<GlobedNameLabel> nameLabel;
    PlayerIconType playerIconType = PlayerIconType::Unknown;
    Ref<PlayerStatusIcons> statusIcons;
    bool isPlatformer;
    bool isEditor;

    // these 3 used in robot and spider anims
    bool wasGrounded = false;
    bool wasStationary = true;
    bool wasFalling = false;

    // used in spider anims
    cocos2d::ccColor3B storedMainColor, storedSecondaryColor;
    float tpColorDelta = 0.f;

    // used in swing anims
    bool wasUpsideDown = false;

    // used in platformer squish anim
    bool wasRotating = false;
    bool didPerformPlatformerJump = false;

    // used in dashing anim
    bool wasDashing = false;

    // used to call onEnter, onExit
    bool wasPaused = false;

    // used for many anims
    bool wasNearby = false;

    // uhh yeah forcibly hiding players
    bool isForciblyHidden = false;

    // used for player collision stickiness
    bool p1sticky = false, p2sticky = false;

    PlayerIconData storedIcons;

    // used for async icon loading
    struct AsyncLoadRequest {
        int key;
        int iconId;
        PlayerIconType iconType;
    };

    int iconsLoaded = 0;
    std::unordered_map<int, AsyncLoadRequest> asyncLoadRequests;

    static constexpr int ROBOT_FIRE_ACTION = 1000727;
    static constexpr int SWING_FIRE_ACTION = 1000728;
    static constexpr int SPIDER_TELEPORT_COLOR_ACTION = 1000729;

    void spiderTeleportUpdateColor();

    void updateRobotAnimation();
    void updateSpiderAnimation();

    void animateRobotFire(bool enable);
    void onAnimateRobotFireIn();
    void onAnimateRobotFireOut();

    void animateSwingFire(bool goingDown);
    void updateOpacity();

    void tryLoadIconsAsync();
    void onFinishedLoadingIconAsync();
    // fucking hell i hate this
    void asyncIconLoadedIntermediary(cocos2d::CCObject*);

    void cancelPlatformerJumpAnim();
    void enableTrail();
    void disableTrail();

    bool isPlayerNearby(const GameCameraState& camState);
};