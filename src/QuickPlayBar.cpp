#include "QuickPlayBar.hpp"
#include "RecentLevelsPopup.hpp"
#include "QuickPlayUI.hpp"

#include <algorithm>

#include <Geode/ui/Notification.hpp>
#include <Geode/ui/NineSlice.hpp>

using namespace geode::prelude;
using namespace quickplay::ui;

namespace {

// Node 300x64 (origin bottom-left). Pill box node-local y 14..54 (300x40),
// caption rides the top edge, tab tip protrudes just below the pill.
constexpr float kBarW = 300.f;
constexpr float kBarH = 64.f;
constexpr float kPillY = 14.f;
constexpr float kPillW = 300.f;
constexpr float kPillH = 40.f;

// How far (in px) the pill must be pulled below its resting spot before
// releasing hides the bar instead of snapping it back.
constexpr float kHidePull = 55.f;

// Bar center when fully slid off the bottom of the screen.
constexpr float kHiddenY = -40.f;

// Center height of the resurface arrow (screen coords), so only its flat
// top part peeks above the bottom edge.
constexpr float kStubY = 12.f;

} // namespace

// Menu wrapper that lets the real buttons stay normal buttons while empty
// space on the pill acts as a drag handle.
class QuickPlayDragMenu : public CCMenu {
public:
    QuickPlayBar* m_bar = nullptr;
    bool m_draggingBar = false;

