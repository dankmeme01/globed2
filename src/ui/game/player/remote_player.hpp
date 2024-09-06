#pragma once
#include <defs/all.hpp>

#include "complex_visual_player.hpp"
#include <ui/game/progress/progress_icon.hpp>
#include <ui/game/progress/progress_arrow.hpp>
#include <data/types/gd.hpp>
#include <game/visual_state.hpp>
#include <game/camera_state.hpp>

class GLOBED_DLL RemotePlayer : public cocos2d::CCNode {
public:
    bool init(GameCameraState* gameCameraState, PlayerProgressIcon* progressIcon, PlayerProgressArrow* progressArrow, const PlayerAccountData& data);
    void updateAccountData(const PlayerAccountData& data, bool force = false);
    const PlayerAccountData& getAccountData() const;

    void updateData(
        const VisualPlayerState& data,
        FrameFlags frameFlags,
        bool speaking,
        float loudness
    );
    void updateProgressIcon();
    void updateProgressArrow(
        cocos2d::CCPoint cameraOrigin,
        cocos2d::CCSize cameraCoverage,
        cocos2d::CCPoint visibleOrigin,
        cocos2d::CCSize visibleCoverage,
        float zoom
    );

    void onExit() override;

    unsigned int getDefaultTicks();
    void setDefaultTicks(unsigned int ticks);
    void incDefaultTicks();
    void removeProgressIndicators();

    void setForciblyHidden(bool state);
    bool getForciblyHidden();

    bool isValidPlayer();

    static RemotePlayer* create(GameCameraState* gameCameraState, PlayerProgressIcon* progressIcon, PlayerProgressArrow* progressArrow, const PlayerAccountData& data);
    static RemotePlayer* create(GameCameraState* gameCameraState, PlayerProgressIcon* progressIcon, PlayerProgressArrow* progressArrow);

    Ref<PlayerProgressIcon> progressIcon;
    Ref<PlayerProgressArrow> progressArrow;
    ComplexVisualPlayer* player1;
    ComplexVisualPlayer* player2;

    FrameFlags lastFrameFlags;
    VisualPlayerState lastVisualState;

protected:
    unsigned int defaultTicks = 0;
    float lastPercentage = 0.f;
    bool wasPracticing = false;
    bool isForciblyHidden = false;
    bool isEditorBuilding = false;


    GameCameraState* gameCameraState;

    PlayerAccountData accountData;
};