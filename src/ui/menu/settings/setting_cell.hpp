#pragma once
#include <defs/all.hpp>

#include <any>

class GlobedSettingCell : public cocos2d::CCLayer, public TextInputDelegate {
public:
    enum class Type {
        Bool, Int, Float, String, AudioDevice, Corner, PacketFragmentation, AdvancedSettings, DiscordRPC, InvitesFrom, LinkCode
    };

    struct Limits {
        float floatMin = 0.f, floatMax = 0.f;
        int intMin = 0, intMax = 0;
    };

    static constexpr float CELL_WIDTH = 358.0f;
    static constexpr float CELL_HEIGHT = 37.5f;

    // The character parameters must be string literals, or must exist for the entire lifetime of the cell.
    static GlobedSettingCell* create(void*, Type, const char*, const char*, const Limits&);

    void storeAndSave(std::any&& value);

private:
    void* settingStorage;
    Type settingType;
    const char* descText;
    Limits limits;

    cocos2d::CCLabelBMFont* labelName;
    CCMenuItemSpriteExtra* btnInfo = nullptr;

    CCMenuItemToggler* inpCheckbox = nullptr;
    Slider* inpSlider = nullptr;
    geode::InputNode* inpField = nullptr;
    CCMenuItemSpriteExtra* inpAudioButton = nullptr;

    CCMenuItemSpriteExtra* cornerButton = nullptr;
    CCMenuItemSpriteExtra* invitesFromButton = nullptr;

    bool init(void*, Type, const char*, const char*, const Limits&);
    void onCheckboxToggled(cocos2d::CCObject*);
    void onSliderChanged(cocos2d::CCObject*);
    void onInteractiveButton(cocos2d::CCObject*);
    void onStringChanged(const std::string_view);

    void recreateCornerButton();
    void recreateInvitesFromButton();

    void textChanged(CCTextInputNode* p0);
    void textInputOpened(CCTextInputNode* p0);
    void textInputClosed(CCTextInputNode* p0);
    void textInputShouldOffset(CCTextInputNode* p0, float p1);
    void textInputReturn(CCTextInputNode* p0);
    bool allowTextInput(CCTextInputNode* p0);
    void enterPressed(CCTextInputNode* p0);
};