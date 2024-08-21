#pragma once

#include <defs/geode.hpp>
#include <defs/assert.hpp>
#include <defs/util.hpp>

#include "border.hpp"
#include "content_layer.hpp"
#include <util/ui.hpp>
#include <util/rng.hpp>
#include <util/lowlevel.hpp>

template <typename CellType>
    requires (std::is_base_of_v<cocos2d::CCNode, CellType>)
class GlobedListCell : public cocos2d::CCLayer {
public:
    static GlobedListCell* create(CellType* inner, float width) {
        auto ret = new GlobedListCell();
        if (ret->init(inner, width)) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }

    template <typename T>
    static GlobedListCell* create(CellType* inner, float width, const T& color) {
        auto* cell = create(inner, width);
        if (cell) {
            cell->setColor(color);
        }

        return cell;
    }

private:
    template <typename T>
        requires (std::is_base_of_v<cocos2d::CCNode, T>)
    friend class GlobedListLayer;
    CellType* inner;

    cocos2d::CCLayerColor* background;

    bool init(CellType* inner, float width) {
        if (!CCLayer::init()) return false;

        this->inner = inner;
        this->addChild(inner);

        float height = inner->getScaledContentSize().height;

        this->setContentSize(cocos2d::CCSize{width, height});

        Build<cocos2d::CCLayerColor>::create(cocos2d::ccColor4B{255, 255, 255, 255}, width, height)
            .zOrder(-1)
            .parent(this)
            .store(background);

        return true;
    }

    void draw() override {
        // i stole this from geode
        auto size = this->getContentSize();
        cocos2d::ccDrawColor4B(0, 0, 0, 0x4f);
        glLineWidth(2.0f);
        cocos2d::ccDrawLine({ 1.0f, 0.0f }, { size.width - 1.0f, 0.0f });
        cocos2d::ccDrawLine({ 1.0f, size.height }, { size.width - 1.0f, size.height });
    }

    template <typename T>
    void setColor(const T& color) {
        background->setColor(globed::into<cocos2d::ccColor3B>(color));
    }

    template <>
    void setColor(const cocos2d::ccColor4B& color) {
        background->setColor(globed::into<cocos2d::ccColor3B>(color));
        background->setOpacity(color.a);
    }
};


template <typename CellType>
    requires (std::is_base_of_v<cocos2d::CCNode, CellType>)
class GlobedListLayer : public cocos2d::CCLayer {
public:
    using WrapperCell = GlobedListCell<CellType>;

    class ScrollPos {
    public:
        ScrollPos(float val) : val(val), atBottom(false) {}
        ScrollPos() : val(0.f), atBottom(true) {}

        operator float() { return val; }

        float val = 0.0f;
        bool atBottom = false;
    };

    size_t cellCount() {
        return scrollLayer->m_contentLayer->getChildrenCount();
    }

    CellType* getCell(int index) {
        GLOBED_REQUIRE(index >= 0 && index < this->cellCount(), "invalid index passed to getCell");

        return static_cast<WrapperCell*>(scrollLayer->m_contentLayer->getChildren()->objectAtIndex(index))->inner;
    }

    // Returns CCArray<GlobedListCell<CellType>>*
    cocos2d::CCArray* getCells() {
        return scrollLayer->m_contentLayer->getChildren();
    }

    void addCell(CellType* cell, bool updateLayout = true) {
        this->addWrapperCell(cell, this->cellCount(), updateLayout);
    }

    void insertCell(CellType* cell, int index, bool updateLayout = true) {
        GLOBED_REQUIRE(index >= 0 && index <= this->cellCount(), "invalid index passed to insertCell");

        this->addWrapperCell(cell, index, updateLayout);
    }

    void removeCell(WrapperCell* cell, bool updateLayout = true) {
        if (cell && cell->getParent() == scrollLayer->m_contentLayer) {
            scrollLayer->m_contentLayer->removeChild(cell);
        }

        if (updateLayout) {
            this->updateCells();
        }
    }

    void removeCell(CellType* cell, bool updateLayout = true) {
        this->removeCell(static_cast<WrapperCell*>(cell->getParent()), updateLayout);
    }

    void removeCell(int index, bool updateLayout = true) {
        GLOBED_REQUIRE(index >= 0 && index < this->cellCount(), "invalid index passed to removeCell");

        this->removeCell(this->getCell(index), updateLayout);
    }

    void removeAllCells(bool updateLayout = true) {
        scrollLayer->m_contentLayer->removeAllChildren();

        if (updateLayout) {
            this->updateCellOrder();
        }
    }

