#include <defs/geode.hpp>
#include <hooks/gjbasegamelayer.hpp>

#include <Geode/modify/UILayer.hpp>

struct GLOBED_DLL HookedUILayer : geode::Modify<HookedUILayer, UILayer> {
    static void onModify(auto& self) {
        (void) self.setHookPriority("UILayer::handleKeypress", 9999);
    }

    void handleKeypress(cocos2d::enumKeyCodes p0, bool p1) {
        auto gjbgl = GlobedGJBGL::get();
        if (gjbgl) gjbgl->m_fields->isManuallyResettingLevel = true;
        UILayer::handleKeypress(p0, p1);
        if (gjbgl) gjbgl->m_fields->isManuallyResettingLevel = false;
    }
};
