#include "RecentLevelsPopup.hpp"
#include "QuickPlayUI.hpp"

#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Notification.hpp>

using namespace geode::prelude;

namespace {

constexpr float kRowH = 60.f;
constexpr float kGap = 1.f;

constexpr CCPoint kHalf = {0.5f, 0.5f};

// Rounded-rect point loop (CCW) for CCDrawNode card backgrounds.
std::vector<CCPoint> qpRoundedRect(float w, float h, float r, int seg = 5) {
    std::vector<CCPoint> pts;
    constexpr float kPi = 3.14159265f;
    struct Arc { float cx, cy, start; };
    Arc arcs[4] = {
        {w - r, r, 270.f}, {w - r, h - r, 0.f},
        {r, h - r, 90.f}, {r, r, 180.f},
    };
    for (auto& a : arcs) {
        for (int i = 0; i <= seg; i++) {
            float ang = (a.start + 90.f * i / seg) * kPi / 180.f;
            pts.push_back(CCPoint(a.cx + r * cosf(ang), a.cy + r * sinf(ang)));
        }
    }
    return pts;
}

// Compact count text like GD stats (725K, 6.9M).
std::string qpCountText(int n) {
    std::string s;
    if (n >= 1000000) {
        float v = n / 1000000.f;
        s = (v >= 100.f) ? fmt::format("{}M", (int)v) : fmt::format("{:.1f}M", v);
    } else if (n >= 1000) {
        float v = n / 1000.f;
        s = (v >= 100.f) ? fmt::format("{}K", (int)v) : fmt::format("{:.1f}K", v);
    } else {
        s = fmt::format("{}", n);
    }
    auto p = s.find(".0");
    if (p != std::string::npos) s.erase(p, 2);
    return s;
}

std::string qpLengthText(GJGameLevel* level) {
    if (!level) return "";
    try {
        switch (level->m_levelLength) {
            case 0: return "Tiny";
            case 1: return "Short";
            case 2: return "Medium";
            case 3: return "Long";
            case 4: return "XL";
            default: return "";
        }
    } catch (...) {}
    return "";
}

int qpStat(GJGameLevel* level, int GJGameLevel::* member) {
    if (!level) return 0;
    try { return (level->*member); } catch (...) { return 0; }
}

} // namespace

