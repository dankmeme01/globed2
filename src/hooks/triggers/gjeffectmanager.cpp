#include "gjeffectmanager.hpp"

#include <Geode/modify/GJEffectManager.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/CountTriggerGameObject.hpp>

#include <defs/platform.hpp>
#include <defs/minimal_geode.hpp>
#include <hooks/gjbasegamelayer.hpp>
#include <globed/constants.hpp>
#include <util/lowlevel.hpp>

void GJEffectManagerHook::updateCountForItem(int id, int value) {
    if (globed::isWritableCustomItem(id)) {
        this->updateCountForItemCustom(id, value);
    } else {
        // for testing!
        // if (m_countTriggerActions.contains(id)) {
        //     auto& actions = m_countTriggerActions.at(id);

        //     for (const auto& action : actions) {
        //         log::debug("Action: 0 = {}, 4 = {}, count = {}, group = {}, 10 = {}, 14 = {}, 18 = {}, item id = {}, multiac = {}, set = {}", action.m_unk0, action.m_unk4, action.m_targetCount, action.m_targetGroup, action.m_unk10, action.m_unk14, action.m_unk18, action.m_itemID, action.m_multiActivate, action.m_unkVecInt);
        //         // m_triggerEffectDelegate->toggleGroupTriggered(id)
        //     }
        // }

        GJEffectManager::updateCountForItem(id, value);
    }
}

#ifndef GEODE_IS_WINDOWS
int GJEffectManagerHook::countForItem(int item) {
    if (globed::isWritableCustomItem(item)) {
        return this->countForItemCustom(item);
    } else {
        return GJEffectManager::countForItem(item);
    }
}
#else

// We can't hook this function because gd's LTO assumes it will not modify r11,
// which our hook will inadvertently do, breaking one or multiple callsites.

// The patch preserves r11 and calls a reimplementation of the function.

static geode::Patch* countForItemPatch = nullptr;

int countForItemDetour(GJEffectManagerHook* self, int itemId) {
    log::debug("Request for item {}", itemId);
    if (globed::isCustomItem(itemId)) {
        return self->countForItemCustom(itemId);
    } else {
        itemId = std::clamp(itemId, 0, 9999);
        return self->m_itemIDs[itemId];
    }
}

$execute {
    std::vector<uint8_t> bytes = {
        0x41, 0x53, // push r11
        0x48, 0x83, 0xec, 0x10, // sub rsp, 0x20
        0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // movabs rax, 0x0
        0xff, 0xd0, // call rax
        0x48, 0x83, 0xc4, 0x10, // add rsp, 0x20
        0x41, 0x5b, // pop r11
        0xc3, // ret
    };

    uint64_t daddr = reinterpret_cast<uint64_t>(&countForItemDetour);
    std::memcpy(bytes.data() + 8, &daddr, sizeof(daddr));

    // for some reason getNonVirtual does not work here lol
#if GEODE_COMP_GD_VERSION == 22060
countForItemPatch = util::lowlevel::patch(0x2506d0, bytes);
#else
# error Update this for new gd, is address of GJEffectManager::countForItem
#endif
}
#endif

void GJEffectManagerHook::addCountToItemCustom(int id, int diff) {
    int newValue = m_fields->customItems[id] + diff;
    this->updateCountForItemCustom(id, newValue);
}

void GJEffectManagerHook::reset() {
    GJEffectManager::reset();

    m_fields->customItems.clear();
}

