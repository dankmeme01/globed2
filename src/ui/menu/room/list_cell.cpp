#include "list_cell.hpp"

#include <managers/settings.hpp>
#include <util/gd.hpp>
#include <util/ui.hpp>

using namespace geode::prelude;

bool CollapsableLevelCell::init(GJGameLevel* level, float width) {
    m_level = level;
    this->setContentSize({ width, HEIGHT });

    // initialize levelCell

    m_levelCell = new LevelCell(level->m_levelName.c_str(), width, HEIGHT);
    m_levelCell->autorelease();
    m_levelCell->loadFromLevel(level);
    m_levelCell->setContentSize({ width, HEIGHT });
    m_levelCell->setPosition({ 0.f, 0.f });
    m_levelCell->setVisible(false);

    auto cvoltonID = m_levelCell->m_mainLayer->getChildByIDRecursive("cvolton.betterinfo/level-id-label");
    if (cvoltonID != nullptr) {
        cvoltonID->setVisible(false);
    }

    // initialize collapsedCell

    m_collapsedCell = CCNode::create();

    // (stolen from PlayerListCell)
    auto leftMenu = Build<CCMenu>::create()
        .pos(10.f, COLLAPSED_HEIGHT / 2.f)
        .anchorPoint(0.f, 0.5f)
        .layout(RowLayout::create()
            ->setAutoScale(false)
            ->setAxisAlignment(AxisAlignment::Start)
            ->setGap(10.f)
        )
        .parent(m_collapsedCell)
        .id("left-menu")
        .collect();

    Build<GJDifficultySprite>::create(util::gd::calcLevelDifficulty(level), GJDifficultyName::Short)
        .scale(0.5f)
        .anchorPoint(0.f, 0.5f)
        .id("difficulty-sprite"_spr)
        .parent(leftMenu);

    Build<CCLabelBMFont>::create(level->m_levelName.c_str(), "goldFont.fnt")
        .limitLabelWidth(170.f, 0.6f, 0.1f)
        .intoMenuItem(this, menu_selector(CollapsableLevelCell::onOpenLevel))
        .anchorPoint(0.5f, 0.5f)
        .scaleMult(1.1f)
        .parent(leftMenu);

    leftMenu->updateLayout();

    m_collapsedCell->setContentSize({ width, COLLAPSED_HEIGHT });
    m_collapsedCell->setPosition({ 0.f, 0.f });
    m_collapsedCell->setVisible(false);

    this->addChild(m_levelCell);
    this->addChild(m_collapsedCell);

    return true;
}

void CollapsableLevelCell::setIsCollapsed(bool isCollapsed) {
    m_collapsedCell->setVisible(false);
    m_levelCell->setVisible(false);
    if (isCollapsed) {
        m_collapsedCell->setVisible(true);
        this->setContentSize(
            m_collapsedCell->getContentSize()
        );
    } else {
        m_levelCell->setVisible(true);
        this->setContentSize(
            m_levelCell->getContentSize()
        );
    }

    if (auto parent = this->getParent()) {
        parent->setContentSize(this->getContentSize());
    }

    m_isCollapsed = isCollapsed;

    GlobedSettings::get().globed.pinnedLevelCollapsed = isCollapsed;
}

void CollapsableLevelCell::onOpenLevel(CCObject* sender) {
    util::ui::switchToScene(LevelInfoLayer::create(m_level, false));
}

CollapsableLevelCell* CollapsableLevelCell::create(GJGameLevel* level, float width) {
    auto ret = new CollapsableLevelCell;
    if (ret->init(level, width)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool ListCellWrapper::init(const PlayerRoomPreviewAccountData& data, float cellWidth, bool forInviting, bool isIconLazyLoad) {
    playerCell = PlayerListCell::create(
        data,
        cellWidth,
        forInviting,
        isIconLazyLoad
    );

    this->setContentSize(playerCell->getContentSize());
    this->addChild(playerCell);

    return true;
}

bool ListCellWrapper::init(GJGameLevel* level, float width, CollapsedCallback&& callback) {
    roomCell = CollapsableLevelCell::create(level, width);
    this->addChild(roomCell);

    this->callback = std::move(callback);

    Build<CCMenu>::create()
        .pos(width - 20.f, CollapsableLevelCell::HEIGHT - CollapsableLevelCell::COLLAPSED_HEIGHT / 2.f)
        .anchorPoint(0.f, 0.5f)
        .parent(roomCell)
        .id("right-menu")
        .store(rightMenu);

    float scaleFactor = 0.4f;

    auto collapsedSprite = CCSprite::createWithSpriteFrameName("navArrowBtn_001.png");
    collapsedSprite->setRotation(90);
    collapsedSprite->setScale(scaleFactor);
    collapsedSprite->setFlipY(true);

    auto expandedSprite = CCSprite::createWithSpriteFrameName("navArrowBtn_001.png");
    expandedSprite->setRotation(-90);
    expandedSprite->setScale(scaleFactor);

    auto* toggler = Build<CCMenuItemToggler>::createToggle(collapsedSprite, expandedSprite, [this, callback](CCMenuItemToggler* toggler) {
        this->onCollapse(!toggler->isOn());
    })
        .parent(rightMenu)
        .collect();

    bool isCollapsed = GlobedSettings::get().globed.pinnedLevelCollapsed;
    toggler->toggle(isCollapsed);

    this->onCollapse(isCollapsed);

    return true;
}

void ListCellWrapper::onCollapse(bool isCollapsed) {
    roomCell->setIsCollapsed(isCollapsed);

    if (auto parent = this->getParent()) {
        parent->setContentSize(this->getContentSize());
    }

    // menu
    auto topPad = CollapsableLevelCell::COLLAPSED_HEIGHT / 2.f;
    rightMenu->setPositionY(isCollapsed ? topPad : CollapsableLevelCell::HEIGHT - topPad);

    this->callback(roomCell->m_isCollapsed);
}

ListCellWrapper* ListCellWrapper::create(const PlayerRoomPreviewAccountData& data, float cellWidth, bool forInviting, bool isIconLazyLoad) {
    auto ret = new ListCellWrapper;
    if (ret->init(
        data,
        cellWidth,
        forInviting,
        isIconLazyLoad
    )) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

ListCellWrapper* ListCellWrapper::create(GJGameLevel* level, float width, CollapsedCallback&& callback) {
    auto ret = new ListCellWrapper;
    if (ret->init(
        level,
        width,
        std::forward<CollapsedCallback>(callback)
    )) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

