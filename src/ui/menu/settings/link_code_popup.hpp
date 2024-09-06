#pragma once
#include <defs/geode.hpp>

class LinkCodePopup : public geode::Popup<> {
public:
    static constexpr float POPUP_WIDTH = 240.f;
    static constexpr float POPUP_HEIGHT = 140.f;

    static LinkCodePopup* create();

private:
    Ref<cocos2d::CCLabelBMFont> label;

    bool setup() override;
    void onLinkCodeReceived(uint32_t linkCode);
};