bool GJEffectManagerHook::updateCountForItemCustom(int id, int value) {
    auto& fields = *m_fields.self();

    int prev = fields.customItems[id];
    fields.customItems[id] = value;

    // update count triggers haha
    // please dont even begin to try to understand any of this

    if (m_countTriggerActions.contains(id)) {
        auto& actions = m_countTriggerActions.at(id);

        auto comparator = value < prev
            ? [](const CountTriggerAction& a, const CountTriggerAction& b){ return b.m_targetCount < a.m_targetCount; }
            : [](const CountTriggerAction& a, const CountTriggerAction& b){ return a.m_targetCount < b.m_targetCount; };

        // yeah whatever you say robby
        std::sort(actions.begin(), actions.end(), comparator);


        for (size_t i = 0; i < actions.size();) {
            auto& action = actions[i];

            if (action.m_previousCount == value) {
                i++;
                continue;
            }

            bool multiActivate = action.m_multiActivate;
            int prevCount = action.m_previousCount;
            action.m_previousCount = value;

            auto stuff = [&]{

                auto unkVecInt = action.m_unkVecInt;
                bool unk10 = action.m_unk10;
                int unk14 = action.m_unk14;
                int unk18 = action.m_unk18;
                int groupId = action.m_targetGroup;

                if (!action.m_multiActivate) {
                    actions.erase(actions.begin() + i);
                }

                if (!m_triggerEffectDelegate) {
                    // i cant be bothered im not gonna lie
                    // this->toggleGroup(groupId, unk10);
                } else {
                    m_triggerEffectDelegate->toggleGroupTriggered(groupId, unk10, unkVecInt, unk14, unk18);
                }
            };

            if (action.m_targetCount <= prevCount) {
                if ((action.m_targetCount < prevCount) && (value <= action.m_targetCount)) {
                    stuff();
                } else {
                    i++;
                    continue;
                }
            } else if (value < action.m_targetCount) {
                i++;
                continue;
            } else {
                stuff();
            }

            // log::debug("Action: 0 = {}, 4 = {}, 8 = {}, c = {}, 10 = {}, 14 = {}, 18 = {}, 1c = {}, set = {}", action.m_unk0, action.m_unk4, action.m_unk8, action.m_unkc, action.m_unk10, action.m_unk14, action.m_unk18, action.m_unk1c, action.m_unkVecInt);
            // m_triggerEffectDelegate->toggleGroupTriggered(id)

            if (multiActivate) {
                i++;
            }
        }
    }

    return prev != value;
}

int GJEffectManagerHook::countForItemCustom(int id) {
    return m_fields->customItems[id];
}

void GJEffectManagerHook::applyFromCounterChange(const GlobedCounterChange& change) {
    using enum GlobedCounterChange::Type;

    int prev = this->countForItemCustom(change.itemId);

    switch (change.type) {
        case Add: {
            this->updateCountForItemCustom(change.itemId, prev + change._val.intVal);
        } break;
        case Set: {
            this->updateCountForItemCustom(change.itemId, change._val.intVal);
        } break;
        case Multiply: {
            this->updateCountForItemCustom(change.itemId, prev * change._val.floatVal);
        } break;
        case Divide: {
            this->updateCountForItemCustom(change.itemId, prev / change._val.floatVal);
        } break;
    }

    GlobedGJBGL::get()->updateCounters(change.itemId, this->countForItemCustom(change.itemId));
}

void GJEffectManagerHook::applyItem(int id, int value) {
    if (this->updateCountForItemCustom(id, value)) {
        GlobedGJBGL::get()->updateCounters(id, value);
    }
}

// gjbgl collectedObject and addCountToItem inlined on windows.

// TODO: idk if this hook is needed? maybe 2.1 levels?
struct GLOBED_DLL EffectGameObjectHook : geode::Modify<EffectGameObjectHook, EffectGameObject> {
    void triggerObject(GJBaseGameLayer* layer, int idk, gd::vector<int> const* idunno) {
        if (this->m_collectibleIsPickupItem && globed::isWritableCustomItem(m_itemID)) {
            auto gjbgl = GlobedGJBGL::get();

            GlobedCounterChange cc;
            cc.itemId = globed::itemIdToCustom(m_itemID);

            using enum GlobedCounterChange::Type;

            if (m_subtractCount) {
                cc.type = Add;
                cc._val.intVal = -1;
            } else {
                cc.type = Add;
                cc._val.intVal = 1;
            }

            gjbgl->queueCounterChange(cc);
        } else {
            EffectGameObject::triggerObject(layer, idk, idunno);
        }
    }
};

// gjbgl collectedObject and addCountToItem inlined on windows.
struct GLOBED_DLL CountObjectHook : geode::Modify<CountObjectHook, CountTriggerGameObject> {
    void triggerObject(GJBaseGameLayer* layer, int idk, gd::vector<int> const* idunno) {
        if (m_objectID == 0x719 && globed::isWritableCustomItem(m_itemID)) {
            // gjbgl::addPickupTrigger reimpl
            auto gjbgl = GlobedGJBGL::get();

            GlobedCounterChange cc;
            cc.itemId = globed::itemIdToCustom(m_itemID);

            using enum GlobedCounterChange::Type;

            if (m_pickupTriggerMode == 1) {
                cc.type = Multiply;
                cc._val.floatVal = m_pickupTriggerMultiplier;
            } else if (m_pickupTriggerMode == 2) {
                cc.type = Divide;
                cc._val.floatVal = m_pickupTriggerMultiplier;
            } else if (!m_unkPickupBool2) {
                cc.type = Add;
                cc._val.intVal = m_pickupCount;
            } else {
                cc.type = Set;
                cc._val.intVal = m_pickupCount;
            }

            gjbgl->queueCounterChange(cc);
        } else {
            CountTriggerGameObject::triggerObject(layer, idk, idunno);
        }
    }
};