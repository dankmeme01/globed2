#include "setting_cell.hpp"

#include <asp/math/NumberCycle.hpp>

#include "audio_setup_popup.hpp"
#include "frag_calibration_popup.hpp"
#include "string_input_popup.hpp"
#include "advanced_settings_popup.hpp"
#include "link_code_popup.hpp"
#include <managers/settings.hpp>
#include <net/manager.hpp>
#include <ui/general/ask_input_popup.hpp>
#include <util/format.hpp>
#include <util/misc.hpp>

using namespace geode::prelude;
namespace permission = geode::utils::permission;
using permission::Permission;

bool GlobedSettingCell::init(void* settingStorage, Type settingType, const char* nameText, const char* descText, const Limits& limits) {
    if (!CCLayer::init()) return false;
    this->settingStorage = settingStorage;
    this->settingType = settingType;
    this->descText = descText;
    this->limits = limits;

    Build<CCLabelBMFont>::create(nameText, "bigFont.fnt")
        .scale(0.6f)
        .anchorPoint(0.f, 0.5f)
        .pos(10.f, CELL_HEIGHT / 2)
        .parent(this)
        .store(labelName);

    // desc button
    if (descText && descText[0] != '\0') {
        auto labelSize = labelName->getScaledContentSize();
        Build<CCSprite>::createSpriteName("GJ_infoIcon_001.png")
            .scale(0.4f)
            .intoMenuItem([nameText, descText](auto) {
                FLAlertLayer::create(nameText, descText, "Ok")->show();
            })
            .pos(10.f + labelSize.width + 5.f, CELL_HEIGHT / 2 + labelSize.height / 2 - 5.f)
            .intoNewParent(CCMenu::create())
            .pos(0.f, 0.f)
            .parent(this);
    }

    switch (settingType) {
        case Type::Bool: {
            Build<CCMenuItemToggler>(CCMenuItemToggler::createWithStandardSprites(this, menu_selector(GlobedSettingCell::onCheckboxToggled), 1.0f))
                .anchorPoint(0.5f, 0.5f)
                .pos(CELL_WIDTH - 20.f, CELL_HEIGHT / 2)
                .scale(0.8f)
                .id("input-checkbox"_spr)
                .store(inpCheckbox)
                .intoNewParent(CCMenu::create())
                .pos(0.f, 0.f)
                .id("input-menu"_spr)
                .parent(this);

            inpCheckbox->toggle(*(bool*)(settingStorage));
        } break;
        case Type::DiscordRPC: {
            bool possible = Loader::get()->isModLoaded("techstudent10.discord_rich_presence");
            float opacity = possible ? 1.f : 0.5f;

            Build<CCMenuItemToggler>(CCMenuItemToggler::createWithStandardSprites(this, menu_selector(GlobedSettingCell::onCheckboxToggled), 1.0f))
                .anchorPoint(0.5f, 0.5f)
                .pos(CELL_WIDTH - 20.f, CELL_HEIGHT / 2)
                .scale(0.8f)
                .with([&](auto* btn) {
                    btn->m_offButton->setOpacity(static_cast<unsigned char>(255 * opacity));
                    btn->m_onButton->setOpacity(static_cast<unsigned char>(255 * opacity));
                })
                .id("input-checkbox"_spr)
                .store(inpCheckbox)
                .intoNewParent(CCMenu::create())
                .pos(0.f, 0.f)
                .id("input-menu"_spr)
                .parent(this)
                .enabled(possible);

            if (!possible) {
                Build<CCSprite>::createSpriteName("GJ_infoIcon_001.png")
                    .scale(0.4f)
                    .intoMenuItem([](auto) {
                        FLAlertLayer::create("Not available", "This feature requires the Discord Rich Presence mod to be installed.", "Ok")->show();
                    })
                    .pos(inpCheckbox->getPositionX() - 20.f, inpCheckbox->getPositionY() + 10.f)
                    .intoNewParent(CCMenu::create())
                    .pos(0.f, 0.f)
                    .parent(this);

                this->storeAndSave(false);
            }

            inpCheckbox->toggle(*(bool*)(settingStorage));
        } break;
        case Type::Float: {
            Build<Slider>::create(this, menu_selector(GlobedSettingCell::onSliderChanged), 0.3f)
                .anchorPoint(1.f, 0.5f)
                .pos(CELL_WIDTH - 45.f, CELL_HEIGHT / 2)
                .parent(this)
                .id("input-slider"_spr)
                .store(inpSlider);

            // my ass
            inpSlider->m_groove->setScaleY(0.75f);
            auto thumbs1 = (CCNode*)(inpSlider->getThumb()->getChildren()->objectAtIndex(0));
            auto thumbs2 = (CCNode*)(inpSlider->getThumb()->getChildren()->objectAtIndex(1));

            thumbs1->setScale(1.5f);
            thumbs2->setScale(1.5f);

            thumbs1->setPositionY(thumbs1->getPositionY() - 10.f);
            thumbs2->setPositionY(thumbs2->getPositionY() - 10.f);

            float value = *(float*)(settingStorage);
            float relativeValue = value / (limits.floatMax - limits.floatMin);
            inpSlider->setValue(relativeValue);
        } break;
        case Type::String: [[fallthrough]];
        case Type::PacketFragmentation: [[fallthrough]];
        case Type::AdvancedSettings: [[fallthrough]];
        case Type::LinkCode: [[fallthrough]];
        case Type::AudioDevice: {
            const char* text;
            if (settingType == Type::PacketFragmentation) {
                text = "Auto";
            } else if (settingType == Type::AdvancedSettings) {
                text = "View";
            } else if (settingType == Type::AudioDevice) {
                text = "Set";
            } else if (settingType == Type::LinkCode) {
                text = "View";
            } else {
                text = "String??";
            }

            Build<ButtonSprite>::create(text, "goldFont.fnt", "GJ_button_04.png", .7f)
                .intoMenuItem([this](auto* sender) {
                    this->onInteractiveButton(sender);
                })
                .anchorPoint(0.5f, 0.5f)
                .pos(CELL_WIDTH - 10.f, CELL_HEIGHT / 2)
                .id("input-interactive-btn"_spr)
                .store(inpAudioButton)
                .intoNewParent(CCMenu::create())
                .pos(0.f, 0.f)
                .id("input-menu"_spr)
                .parent(this);

            inpAudioButton->setPositionX(inpAudioButton->getPositionX() - inpAudioButton->getScaledContentSize().width / 2);
        } break;
        case Type::Int: {
            int currentValue = *(int*)(settingStorage);
            Build<InputNode>::create(CELL_WIDTH * 0.2f, "", "chatFont.fnt", std::string(util::misc::STRING_DIGITS), 10)
                .anchorPoint(1.f, 0.5f)
                .pos(CELL_WIDTH - 10.f, CELL_HEIGHT / 2)
                .id("input-field"_spr)
                .store(inpField)
                .intoNewParent(CCMenu::create())
                .pos(0.f, 0.f)
                .id("input-menu"_spr)
                .parent(this);

            inpField->getInput()->setDelegate(this);
            inpField->setString(std::to_string(currentValue));
        } break;
        case Type::Corner: {
            this->recreateCornerButton();
        } break;
        case Type::InvitesFrom: {
            this->recreateInvitesFromButton();
        } break;
    }

    if (auto* menu = this->getChildByID("input-menu"_spr)) {
        menu->setLayout(RowLayout::create()
                            ->setAxisAlignment(AxisAlignment::End)
                            ->setAxisReverse(true)
                            ->setAutoScale(false));
        menu->setContentWidth(CELL_WIDTH - 5.f);
        menu->setAnchorPoint({0.f, 0.5f});
        menu->setPositionX(-2.f);
        menu->setPositionY(CELL_HEIGHT / 2.f);

        if (settingType == Type::PacketFragmentation) {
            auto spr = Build<CCSprite>::createSpriteName("pencil.png"_spr)
                .collect();

            // button to manually edit packet frag
            Build<CircleButtonSprite>::create(spr)
                .scale(0.65f)
                .intoMenuItem([this, settingStorage] (auto) {
                    AskInputPopup::create("Packet limit", [this](auto input) {
                        auto limit = util::format::parse<int>(input).value_or(0);
                        if (limit < 1300 || limit > 65000) {
                            FLAlertLayer::create("Error", "<cr>Invalid</c> limit was set. For best results, press the <cy>Auto</c> button instead, and only use this option if you know what you're doing.", "Ok")->show();
                            return;
                        }

                        this->storeAndSave(limit);
                    }, 6, std::to_string(*(int*)settingStorage), util::misc::STRING_DIGITS, 10.f)->show();
                })
                .id("fraglimit-manual-btn"_spr)
                .parent(menu);
        }

        menu->updateLayout();
    }

    return true;
}

