#include "changelog.hpp"

#include <util/format.hpp>
#include <managers/settings.hpp>

using namespace geode::prelude;

static const std::string LAST_CHANGELOG_KEY = "_last-changelog-seen";

namespace globed {

bool shouldShowChangelogPopup() {
    if (!GlobedSettings::get().globed.changelogPopups) return false;

    auto currentVersion = Mod::get()->getVersion();
    auto lastVerResult = VersionInfo::parse(Mod::get()->getSavedValue<std::string>(LAST_CHANGELOG_KEY));
    VersionInfo lastVer;
    if (lastVerResult) {
        lastVer = lastVerResult.unwrap();
    } else {
        return true;
    }

    if (lastVer < currentVersion) {
        return true;
    }

    return false;
}

namespace {
class BetterMDPopup : public MDPopup {
public:
    static BetterMDPopup* create(
        std::string const& title, std::string const& content, char const* btn1, char const* btn2,
        utils::MiniFunction<void(bool)> onClick
    ) {
        auto ret = new BetterMDPopup();
        if (ret->initAnchored(
                420.f, MDPopup::estimateHeight(content), title, content, btn1, btn2, onClick,
                "square01_001.png", { 0, 0, 94, 94 }
            )) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool setup(
        std::string const& title, std::string const& info, char const* btn1Text, char const* btn2Text,
        utils::MiniFunction<void(bool)> onClick
    ) {
        this->setTitle(title.c_str(), "goldFont.fnt", .9f, 33.f);

        m_onClick = onClick;

        auto contentSize = CCSize {
            m_size.width - 70.f,
            m_size.height - 120.f,
        };
        auto content = MDTextArea::create(info, contentSize);
        m_mainLayer->addChildAtPosition(content, Anchor::Center, ccp(0, 0));

        auto btnSpr = ButtonSprite::create(btn1Text);

        auto btn = CCMenuItemSpriteExtra::create(btnSpr, this, menu_selector(BetterMDPopup::onBtn));
        btn->setTag(0);

        auto menu = CCMenu::create();
        menu->setLayout(
            RowLayout::create()
                ->setAxisAlignment(AxisAlignment::Center)
                ->setGap(10.f)
        );
        menu->addChild(btn);

        if (btn2Text) {
            auto btn2Spr = ButtonSprite::create(btn2Text);

            auto btn2 = CCMenuItemSpriteExtra::create(btn2Spr, this, menu_selector(BetterMDPopup::onBtn));
            btn2->setTag(1);

            menu->addChild(btn2);
        }

        m_buttonMenu->addChildAtPosition(menu, Anchor::Bottom, ccp(0, 30));
        menu->updateLayout();

        return true;
    }

    void onClose(CCObject* b) {
        MDPopup::onClose(b);
    }

private:
    void onBtn(CCObject* btn) {
        if (m_onClick) {
            m_onClick(btn->getTag());
        }
    }
};
}

FLAlertLayer* showChangelogPopup() {
    auto currentVersion = Mod::get()->getVersion();
    // parse changelog.md lol
    auto clog_ = Mod::get()->getMetadata().getChangelog();
    if (!clog_) {
        log::warn("Changelog is null");
        return nullptr;
    }

    auto clog = std::move(clog_.value());
    auto lines = util::format::splitlines(clog);

    std::string currentChangelog;

    bool insideCorrectVersion = false;

    for (auto line : lines) {
        if (line.starts_with("## ")) {
            auto linever = util::format::trim(line.substr(sizeof("## ") - 1));
            auto parsedr = VersionInfo::parse(linever);
            if (!parsedr) {
                continue;
            }


            if (parsedr.unwrap() == currentVersion) {
                insideCorrectVersion = true;
            } else if (insideCorrectVersion) {
                break;
            }
        } else if (insideCorrectVersion) {
            currentChangelog += line;
            currentChangelog += "\n";
        }
    }

    log::debug("Changelog text: {}, size: {}", currentChangelog, currentChangelog.size());

    if (!currentChangelog.empty()) {
        Mod::get()->setSavedValue(LAST_CHANGELOG_KEY, Mod::get()->getVersion().toVString());

        static auto closeChangelogPopup = [] {
            if (auto popup = CCScene::get()->getChildByID("changelog-popup"_spr)) {
                static_cast<BetterMDPopup*>(popup)->onClose(popup);
            }
        };

        static auto onDisable = [] {
            geode::createQuickPopup(
                "Note",
                "Are you sure you want to disable <cy>changelog popups</c>? You can <cg>enable</c> them again in <cy>settings</c>.",
                "Cancel", "Yes",
                [](auto, bool yes) {
                    if (yes) {
                        GlobedSettings::get().globed.changelogPopups = false;
                        closeChangelogPopup();
                    }
                });
        };

        static auto onOk = [] {
            closeChangelogPopup();
        };

        auto popup = BetterMDPopup::create(
            "Globed changelog",
            fmt::format("\n## {}\n\n{}", currentVersion.toVString(), currentChangelog),
            "Disable",
            "Ok", [](bool ok) {
                ok ? onOk() : onDisable();
            }
        );
        popup->setID("changelog-popup"_spr);

        return popup;
    }

    return nullptr;
}

}