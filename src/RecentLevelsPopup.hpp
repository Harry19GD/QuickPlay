#pragma once

#include <Geode/ui/Popup.hpp>
#include "QuickPlay.hpp"

using namespace geode::prelude;

// Recent-levels browser. Rows are built from scratch (difficulty face,
// name, creator, stats, play button) with every font scale and position
// sized as fractions of the row box, so nothing clips or overlaps at any
// window size. Tap the play button to drop straight in.
class RecentLevelsPopup : public geode::Popup {
protected:
    ScrollLayer* m_scroll = nullptr;
    std::vector<quickplay::Entry> m_entries;
    std::vector<GJGameLevel*> m_levels;

    bool build();
    void rebuild();
    void animateWindowIn();
    CCNode* buildRow(GJGameLevel* level, int idx, float rowW, float rowH);
    void onPlayLevel(CCObject* sender);
    void onRemoveLevel(CCObject* sender);
    void onCloseBtn(CCObject* sender);

public:
    static RecentLevelsPopup* create();
    void show();
};