void GlobedSettingCell::onCheckboxToggled(cocos2d::CCObject*) {
    this->storeAndSave(!inpCheckbox->isOn());
}

void GlobedSettingCell::onSliderChanged(cocos2d::CCObject*) {
    float relativeValue = inpSlider->getThumb()->getValue();
    float value = limits.floatMin + (limits.floatMax - limits.floatMin) * relativeValue;
    this->storeAndSave(value);
}

void GlobedSettingCell::onInteractiveButton(cocos2d::CCObject*) {
    if (settingType == Type::AudioDevice) {
#ifdef GLOBED_VOICE_SUPPORT
# ifndef GLOBED_VOICE_CAN_TALK
        // if we can't talk, show an error popup
        FLAlertLayer::create("Error", "Sorry, but recording audio is currently <cr>not possible</c> on this platform.", "Ok")->show();
        return;
# endif // GLOBED_VOICE_CAN_TALK

        // check for permission
        bool perm = permission::getPermissionStatus(Permission::RecordAudio);

        if (!perm) {
            geode::createQuickPopup(
                "No permission",
                "Globed does not currently have permission to use your microphone. Do you want to grant the permission?",
                "Cancel", "Grant", [] (FLAlertLayer*, bool btn2) {

                if (btn2) {
                    permission::requestPermission(Permission::RecordAudio, [](bool granted) {
                        if (granted) {
                            AudioSetupPopup::create()->show();
                        } else {
                            log::warn("permission denied when requesting audio access");
                        }
                    });
                }
            });

            return;
        }
        AudioSetupPopup::create()->show();
#endif // GLOBED_VOICE_SUPPORT
    } else if (settingType == Type::PacketFragmentation) {
        if (NetworkManager::get().established()) {
            FragmentationCalibartionPopup::create()->show();
        } else {
            FLAlertLayer::create("Error", "This action can only be done when connected to a server.", "Ok")->show();
        }
    } else if (settingType == Type::AdvancedSettings) {
        AdvancedSettingsPopup::create()->show();
    } else if (settingType == Type::LinkCode) {
        if (NetworkManager::get().established()) {
            LinkCodePopup::create()->show();
        } else {
            FLAlertLayer::create("Error", "This action can only be done when connected to a server.", "Ok")->show();
        }
    } else {
        StringInputPopup::create([this](const std::string_view text) {
            this->onStringChanged(text);
        })->show();
    }
}