    void swapCells(cocos2d::CCArray* cells, bool preserveScrollPos = true) {
        auto scpos = this->getScrollPos();

        this->removeAllCells();

        for (auto* cell : CCArrayExt<CellType*>(cells)) {
            this->addCell(cell, false);
        }

        this->updateCellOrder();

        if (preserveScrollPos) {
            this->scrollToPos(scpos);
        }
    }

private:
    template <typename F> requires (std::invocable<F, std::vector<Ref<WrapperCell>>&>)
    void sortInternal(F&& functor) {
        std::vector<Ref<WrapperCell>> sorted;

        for (auto* cell : CCArrayExt<WrapperCell*>(this->getCells())) {
            sorted.push_back(Ref(cell));
        }

        functor(sorted); // sort

        scrollLayer->m_contentLayer->removeAllChildren();

        size_t idx = 0;
        for (auto elem : sorted) {
            this->addWrapperCell(elem, idx++, false);
        }

        this->updateCells();
    }

public:
    void sort(std::function<bool (CellType*, CellType*)>&& pred) {
        sortInternal([&](auto& sorted) {
            std::sort(sorted.begin(), sorted.end(), [&](auto a, auto b) {
                return pred(a->inner, b->inner);
            });
        });
    }

    void shuffle() {
        sortInternal([&](auto& sorted) {
            std::shuffle(sorted.begin(), sorted.end(), util::rng::Random::get().getEngine());
        });
    }

    template <typename F> requires (std::invocable<F, std::vector<Ref<WrapperCell>>&>)
    void customSort(F&& functor) {
        sortInternal(std::forward<F>(functor));
    }

    void scrollToTop() {
        util::ui::scrollToTop(scrollLayer);
    }

    void scrollToBottom() {
        util::ui::scrollToBottom(scrollLayer);
    }

    ScrollPos getScrollPos() {
        auto* cl = scrollLayer->m_contentLayer;
        if (cl->getPositionY() > 0.f) return ScrollPos();
        return ScrollPos(cl->getScaledContentSize().height + cl->getPositionY());
    }

    void scrollToPos(ScrollPos pos) {
        if (pos.atBottom) return;

        auto* cl = scrollLayer->m_contentLayer;
        float actualPos = pos.val - cl->getScaledContentSize().height;

        cl->setPositionY(std::min(actualPos, 0.f));
    }

    void scrollToCell() {
        // unimpl
    }

    template <typename T>
    void setCellColor(const T& in) {
        oddCellColor = evenCellColor = globed::into<cocos2d::ccColor4B>(in);
    }

    template <typename T, typename Y>
    void setCellColors(const T& odd, const Y& even) {
        oddCellColor = globed::into<cocos2d::ccColor4B>(odd);
        evenCellColor = globed::into<cocos2d::ccColor4B>(even);
    }

    void setCellHeight(float h) {
        cellHeight = h;
    }

    void forceUpdate() {
        this->updateCellOrder();
    }

    // disable being able to like scroll up above the stuff you know
    void disableOverScrollUp() {
        isDisableOverScrollUp = true;
    }

    // helper templated functions

    template <typename... Args>
    CellType* addCell(Args&&... args) {
        CellType* cell = CellType::create(std::forward<Args>(args)...);

        this->addCell(cell, true);

        return cell;
    }

    // like `addCell` but does not update layout
    template <typename... Args>
    CellType* addCellFast(Args&&... args) {
        CellType* cell = CellType::create(std::forward<Args>(args)...);

        this->addCell(cell, false);

        return cell;
    }

    template <typename... Args>
    CellType* insertCell(Args&&... args, int index) {
        CellType* cell = CellType::create(std::forward<Args>(args)...);

        this->insertCell(cell, index, true);

        return cell;
    }

    // like `insertCell` but does not update layout
    template <typename... Args>
    CellType* insertCellFast(Args&&... args, int index) {
        CellType* cell = CellType::create(std::forward<Args>(args)...);

        this->insertCell(cell, index, false);

        return cell;
    }

    template <typename T>
    static GlobedListLayer* create(float width, float height, const T& background, float cellHeight = 0.0f, GlobedListBorderType borderType = GlobedListBorderType::None) {
        auto ret = new GlobedListLayer();
        if (ret->init(width, height, globed::into(background), cellHeight, borderType)) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }

