#include "overlay_cell.hpp"

#include <util/ui.hpp>

using namespace geode::prelude;
using namespace util::ui;

bool VoiceOverlayCell::init(const PlayerAccountData& data) {
    if (!CCNode::init()) return false;

    Build<CCNode>::create()
        .parent(this)
        .id("vc-cell-wrapper"_spr)
        .store(nodeWrapper);

    accountId = data.accountId;

    nodeWrapper->setLayout(RowLayout::create()->setGap(5.f)->setAutoScale(false)->setAxisReverse(true));

    // audio visualizer
    Build<GlobedAudioVisualizer>::create()
        .scale(0.5f)
        .parent(nodeWrapper)
        .store(visualizer);

    visualizer->setScaleX(0.4f);

    // username
    ccColor3B nameColor = ccc3(255, 255, 255);
    if (data.specialUserData.has_value()) {
        nameColor = data.specialUserData.value().nameColor;
    }

    auto nameLabel = Build<CCLabelBMFont>::create(data.name.c_str(), "bigFont.fnt")
        .scale(0.35f)
        .color(nameColor)
        .parent(nodeWrapper)
        .collect();

    // player icon
    auto gm = GameManager::get();
    auto color1 = gm->colorForIdx(data.icons.color1);
    auto color2 = gm->colorForIdx(data.icons.color2);

    auto playerIcon = Build<SimplePlayer>::create(data.icons.cube)
        .color(color1)
        .secondColor(color2)
        .scale(0.45f)
        .pos(0.f, 0.f)
        .parent(nodeWrapper)
        .collect();

    playerIcon->setContentSize(playerIcon->m_firstLayer->getScaledContentSize());
    auto firstNode = static_cast<CCNode*>(playerIcon->getChildren()->objectAtIndex(0));
    firstNode->setPosition(playerIcon->m_firstLayer->getScaledContentSize() / 2);

    if (data.specialUserData.has_value()) {
        auto badge = createBadgeIfSpecial(nameColor, playerIcon->getPosition() + CCPoint{-20.f, 0.f});
        if (badge != nullptr) nodeWrapper->addChild(badge);
    }

    if (data.icons.glowColor != -1) {
        playerIcon->setGlowOutline(gm->colorForIdx(data.icons.glowColor));
    } else {
        playerIcon->disableGlowOutline();
    }

    const float heightMult = 1.3f;
    const float widthMult = 1.1f;

    nodeWrapper->setContentWidth(playerIcon->getScaledContentSize().width + 5.f + nameLabel->getScaledContentSize().width + 5.f + visualizer->getScaledContentSize().width);
    nodeWrapper->setContentHeight(playerIcon->getScaledContentSize().height * heightMult);
    nodeWrapper->updateLayout();

    // background
    auto sizeScale = 4.f;
    auto cc9s = Build<CCScale9Sprite>::create("square02_001.png")
        .contentSize(nodeWrapper->getScaledContentSize() * sizeScale + CCPoint{37.f, 25.f})
        .scaleX(1.f / sizeScale)
        .scaleY(1.f / sizeScale)
        .opacity(80)
        .zOrder(-1)
        .anchorPoint(0.f, 0.f)
        .parent(this)
        .collect();

    this->setContentSize(cc9s->getScaledContentSize());

    nodeWrapper->setPosition({this->getScaledContentSize().width - nodeWrapper->getScaledContentSize().width - 5.f, 3.5f});

    return true;
}

void VoiceOverlayCell::updateVolume(float vol) {
    visualizer->setVolume(vol);
}

VoiceOverlayCell* VoiceOverlayCell::create(const PlayerAccountData& data) {
    auto ret = new VoiceOverlayCell;
    if (ret->init(data)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}