void GlobedSettingCell::onStringChanged(const std::string_view text) {
    this->storeAndSave(text);
}

void GlobedSettingCell::recreateCornerButton() {
    if (cornerButton) {
        cornerButton->getParent()->removeFromParent();
        cornerButton->removeFromParent();
        cornerButton = nullptr;
    }

    int currentValue = *(int*)(settingStorage);

    ButtonSprite* theButton;

    Build<ButtonSprite>::create("000", "bigFont.fnt", "GJ_button_04.png", 0.4f)
        .scale(0.75f)
        .store(theButton)
        .intoMenuItem([this, currentValue](auto) {
            int cv = currentValue + 1;
            if (cv > 3) {
                cv = 0;
            }

            this->storeAndSave(cv);
            this->recreateCornerButton();
        })
        .anchorPoint(0.5f, 0.5f)
        .pos(CELL_WIDTH - 25.f, CELL_HEIGHT / 2)
        .scaleMult(1.1f)
        .id("overlay-btn")
        .store(cornerButton)
        .intoNewParent(CCMenu::create())
        .pos(0.f, 0.f)
        .id("overlay-menu")
        .parent(this);

    // kill me
    float xOff = 9.f * ((currentValue % 2 == 1) ? 1.f : -1.f);
    float yOff = 8.f * ((currentValue < 2) ? 1.f : -1.f);
    theButton->m_label->setPosition(theButton->m_label->getPosition() + ccp(xOff + 1.f, yOff));
    theButton->m_label->setString("0");
}