    static QuickPlayDragMenu* create(QuickPlayBar* bar) {
        auto ret = new QuickPlayDragMenu();
        if (ret && ret->init()) {
            ret->m_bar = bar;
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        if (CCMenu::ccTouchBegan(touch, event)) return true;
        if (!m_bar || !m_bar->isDragTouch(touch)) return false;
        m_draggingBar = m_bar->onDragBegan(touch);
        return m_draggingBar;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        if (m_draggingBar) {
            if (m_bar) m_bar->onDragMoved(touch);
            return;
        }
        CCMenu::ccTouchMoved(touch, event);
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        if (m_draggingBar) {
            m_draggingBar = false;
            if (m_bar) m_bar->onDragEnded(touch);
            return;
        }
        CCMenu::ccTouchEnded(touch, event);
    }

    void ccTouchCancelled(CCTouch* touch, CCEvent* event) override {
        if (m_draggingBar) {
            m_draggingBar = false;
            if (m_bar) m_bar->onDragCancelled();
            return;
        }
        CCMenu::ccTouchCancelled(touch, event);
    }
};

QuickPlayBar* QuickPlayBar::create() {
    auto ret = new QuickPlayBar();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool QuickPlayBar::init() {
    if (!CCNode::init()) return false;

    setContentSize({kBarW, kBarH});
    setAnchorPoint(kHalf);
    setID("quickplay-continue-bar");

    // Pill body: a real GD long-button texture (probe-verified present in the
    // menu sheet: 92x30 with genuine outline, bevel and rounded corners),
    // nine-sliced into the bar. Frames from not-yet-loaded sheets render as
    // flat fallback squares here, so only probe-verified names are used.
    if (auto pill = NineSlice::createWithSpriteFrameName(
            "GJ_longBtn01_001.png", {8.f, 20.f, 8.f, 20.f})) {
        pill->setContentSize({kPillW, kPillH});
        pill->setAnchorPoint({0.f, 0.f});
        pill->setPosition({0.f, kPillY});
        pill->setID("quickplay-pill-bg");
        addChild(pill);
    } else {
        auto flat = CCLayerColor::create({40, 44, 148, 255}, kPillW, kPillH);
        flat->ignoreAnchorPointForPosition(false);
        flat->setAnchorPoint({0.f, 0.f});
        flat->setPosition({0.f, kPillY});
        addChild(flat);
    }

    auto tag = label("Quick Play", "goldFont.fnt", 0.5f);
    tag->setAnchorPoint(kLeftMid);
    tag->setID("quickplay-pill-tag");
    addChildAtPosition(tag, Anchor::TopLeft, {16.f, -10.f}, kLeftMid);

    m_nameLabel = CCLabelBMFont::create("-", "bigFont.fnt");
    m_nameLabel->setColor({255, 255, 255});
    m_nameLabel->setAnchorPoint(kLeftMid);
    m_nameLabel->setID("quickplay-last-name");
    addChildAtPosition(m_nameLabel, Anchor::Left, {16.f, 3.5f}, kLeftMid);

    m_byLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_byLabel->setColor({255, 225, 95});
    m_byLabel->setAnchorPoint(kLeftMid);
    m_byLabel->setID("quickplay-last-by");
    addChildAtPosition(m_byLabel, Anchor::Left, {200.f, 2.5f}, kLeftMid);

    m_pctLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_pctLabel->setColor({255, 225, 95});
    m_pctLabel->setScale(0.5f);
    m_pctLabel->setAnchorPoint(kRightMid);
    m_pctLabel->setID("quickplay-last-pct");
    addChildAtPosition(m_pctLabel, Anchor::Right, {-55.f, 3.5f}, kRightMid);

    auto menu = QuickPlayDragMenu::create(this);
    menu->setContentSize(getContentSize());
    menu->setID("quickplay-bar-menu");
    addChildAtPosition(menu, Anchor::Center, {0.f, 0.f}, kHalf);

    // Play: single-ring badge sprite on its own — no wrapper circle, so
    // exactly one outline surrounds the triangle. Log line confirms it.
    CCNode* playImg = nullptr;
    if (auto spr = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png")) {
        float h = spr->getContentSize().height;
        if (h > 1.f) spr->setScale(34.f / h);
        playImg = spr;
    } else {
        auto t = CCLabelBMFont::create(">", "bigFont.fnt");
        t->setScale(0.9f);
        t->setColor({255, 255, 255});
        playImg = t;
    }
    m_playBtn = CCMenuItemSpriteExtra::create(
        playImg, this, menu_selector(QuickPlayBar::onPlay));
    m_playBtn->setID("quickplay-bar-play");
    menu->addChildAtPosition(m_playBtn, Anchor::Right, {-30.f, 2.5f}, kHalf);

    // Tab: GD's pink-style back arrow (GJ_arrow_03_001, BackButtonStyle's
    // Pink variant — the back-button look from your screenshot), rotated to
    // point down. Native direction is LEFT (Geode flips it X for right
    // arrows), so -90 faces it down. Untinted: the exact asset look wins
    // over recoloring. Same slot, size class and behavior as before.
    CCNode* tabImg = nullptr;
    if (auto spr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png")) {
        spr->setRotation(-90.f);
        tabImg = spr;
    }
    if (!tabImg) {
        auto v = CCLabelBMFont::create("v", "bigFont.fnt");
        v->setScale(0.5f);
        v->setColor({64, 196, 176});
        tabImg = v;
    }
    if (auto spr = typeinfo_cast<CCSprite*>(tabImg)) {
        // After the -90° rotation, local X maps to visual height and local
        // Y maps to visual width: half height (~11px) + intentionally wide
        // (~20px) instead of a uniform shrink.
        float w = spr->getContentSize().width;
        float h = spr->getContentSize().height;
        if (w > 1.f && h > 1.f) {
            spr->setScaleX(11.f / w);
            spr->setScaleY(20.f / h);
        }
    }
    auto chevron = CCMenuItemSpriteExtra::create(
        tabImg, this, menu_selector(QuickPlayBar::onOpenList));
    chevron->setID("quickplay-bar-chevron");
    menu->addChildAtPosition(chevron, Anchor::Bottom, {0.f, 16.f}, kHalf);

    refresh();
    return true;
}

void QuickPlayBar::refresh() {
    std::string name = "Play a level first";
    std::string creator;
    bool haveStats = false;
    int pct = 0;
    if (auto last = quickplay::loadLastPlayed()) {
        name = last->name.empty() ? "Unknown level" : last->name;
        if (auto lvl = quickplay::resolve(*last)) {
            creator = quickplay::creatorName(lvl);
            haveStats = true;
            pct = quickplay::levelProgress(lvl);
            if (pct <= 0 && quickplay::levelAttempts(lvl) <= 0) pct = -1;
        }
    }
    m_nameLabel->setString(name.c_str());
    fitWidth(m_nameLabel, 165.f, 0.6f, 0.3f);

    // Creator inline after the title, 2x smaller, gold. Truncates to the
    // space before the circle zone; hides when nothing would fit.
    float nameScale = m_nameLabel->getScale();
    float nameW = m_nameLabel->getScaledContentSize().width;
    float byScale = nameScale * 0.5f;
    float byX = 16.f + nameW + 8.f;
    float avail = 181.f - byX;
    int maxChars = (byScale > 0.01f) ? (int)(avail / (24.f * byScale)) : 0;
    if (!creator.empty() && maxChars >= 6) {
        m_byLabel->setVisible(true);
        m_byLabel->setScale(byScale);
        m_byLabel->setString(("by " + quickplay::trunc(creator, (size_t)(maxChars - 3))).c_str());
        m_byLabel->setPosition({byX, 32.f + 2.5f});
    } else {
        m_byLabel->setVisible(false);
    }

    // Best percent beside the play button (fixed right slot).
    if (haveStats) {
        m_pctLabel->setVisible(true);
        m_pctLabel->setString((pct < 0 ? "NEW" : fmt::format("{}%", pct)).c_str());
    } else {
        m_pctLabel->setVisible(false);
    }
}

void QuickPlayBar::onPlay(CCObject*) {
    if (auto lvl = quickplay::resolveLastPlayed()) {
        if (quickplay::play(lvl)) return;
    }
    Notification::create("No recent level to continue", NotificationIcon::Warning)->show();
}

void QuickPlayBar::onOpenList(CCObject*) {
    if (auto popup = RecentLevelsPopup::create()) popup->show();
}

bool QuickPlayBar::onDragBegan(CCTouch* touch) {
    if (!m_visible || m_dragging) return false;
    m_dragging = true;
    m_homePos = CCPoint(getPositionX(), getPositionY());
    m_touchStartY = touch->getLocation().y;
    stopAllActions();
    showDragHint();
    return true;
}

void QuickPlayBar::onDragMoved(CCTouch* touch) {
    if (!m_dragging) return;
    float dy = touch->getLocation().y - m_touchStartY;
    // Clamp: the pill can be pulled down, never pushed up past its home.
    float ny = std::min(m_homePos.y, m_homePos.y + dy);
    setPositionY(ny);
}

void QuickPlayBar::onDragEnded(CCTouch*) {
    if (!m_dragging) return;
    m_dragging = false;
    hideDragHint();
    float pulled = m_homePos.y - getPositionY();
    if (pulled >= kHidePull) {
        hideBar();
    } else {
        runAction(CCEaseOut::create(
            CCMoveTo::create(0.25f, m_homePos), 1.f));
    }
}

void QuickPlayBar::onDragCancelled() {
    if (!m_dragging) return;
    m_dragging = false;
    hideDragHint();
    stopAllActions();
    setPosition(m_homePos);
}

bool QuickPlayBar::isDragTouch(CCTouch* touch) const {
    if (!touch) return false;
    auto local = const_cast<QuickPlayBar*>(this)->convertToNodeSpace(touch->getLocation());
    return local.x >= 0.f && local.x <= kPillW &&
        local.y >= kPillY && local.y <= kPillY + kPillH;
}

// While the pill is held: a soft screen dim plus a one-line hint that the
// gesture hides the bar.
void QuickPlayBar::showDragHint() {
    auto parent = getParent();
    if (!parent) return;
    if (m_hintOverlay) return;
    stopActionByTag(1201);
    auto win = CCDirector::sharedDirector()->getWinSize();

    m_hintOverlay = CCLayerColor::create({0, 0, 0, 95}, win.width, win.height);
    m_hintOverlay->setID("quickplay-drag-overlay");
    m_hintOverlay->setOpacity(0);
    m_hintOverlay->runAction(CCFadeTo::create(0.12f, 95));
    parent->addChild(m_hintOverlay, 9);

    m_hintLabel = CCLabelBMFont::create("Drag Quick Play down to hide", "bigFont.fnt");
    m_hintLabel->setScale(0.5f);
    m_hintLabel->setColor({255, 225, 95});
    m_hintLabel->setOpacity(0);
    m_hintLabel->setID("quickplay-drag-hint");
    m_hintLabel->setPosition({win.width / 2.f, win.height * 0.60f});
    m_hintLabel->setAnchorPoint(kHalf);
    m_hintLabel->runAction(CCFadeIn::create(0.12f));
    parent->addChild(m_hintLabel, 10);
}

void QuickPlayBar::hideDragHint() {
    if (m_hintOverlay) {
        m_hintOverlay->stopAllActions();
        m_hintOverlay->runAction(CCFadeOut::create(0.12f));
    }
    if (m_hintLabel) {
        m_hintLabel->stopAllActions();
        m_hintLabel->runAction(CCFadeOut::create(0.12f));
    }
    auto cleanup = CCSequence::create(
        CCDelayTime::create(0.13f),
        CCCallFunc::create(this, callfunc_selector(QuickPlayBar::removeDragHint)),
        nullptr);
    cleanup->setTag(1201);
    runAction(cleanup);
}

void QuickPlayBar::removeDragHint() {
    if (m_hintOverlay) {
        m_hintOverlay->removeFromParent();
        m_hintOverlay = nullptr;
    }
    if (m_hintLabel) {
        m_hintLabel->removeFromParent();
        m_hintLabel = nullptr;
    }
}

// Slide the pill off the bottom of the screen, then plant the resurface
// arrow. The bar stays in the scene so its state (slot, name) survives,
// just parked out of view.
void QuickPlayBar::hideBar() {
    m_visible = false;
    auto p = getPosition();
    runAction(CCSequence::create(
        CCEaseIn::create(CCMoveTo::create(0.35f, {p.x, kHiddenY}), 2.f),
        CCCallFunc::create(this, callfunc_selector(QuickPlayBar::finishHide)),
        nullptr));
}

// The resurface arrow: the bar's own pink down-triangle, duplicated and
// parked so only its flat top peeks above the bottom-center edge.
void QuickPlayBar::createStub() {
    auto parent = getParent();
    if (!parent) return;
    if (m_stubMenu) return;
    auto win = CCDirector::sharedDirector()->getWinSize();

    auto menu = CCMenu::create();
    menu->setPosition({win.width / 2.f, kStubY});
    menu->setID("quickplay-resurface-menu");
    m_stubMenu = menu;

    auto img = quickplay::iconOrText("GJ_arrow_03_001.png", "^", 0.5f);
    if (auto spr = typeinfo_cast<CCSprite*>(img)) {
        spr->setRotation(90.f);
        float w = spr->getContentSize().width;
        float h = spr->getContentSize().height;
        if (w > 1.f && h > 1.f) {
            spr->setScaleX(11.f / w);
            spr->setScaleY(20.f / h);
        }
    }
    auto item = CCMenuItemSpriteExtra::create(
        img, this, menu_selector(QuickPlayBar::onStubClick));
    item->setContentSize({44.f, 30.f});
    item->setID("quickplay-resurface");
    m_stubItem = item;

    item->setPosition({15.f, -8.f});
    item->setOpacity(0);
    item->runAction(CCFadeIn::create(0.2f));
    item->runAction(CCEaseOut::create(CCMoveTo::create(0.25f, {15.f, 0.f}), 1.5f));

    menu->addChild(item);
    parent->addChild(menu, 11);
}

void QuickPlayBar::finishHide() {
    createStub();
}

void QuickPlayBar::removeStub() {
    if (m_stubMenu) {
        m_stubMenu->removeFromParent();
        m_stubMenu = nullptr;
    }
    m_stubItem = nullptr;
}

void QuickPlayBar::onStubClick(CCObject*) {
    if (m_stubItem) {
        m_stubItem->setEnabled(false);
        m_stubItem->stopAllActions();
        m_stubItem->runAction(CCFadeOut::create(0.16f));
        m_stubItem->runAction(CCEaseIn::create(CCMoveTo::create(0.16f, {0.f, -8.f}), 2.f));
    }
    if (m_stubMenu) {
        auto cleanup = CCSequence::create(
            CCDelayTime::create(0.17f),
            CCCallFunc::create(this, callfunc_selector(QuickPlayBar::removeStub)),
            nullptr);
        m_stubMenu->runAction(cleanup);
    }
    m_visible = true;
    // Park at the hidden spot (already there) and bounce back up to home.
    auto p = getPosition();
    setPosition({p.x, kHiddenY});
    runAction(CCEaseBounceOut::create(CCMoveTo::create(0.65f, m_homePos)));
}