RecentLevelsPopup* RecentLevelsPopup::create() {
    auto ret = new RecentLevelsPopup();
    // A roomy panel just short of the window edges — big enough that the
    // rows read comfortably at native font scales, small enough that it
    // clearly is a panel and not a screen takeover. Falls back to 380x235.
    float w = 380.f, h = 235.f;
    if (auto dir = CCDirector::sharedDirector()) {
        float winW = dir->getWinSize().width;
        float winH = dir->getWinSize().height;
        if (winW > 0.f) w = winW * 0.64f;
        if (winH > 0.f) h = winH * 0.82f;
    }
    if (ret->init(w, h) && ret->build()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool RecentLevelsPopup::build() {
    setTitle("RECENT LEVELS");

    auto tagline = CCLabelBMFont::create("Continue where you left off", "chatFont.fnt");
    tagline->setScale(0.75f);
    tagline->setColor({255, 255, 255});
    tagline->setID("quickplay-tagline");
    m_mainLayer->addChildAtPosition(tagline, Anchor::Top, {0.f, -39.f}, kHalf);

    auto size = m_mainLayer->getContentSize();

    // List spans the popup minus fixed margins (44 top for the title,
    // 48 bottom for the close zone).
    float listW = size.width - 40.f;
    float listH = size.height - 114.f;

    // Rounded list frame tracing the scroll area: a solid dark-chocolate
    // panel behind the list (the thin outline, now filled). Added first so
    // the scroll sits on top of it.
    auto frame = CCDrawNode::create();
    {
        ccColor4F fill = {90.f / 255.f, 46.f / 255.f, 20.f / 255.f, 1.f};
        auto pts = qpRoundedRect(listW + 16.f, listH + 16.f, 6.f);
        frame->drawPolygon(pts.data(), (unsigned)pts.size(), fill, 0.f, fill);
    }
    frame->setPosition({12.f, 48.f});
    frame->setID("quickplay-recent-frame");
    m_mainLayer->addChild(frame);

    // Plain bottom-left placement (ScrollLayer ignores its anchor).
    // Scroll spans the panel edge-to-edge top and bottom so cards clip on
    // both the panel's top and lower borders.
    m_scroll = ScrollLayer::create({listW, listH + 16.f});
    m_scroll->setPosition({(size.width - listW) / 2.f, 48.f});
    m_scroll->setID("quickplay-recent-list");
    m_mainLayer->addChild(m_scroll);

    auto closeSpr = ButtonSprite::create("CLOSE");
    closeSpr->setScale(0.7f);
    auto closeBtn = CCMenuItemSpriteExtra::create(
        closeSpr, this, menu_selector(RecentLevelsPopup::onCloseBtn));
    closeBtn->setID("quickplay-recent-close");
    m_buttonMenu->addChildAtPosition(closeBtn, Anchor::Bottom, {0.f, 28.f}, kHalf);

    rebuild();
    return true;
}

void RecentLevelsPopup::rebuild() {
    m_entries.clear();
    m_levels.clear();

    auto stored = quickplay::loadRecents();
    std::vector<quickplay::Entry> kept;
    for (auto& e : stored) {
        if (auto lvl = quickplay::resolve(e)) {
            kept.push_back(e);
            m_levels.push_back(lvl);
        }
    }
    if (kept.size() != stored.size()) quickplay::saveRecents(kept);
    m_entries = std::move(kept);

    // Full-bleed rows derived from the live popup size.
    float listW = m_mainLayer->getContentSize().width - 40.f;
    float listH = m_mainLayer->getContentSize().height - 114.f;
    float rowW = listW;

    auto content = m_scroll->m_contentLayer;
    content->removeAllChildren();
    content->setContentSize({listW, listH});

    if (m_entries.empty()) {
        auto hint = CCLabelBMFont::create("Play something first.", "chatFont.fnt");
        hint->setScale(0.55f);
        hint->setColor({180, 180, 180});
        content->addChildAtPosition(hint, Anchor::Center, {0.f, 0.f}, kHalf);
        return;
    }

    float total = 4.f + (float)m_entries.size() * (kRowH + kGap) + 16.f;
    float contentH = total > listH ? total : listH;
    content->setContentSize({listW, contentH});

    float y = contentH - 4.f - 16.f;
    for (size_t i = 0; i < m_entries.size(); i++) {
        y -= kRowH;
        auto row = buildRow(m_levels[i], (int)i, rowW, kRowH);
        row->setAnchorPoint({0.f, 0.f});
        row->setPosition({0.f, y});
        content->addChild(row);
        y -= kGap;
    }
    m_scroll->scrollToTop();
}

// Fully self-contained row: no native GD cell, so nothing inherits GD's
// full-browser layout. Every element is sized/placed as a fraction of the
// row box, which makes clipping or overlap impossible at any row width.
CCNode* RecentLevelsPopup::buildRow(GJGameLevel* level, int idx, float rowW, float rowH) {
    auto row = CCNode::create();
    row->setContentSize({rowW, rowH});
    row->setID("quickplay-recent-cell");

    // Rounded row background: translucent dark fill with a subtle brown
    // outline, replaced the sharp-cornered square02b nine-slice. Inset by a
    // couple of px so the outline isn't clipped by the scroll area's bounds.
    // Rows alternate two shades like the Saved list does.
    {
        auto bg = CCDrawNode::create();
        const float inset = 2.f;
        bool alt = (idx % 2) == 1;
        ccColor4F fill = alt
            ? ccColor4F{34.f / 255.f, 25.f / 255.f, 12.f / 255.f, 165.f}
            : ccColor4F{14.f / 255.f, 10.f / 255.f, 5.f / 255.f, 165.f};
        ccColor4F edge = alt
            ? ccColor4F{92.f / 255.f, 56.f / 255.f, 24.f / 255.f, 220.f}
            : ccColor4F{68.f / 255.f, 38.f / 255.f, 16.f / 255.f, 220.f};
        auto pts = qpRoundedRect(rowW - inset * 2.f, rowH - inset * 2.f, 11.f, 6);
        bg->drawPolygon(pts.data(), (unsigned)pts.size(), fill, 0.75f, edge);
        bg->setAnchorPoint({0.f, 0.f});
        bg->setPosition({inset, inset});
        row->addChild(bg, -1);
    }

    // GD saved-cell layout (measured from the real game):
    //  - tall name + small creator line, both left-anchored, horizontally
    //    centered as a two-line block
    //  - difficulty face to the LEFT of that text block
    //  - a slim length strip along the bottom
    //  - play button (and % before it) at the far right
    float faceH = std::min(rowH * 0.52f, 33.f);
    float left = 12.f;
    float faceCX = left + faceH / 2.f;

    if (auto face = quickplay::faceForLevel(level)) {
        float fh = face->getContentSize().height;
        if (fh > 1.f) face->setScale(faceH / fh);
        face->setAnchorPoint({0.5f, 0.5f});
        face->setPosition({faceCX, rowH * 0.54f});
        row->addChild(face);
    }

    float textLeft = left + faceH + 8.f;
    float playCx = rowW - 30.f;
    float pctRight = playCx - 17.f - 8.f;
    float rightLimit = std::min(pctRight, rowW - 30.f);

    // Name (top line) and creator (line under it), like GD's two-line block.
    float nameCY = rowH * 0.71f;
    float nameScale = 0.55f;
    float nameMaxW = std::max(60.f, rightLimit - textLeft - 20.f);
    auto name = quickplay::ui::label(
        level->m_levelName.c_str(), "bigFont.fnt", nameScale, {255, 255, 255});
    name->setAnchorPoint({0.f, 0.5f});
    name->setPosition({textLeft, nameCY});
    row->addChild(name);
    name->limitLabelWidth(nameMaxW, nameScale, 0.30f);

    float creatorScale = 0.34f;
    auto creator = quickplay::ui::label(
        quickplay::creatorName(level).c_str(), "bigFont.fnt", creatorScale, {255, 213, 0});
    creator->setAnchorPoint({0.f, 0.5f});
    creator->setPosition({textLeft, rowH * 0.44f});
    row->addChild(creator);
    creator->limitLabelWidth(nameMaxW, creatorScale, 0.22f);

    // Slim length strip along the bottom, text-only.
    float statY = rowH * 0.16f;
    auto len = quickplay::ui::label(
        qpLengthText(level).c_str(), "bigFont.fnt", 0.32f, {175, 175, 175});
    len->setAnchorPoint({0.f, 0.5f});
    len->setPosition({textLeft, statY});
    row->addChild(len);

    // Percentage, right-anchored before the play button at row center.
    int pct = quickplay::levelProgress(level);
    if (pct > 0) {
        auto pLab = quickplay::ui::label(
            fmt::format("{}%", pct), "bigFont.fnt", 0.42f, {130, 255, 150});
        pLab->setAnchorPoint({1.f, 0.5f});
        pLab->setPosition({pctRight, rowH * 0.5f});
        row->addChild(pLab);
    }

    // Play button: a tiny menu so only the glyph is tappable.
    auto menu = CCMenu::create();
    menu->setContentSize({rowW, rowH});
    menu->setAnchorPoint({0.f, 0.f});
    menu->setPosition({0.f, 0.f});
    menu->setZOrder(2);
    row->addChild(menu);

    auto playSpr = quickplay::iconOrText("GJ_playBtn2_001.png", ">", 0.7f);
    if (auto spr = typeinfo_cast<CCSprite*>(playSpr)) {
        float h = spr->getContentSize().height;
        if (h > 1.f) spr->setScale(34.f / h);
    }
    auto play = CCMenuItemSpriteExtra::create(
        playSpr, this, menu_selector(RecentLevelsPopup::onPlayLevel));
    play->setTag(idx);
    play->setPosition({playCx, rowH * 0.5f});
    play->setID("quickplay-recent-play");
    menu->addChild(play);

    // In-game X button sits half-straddling the top-left corner of the
    // card, like the "Quick Play" label that hangs on the pill edge in the
    // main menu. The protrusion above the row is visible because rows
    // start below a small top padding reserved in rebuild().
    {
        auto rmSpr = quickplay::iconOrText("GJ_deleteIcon_001.png", "X", 0.7f);
        if (auto spr = typeinfo_cast<CCSprite*>(rmSpr)) {
            float h = spr->getContentSize().height;
            if (h > 1.f) spr->setScale(14.f / h);
        }
        if (auto lab = typeinfo_cast<CCLabelBMFont*>(rmSpr)) {
            lab->setColor({235, 60, 50});
        }
        auto rm = CCMenuItemSpriteExtra::create(
            rmSpr, this, menu_selector(RecentLevelsPopup::onRemoveLevel));
        rm->setTag(idx);
        rm->setPosition({7.f, rowH - 4.f});
        rm->setID("quickplay-recent-remove");
        menu->addChild(rm);
    }

    return row;
}

void RecentLevelsPopup::onPlayLevel(CCObject* sender) {
    int i = sender ? sender->getTag() : -1;
    if (i < 0 || i >= (int)m_levels.size()) return;
    auto lvl = m_levels[i];
    // NB: successful play replaces the scene, destroying this popup —
    // so play() must be the last thing we do here.
    if (lvl && quickplay::play(lvl)) return;
    Notification::create("Couldn't open that level", NotificationIcon::Error)->show();
}

void RecentLevelsPopup::onRemoveLevel(CCObject* sender) {
    int i = sender ? sender->getTag() : -1;
    if (i < 0 || i >= (int)m_entries.size()) return;
    m_entries.erase(m_entries.begin() + i);
    m_levels.erase(m_levels.begin() + i);
    quickplay::saveRecents(m_entries);
    Notification::create("Removed from recents", NotificationIcon::Info)->show();
    rebuild();
}

void RecentLevelsPopup::onCloseBtn(CCObject* sender) {
    onClose(sender);
}

void RecentLevelsPopup::show() {
    Popup::show();
    // Pin the scale from the first frame: stop the engine's open animation
    // (it drives main-layer scale) so the size can never jump afterwards.
    if (m_mainLayer) {
        m_mainLayer->stopAllActions();
        m_mainLayer->setScale(1.05f);
    }
    animateWindowIn();
}

// Whole window drops in from above with a bounce, like it fell onto the menu.
void RecentLevelsPopup::animateWindowIn() {
    if (!m_mainLayer) return;
    float drop = CCDirector::sharedDirector()->getWinSize().height;
    m_mainLayer->stopAllActions();
    CCPoint base = m_mainLayer->getPosition();
    m_mainLayer->setPosition({base.x, base.y + drop});
    m_mainLayer->runAction(CCEaseBounceOut::create(
        CCMoveTo::create(0.85f, base)));
}