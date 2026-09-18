#pragma once

#include <Geode/Geode.hpp>
#include "QuickPlay.hpp"

using namespace geode::prelude;

class QuickPlayDragMenu;

// Main-menu continue pill: last played level, play, and a chevron that
// opens the compact Quick Play list. The pill can also be grabbed and
// dragged downward to hide the bar entirely; a small arrow that peeks at
// the bottom of the screen brings it back with a bounce.
class QuickPlayBar : public CCNode {
    friend class QuickPlayDragMenu;

protected:
    CCLabelBMFont* m_nameLabel = nullptr;
    CCLabelBMFont* m_byLabel = nullptr;
    CCLabelBMFont* m_pctLabel = nullptr;
    CCMenuItemSpriteExtra* m_playBtn = nullptr;

    CCLayerColor* m_hintOverlay = nullptr;
    CCLabelBMFont* m_hintLabel = nullptr;
    CCMenu* m_stubMenu = nullptr;
    CCMenuItemSpriteExtra* m_stubItem = nullptr;

    bool m_visible = true;
    bool m_dragging = false;
    CCPoint m_homePos = {0.f, 0.f};
    float m_touchStartY = 0.f;

    bool init() override;
    void onPlay(CCObject* sender);
    void onOpenList(CCObject* sender);
    void refresh();

    // Pill drag-to-hide gestures, driven by the invisible grab item.
    bool onDragBegan(CCTouch* touch);
    void onDragMoved(CCTouch* touch);
    void onDragEnded(CCTouch* touch);
    void onDragCancelled();
    bool isDragTouch(CCTouch* touch) const;
    void showDragHint();
    void hideDragHint();
    void removeDragHint();
    void hideBar();
    void finishHide();
    void createStub();
    void removeStub();
    void onStubClick(CCObject* sender);

public:
    static QuickPlayBar* create();
};
