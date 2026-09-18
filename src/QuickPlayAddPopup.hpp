#pragma once

#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include "QuickPlay.hpp"

using namespace geode::prelude;

class QuickPlayPopup;

// Add interface: enter a level ID, or toggle levels from the saved list.
// Refreshes the parent browser on close so it never shows stale entries.
class QuickPlayAddPopup : public geode::Popup {
protected:
    QuickPlayPopup* m_parent = nullptr;
    ScrollLayer* m_scroll = nullptr;
    TextInput* m_idInput = nullptr;
    std::vector<GJGameLevel*> m_saved;

    bool build(QuickPlayPopup* parent);
    void refreshSaved();
    void onAddByID(CCObject* sender);
    void onToggleSaved(CCObject* sender);
    void onClose(CCObject* sender) override;

public:
    static QuickPlayAddPopup* create(QuickPlayPopup* parent);
};