void GlobedSettingCell::recreateInvitesFromButton() {
    using InvitesFrom = GlobedSettings::InvitesFrom;

    if (invitesFromButton) {
        invitesFromButton->getParent()->removeFromParent();
        invitesFromButton->removeFromParent();
        invitesFromButton = nullptr;
    }

    InvitesFrom currentValue = static_cast<InvitesFrom>(std::clamp(*(int*)(settingStorage), 0, 2));

    const char* text = "";
    switch (currentValue) {
        case InvitesFrom::Everyone: text = "Everyone"; break;
        case InvitesFrom::Friends: text = "Friends"; break;
        case InvitesFrom::Nobody: text = "Nobody"; break;
        default: globed::unreachable();
    }

    Build<ButtonSprite>::create(text, "bigFont.fnt", "GJ_button_04.png", 0.5f)
        .scale(0.75f)
        .intoMenuItem([this, currentValue](auto) {
            asp::NumberCycle curValue((int)currentValue, 0, (int)InvitesFrom::Nobody);
            curValue.increment();

            this->storeAndSave(curValue.get());
            this->recreateInvitesFromButton();
        })
        .anchorPoint(0.5f, 0.5f)
        .with([](auto* btn) {
            btn->setPosition(CELL_WIDTH - 8.f - btn->getScaledContentSize().width / 2.f, CELL_HEIGHT / 2);
        })
        .scaleMult(1.1f)
        .id("invite-from-btn")
        .store(invitesFromButton)
        .intoNewParent(CCMenu::create())
        .pos(0.f, 0.f)
        .id("invite-from-menu")
        .parent(this);
}

void GlobedSettingCell::storeAndSave(std::any&& value) {
    // banger
    switch (settingType) {
        case Type::DiscordRPC: [[fallthrough]];
        case Type::Bool:
            *(bool*)(settingStorage) = std::any_cast<bool>(value); break;
        case Type::Float:
            *(float*)(settingStorage) = std::any_cast<float>(value); break;
        case Type::String:
            *(std::string*)(settingStorage) = std::any_cast<std::string>(std::move(value)); break;
        case Type::AudioDevice: [[fallthrough]];
        case Type::Corner: [[fallthrough]];
        case Type::PacketFragmentation: [[fallthrough]];
        case Type::InvitesFrom: [[fallthrough]];
        case Type::LinkCode: [[fallthrough]];
        case Type::Int:
            *(int*)(settingStorage) = std::any_cast<int>(value); break;
        case Type::AdvancedSettings:
            break;
    }

    GlobedSettings::get().save();
}

void GlobedSettingCell::textChanged(CCTextInputNode* p0) {
    auto val = util::format::parse<int>(p0->getString());
    if (val) {
        int vval = val.value();
        if (limits.intMax != 0 && limits.intMin != 0) {
            vval = std::clamp(vval, limits.intMin, limits.intMax);
        }

        this->storeAndSave(val.value());
    }
}

void GlobedSettingCell::textInputOpened(CCTextInputNode* p0) {}
void GlobedSettingCell::textInputClosed(CCTextInputNode* p0) {}
void GlobedSettingCell::textInputShouldOffset(CCTextInputNode* p0, float p1) {}
void GlobedSettingCell::textInputReturn(CCTextInputNode* p0) {}
bool GlobedSettingCell::allowTextInput(CCTextInputNode* p0) { return true; }
void GlobedSettingCell::enterPressed(CCTextInputNode* p0) {}

GlobedSettingCell* GlobedSettingCell::create(void* settingStorage, Type settingType, const char* name, const char* desc, const Limits& limits) {
    auto ret = new GlobedSettingCell;
    if (ret->init(settingStorage, settingType, name, desc, limits)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}