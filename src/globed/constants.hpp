#pragma once

#include <defs/assert.hpp>

namespace globed {
    constexpr inline int CUSTOM_ITEM_ID_START = 90'000;
    constexpr inline int CUSTOM_ITEM_ID_RO_START = 100'000;
    constexpr inline int CUSTOM_ITEM_ID_END = 110'000;

    inline bool isCustomItem(int itemId) {
        return itemId >= CUSTOM_ITEM_ID_START && itemId < CUSTOM_ITEM_ID_END;
    }

    inline bool isWritableCustomItem(int itemId) {
        return itemId >= CUSTOM_ITEM_ID_START && itemId < CUSTOM_ITEM_ID_RO_START;
    }

    inline bool isReadonlyCustomItem(int itemId) {
        return itemId >= CUSTOM_ITEM_ID_RO_START && itemId < CUSTOM_ITEM_ID_END;
    }

    // big value -> small value
    inline uint16_t itemIdToCustom(int itemId) {
#ifdef GLOBED_DEBUG
        GLOBED_REQUIRE(isCustomItem(itemId), "not custom item ID passed to itemIdToCustom");
#endif

        return itemId - CUSTOM_ITEM_ID_START;
    }

    inline int customItemToItemId(uint16_t customItemId) {
#ifdef GLOBED_DEBUG
        GLOBED_REQUIRE(customItemId >= 0 && customItemId < (CUSTOM_ITEM_ID_END - CUSTOM_ITEM_ID_START), "invalid custom item ID passed to customItemToItemId");
#endif

        return CUSTOM_ITEM_ID_START + customItemId;
    }
}