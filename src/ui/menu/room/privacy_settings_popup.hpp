#pragma once

#include <defs/geode.hpp>

enum class PSetting;

class PrivacySettingsPopup : public geode::Popup<> {
public:
    static PrivacySettingsPopup* create();

protected:
    bool setup() override;

    void sendPacket();
    void addButton(PSetting setting, const char* onSprite, const char* offSprite);
    void onDescriptionClicked(PSetting setting);

    cocos2d::CCMenu* buttonMenu;
};
