#pragma once

#include <Geode/ui/Popup.hpp>
#include "QuickPlay.hpp"

using namespace geode::prelude;

// Compact Quick Play list. Last played sits at the top; pinned levels follow.
class QuickPlayPopup : public geode::Popup {
protected:
    ScrollLayer* m_scroll = nullptr;
    CCLabelBMFont* m_countLabel = nullptr;
    std::vector<quickplay::Entry> m_entries;
    std::vector<GJGameLevel*> m_levels;
    std::vector<char> m_pinned;
    int m_lastIndex = -1;
    int m_expanded = -1;

    bool build();
    void rebuildContent();
    CCNode* makeRow(size_t index, float rowH);
    void showEmpty();
    void onToggleRow(CCObject* sender);
    void onPlayLevel(CCObject* sender);
    void onRemoveLevel(CCObject* sender);
    void onAddLevel(CCObject* sender);

public:
    static QuickPlayPopup* create();
    void show() override;
    void refreshList();
};