    static GlobedListLayer* createForComments(float width, float height, float cellHeight = 0.0f) {
        auto ret = new GlobedListLayer();
        if (ret->init(width, height, globed::into(globed::color::Brown), cellHeight, GlobedListBorderType::GJCommentListLayer)) {
            ret->setCellColors(globed::color::DarkBrown, globed::color::Brown);
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }

protected:
    friend class GlobedContentLayer;

    geode::ScrollLayer* scrollLayer;
    cocos2d::CCSprite *borderLeft, *borderRight, *borderTop, *borderBottom;
    float height, width, cellHeight = 0.f;
    cocos2d::ccColor4B oddCellColor, evenCellColor;
    Ref<GlobedListBorder> borderNode;
    bool isDisableOverScrollUp = false;

    bool init(float width, float height, cocos2d::ccColor4B background, float cellHeight, GlobedListBorderType borderType) {
        if (!CCLayer::init()) return false;

        this->height = height;
        this->width = width;
        this->cellHeight = cellHeight;

        Build<geode::ScrollLayer>::create(cocos2d::CCSize{width, height})
            .parent(this)
            .store(scrollLayer);

        scrollLayer->m_contentLayer->setLayout(
            cocos2d::ColumnLayout::create()
                ->setGap(0.f)
                ->setAxisReverse(true)
                ->setAxisAlignment(cocos2d::AxisAlignment::End)
                ->setAutoGrowAxis(height)
        );

        // this is fucking criminal i hope no one notices this
        util::lowlevel::swapVtable<GlobedContentLayer>(scrollLayer->m_contentLayer);

        this->setContentSize({width, height});
        this->ignoreAnchorPointForPosition(false);

        // TODO: bg?
        borderNode = GlobedListBorder::create(borderType, width, height, background);

        if (borderNode) {
            borderNode->setZOrder(1);
            borderNode->setAnchorPoint({0.5f, 0.5f});
            borderNode->setPosition(width / 2.f, height / 2.f);
            this->addChild(borderNode);
        }

        return true;
    }

    WrapperCell* addWrapperCell(CellType* cell, int index, bool updateLayout = false) {
        if (cellHeight != 0.0f) {
            cell->setContentHeight(cellHeight);
        }

        auto wcell = WrapperCell::create(cell, width);
        this->addWrapperCell(wcell, index, updateLayout);

        return wcell;
    }

    void addWrapperCell(WrapperCell* cell, int index, bool updateLayout = false) {
        cell->setColor(this->getCellColor(index));

        // move all cells one up
        for (int i = index; i < this->cellCount(); i++) {
            static_cast<WrapperCell*>(scrollLayer->m_contentLayer->getChildren()->objectAtIndex(i))->setZOrder(i + 1);
        }

        scrollLayer->m_contentLayer->addChild(cell, index);

        if (updateLayout) {
            this->updateCells();
        }
    }

    void updateCellOrder() {
        size_t i = 0;
        for (WrapperCell* child : CCArrayExt<WrapperCell*>(scrollLayer->m_contentLayer->getChildren())) {
            child->setColor(this->getCellColor(i));
            child->setZOrder(i++);
        }

        for (WrapperCell* child : CCArrayExt<WrapperCell*>(scrollLayer->m_contentLayer->getChildren())) {
            child->setContentHeight(child->inner->getContentHeight());
        }

        this->updateCells();
    }

    void updateCells() {
        scrollLayer->m_contentLayer->updateLayout();
    }

    cocos2d::ccColor4B getCellColor(int idx) {
        return idx % 2 == 0 ? oddCellColor : evenCellColor;
    }

public:
    // iterators
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using held_type = WrapperCell*;
        using value_type = CellType*;
        using reference = CellType*&;
        using pointer = CellType**;

        iterator(cocos2d::CCArray* children, size_t idx) : array(children), idx(idx) {}

        reference operator*() const { return get(); }
        pointer operator->() { return &get(); }
        iterator& operator++() {
            idx++;
            return *this;
        }

        bool operator==(const iterator& other) const { return idx == other.idx && array == other.array; }
        bool operator!=(const iterator& other) const = default;

    private:
        cocos2d::CCArray* array;
        size_t idx;

        reference get() const {
            return static_cast<held_type>(array->objectAtIndex(idx))->inner;
        }
    };

    iterator begin() {
        auto arr = scrollLayer->m_contentLayer->getChildren();
        return iterator(arr, 0);
    }

    iterator end() {
        auto arr = scrollLayer->m_contentLayer->getChildren();
        return iterator(arr, arr ? arr->count() : 0);
    }
};
