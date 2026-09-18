// QuickPlay — continue the last played level from the main menu, and keep
// a compact pinned-level list behind the chevron.
//
// MenuLayer: compact continue pill under the main-menu row.
// PlayLayer: remembers the last level actually played.
// LevelInfoLayer: + button adds/removes the open level to/from recents.

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Notification.hpp>

#include <unordered_set>

#include "QuickPlay.hpp"
#include "QuickPlayBar.hpp"
#include "QuickPlayPopup.hpp"

using namespace geode::prelude;

namespace {

bool g_enabled = true;
bool g_keepSaved = true;

// Levels already re-saved this boot: saveLevel shuffles saved-list state,
// so it must fire at most once per level per session (double-fires preceded
// a pauseGame null crash — see 2026-09-17 log).
std::unordered_set<long long> g_keptThisBoot;

inline long long qpKeepKey(int type, int id) {
    return (static_cast<long long>(type) << 32) | (unsigned int)id;
}

void refreshSettings() {
    auto mod = Mod::get();
    if (!mod) return;
    try {
        g_enabled = mod->getSettingValue<bool>("enabled");
    } catch (...) {}
    try {
        g_keepSaved = mod->getSettingValue<bool>("keep-saved");
    } catch (...) {}
}

} // namespace

class $modify(QP_Play, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        if (level) {
            quickplay::saveLastPlayed(level);
            try { quickplay::pushRecent(quickplay::entryFromLevel(level)); } catch (...) {}
            if (g_keepSaved && level->m_levelType == GJLevelType::Saved) {
                try {
                    quickplay::Entry e = quickplay::entryFromLevel(level);
                    // At most once per level per boot (see g_keptThisBoot note).
                    if (g_keptThisBoot.insert(qpKeepKey(e.type, e.id)).second) {
                        if (auto glm = GameLevelManager::sharedState()) {
                            auto saved = glm->getSavedLevel(e.id);
                            if (!saved) {
                                glm->saveLevel(level);
                                saved = glm->getSavedLevel(e.id);
                            }
                            if (saved) {
                                // Saved-list display order is descending
                                // m_levelIndex (highest = top). Playing a
                                // far-back level must lift it above the
                                // current max so the game's save cleanup
                                // stops dropping it ("Load Failed!").
                                int hi = glm->getHighestLevelOrder();
                                if (saved->m_levelIndex < hi) {
                                    saved->m_levelIndex = hi + 1;
                                }
                            }
                        }
                    }
                } catch (...) {}
            }
        }
        return true;
    }
};

class $modify(QP_Menu, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        refreshSettings();
        if (g_enabled) addContinueBar();
        return true;
    }

    void addContinueBar() {
        auto bar = QuickPlayBar::create();
        if (!bar) return;
        auto win = CCDirector::sharedDirector()->getWinSize();
        // Compact pill under the main-button row. Bar node is 300x64: at
        // center y=92.5 the pill top (114) clears the big Play button above
        // (sprite bottom ~115) and the tab bottom (~68) stays clear of the
        // bottom-menu icons (~71).
        float x = win.width / 2.f;
        float y = 92.5f;
        bar->setPosition({x, y});
        this->addChild(bar, 10);
    }
};

class $modify(QP_LevelInfo, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;
        if (g_enabled && level) addPinButton();
        return true;
    }

    void addPinButton() {
        auto img = quickplay::iconOrText("GJ_plusBtn_001.png", "+", 0.9f);
        if (!img) return;
        if (auto spr = dynamic_cast<CCSprite*>(img)) {
            float h = spr->getContentSize().height;
            if (h > 1.f) spr->setScale(36.f / h);
        }
        auto btn = CCMenuItemSpriteExtra::create(
            img, this, menu_selector(QP_LevelInfo::onQPPin));
        btn->setID("quickplay-pin-button");

        if (auto side = this->getChildByID("left-side-menu")) {
            if (auto menu = dynamic_cast<CCMenu*>(side)) {
                menu->addChild(btn);
                menu->updateLayout();
                return;
            }
        }
        if (!m_playBtnMenu) {
            return;
        }
        btn->setScale(0.7f);
        btn->setPosition({-140.f, 0.f});
        m_playBtnMenu->addChild(btn);
    }

    void onQPPin(CCObject*) {
        if (!m_level) return;
        // The pinned-list popup (QuickPlayPopup) is not reachable from any
        // UI, so the level-screen button maintains the RECENT list instead
        // — the one the chevron browser actually shows.
        quickplay::Entry e = quickplay::entryFromLevel(m_level);
        if (e.id == 0 && e.name.empty()) return;

        auto list = quickplay::loadRecents();
        for (auto it = list.begin(); it != list.end(); it++) {
            if (quickplay::sameEntry(*it, e)) {
                list.erase(it);
                quickplay::saveRecents(list);
                Notification::create("Removed from recents", NotificationIcon::Info)->show();
                return;
            }
        }
        // Move-to-front insert, auto-capped by the max-recents setting.
        quickplay::pushRecent(e);
        Notification::create("Added to recents", NotificationIcon::Success)->show();
    }
};

$execute {
    refreshSettings();
}
