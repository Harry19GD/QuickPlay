#include "QuickPlayPopup.hpp"
#include "QuickPlayAddPopup.hpp"
#include "QuickPlayUI.hpp"

#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Notification.hpp>

using namespace geode::prelude;
using namespace quickplay::ui;

namespace {

constexpr float kPopupW = 300.f;
constexpr float kPopupH = 210.f;
constexpr float kListW = 272.f;
constexpr float kListH = 132.f;
constexpr float kRowW = 258.f;
constexpr float kRowCollapsed = 48.f;
constexpr float kRowExpanded = 78.f;
constexpr float kGap = 4.f;

inline float bandDY(float rowH) {
    return rowH / 2.f - kRowCollapsed / 2.f;
}

} // namespace

QuickPlayPopup* QuickPlayPopup::create() {
    auto ret = new QuickPlayPopup();
    if (ret->init(kPopupW, kPopupH) && ret->build()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void QuickPlayPopup::show() {
    Popup::show();
    setOpacity(40);
}

bool QuickPlayPopup::build() {
    setTitle("Quick Play", "goldFont.fnt", 0.52f, 14.f);

    auto addBtn = spriteButton(
        this, menu_selector(QuickPlayPopup::onAddLevel),
        "GJ_plusBtn_001.png", "+", 22.f);
    addBtn->setID("quickplay-add-button");
    m_buttonMenu->addChildAtPosition(addBtn, Anchor::TopRight, {-3.f, -3.f}, kHalf);

    m_countLabel = label("", "chatFont.fnt", 0.45f, {200, 200, 200});
    m_countLabel->setID("quickplay-count");
    m_mainLayer->addChildAtPosition(
        m_countLabel, Anchor::Top, {0.f, -32.f}, kHalf);

    auto host = listHost({kListW, kListH}, m_scroll, false);
    m_mainLayer->addChildAtPosition(host, Anchor::Center, {0.f, -12.f}, kHalf);

    refreshList();
    return true;
}

void QuickPlayPopup::refreshList() {
    m_entries = quickplay::loadList();
    m_levels.clear();
    m_pinned.clear();
    m_lastIndex = -1;

    if (auto last = quickplay::loadLastPlayed()) {
        bool found = false;
        for (size_t i = 0; i < m_entries.size(); i++) {
            if (quickplay::sameEntry(m_entries[i], *last)) {
                found = true;
                if (i != 0) {
                    auto e = m_entries[i];
                    m_entries.erase(m_entries.begin() + (ptrdiff_t)i);
                    m_entries.insert(m_entries.begin(), e);
                }
                m_lastIndex = 0;
                break;
            }
        }
        if (!found) {
            m_entries.insert(m_entries.begin(), *last);
            m_lastIndex = 0;
        }
    }

    std::vector<quickplay::Entry> kept;
    std::vector<char> keptPinned;
    int pruned = 0;
    auto pinnedList = quickplay::loadList();
    for (size_t i = 0; i < m_entries.size(); i++) {
        auto& e = m_entries[i];
        auto lvl = quickplay::resolve(e);
        bool isLast = (int)i == m_lastIndex;
        if (!lvl && isLast) {
            kept.push_back(e);
            keptPinned.push_back(false);
            m_levels.push_back(nullptr);
            continue;
        }
        if (lvl) {
            bool pin = false;
            for (auto& p : pinnedList) {
                if (quickplay::sameEntry(p, e)) { pin = true; break; }
            }
            kept.push_back(e);
            keptPinned.push_back(pin);
            m_levels.push_back(lvl);
        } else {
            pruned++;
        }
    }
    if (pruned > 0) {
        auto store = pinnedList;
        std::vector<quickplay::Entry> still;
        for (auto& p : store) {
            if (quickplay::resolve(p)) still.push_back(p);
        }
        quickplay::saveList(still);
        Notification::create(
            fmt::format("Removed {} unavailable level{}", pruned, pruned == 1 ? "" : "s"),
            NotificationIcon::Info)
            ->show();
    }
    m_entries = kept;
    m_pinned = keptPinned;
    m_lastIndex = -1;
    if (auto last = quickplay::loadLastPlayed()) {
        for (size_t i = 0; i < m_entries.size(); i++) {
            if (quickplay::sameEntry(m_entries[i], *last)) {
                m_lastIndex = (int)i;
                break;
            }
        }
    }
    if (m_expanded >= (int)m_entries.size()) m_expanded = -1;

    auto n = m_entries.size();
    m_countLabel->setString(n == 0
        ? "Nothing yet"
        : fmt::format("{} level{}", n, n == 1 ? "" : "s").c_str());

    rebuildContent();
}

void QuickPlayPopup::rebuildContent() {
    auto content = m_scroll->m_contentLayer;
    content->removeAllChildren();

    if (m_entries.empty()) {
        content->setContentSize({kListW, kListH});
        showEmpty();
        return;
    }

    float total = 8.f;
    for (size_t i = 0; i < m_entries.size(); i++) {
        total += ((int)i == m_expanded ? kRowExpanded : kRowCollapsed) + kGap;
    }
    float contentH = total > kListH ? total : kListH;
    content->setContentSize({kListW, contentH});

    float y = contentH - 4.f;
    for (size_t i = 0; i < m_entries.size(); i++) {
        bool open = (int)i == m_expanded;
        float h = open ? kRowExpanded : kRowCollapsed;
        y -= h;
        auto row = makeRow(i, h);
        row->setPosition({kListW / 2.f, y + h / 2.f});
        content->addChild(row);
        y -= kGap;
    }
    m_scroll->scrollToTop();
}

CCNode* QuickPlayPopup::makeRow(size_t index, float rowH) {
    auto level = m_levels[index];
    bool open = (int)index == m_expanded;
    bool last = (int)index == m_lastIndex;
    bool pin = index < m_pinned.size() && m_pinned[index];
    float dy = bandDY(rowH);

    auto row = CCNode::create();
    row->setContentSize({kRowW, rowH});
    row->setAnchorPoint(kHalf);

    auto menu = CCMenu::create();
    menu->setContentSize({kRowW, rowH});
    menu->setID("quickplay-row-menu");
    row->addChildAtPosition(menu, Anchor::Center, {0.f, 0.f}, kHalf);

    auto stripe = card({kRowW, rowH}, last || open);
    auto bgItem = CCMenuItemSpriteExtra::create(
        stripe, this, menu_selector(QuickPlayPopup::onToggleRow));
    bgItem->setTag((int)index);
    bgItem->setID("quickplay-row-toggle");
    bgItem->setZOrder(-1);
    menu->addChildAtPosition(bgItem, Anchor::Center, {0.f, 0.f}, kHalf);

    if (level) {
        if (auto face = quickplay::faceForLevel(level)) {
            face->setScale(28.f / face->getContentSize().height);
            row->addChildAtPosition(face, Anchor::Left, {20.f, dy}, kHalf);
        }
    }

    std::string name = level ? level->m_levelName.c_str() : m_entries[index].name;
    auto nameLabel = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
    nameLabel->setAnchorPoint(kLeftMid);
    fitWidth(nameLabel, 140.f, 0.32f, 0.2f);
    row->addChildAtPosition(nameLabel, Anchor::Left, {38.f, dy + (last ? 7.f : 0.f)}, kLeftMid);

    if (last) {
        auto badge = label("Last played", "chatFont.fnt", 0.4f, {90, 255, 200});
        badge->setAnchorPoint(kLeftMid);
        row->addChildAtPosition(badge, Anchor::Left, {38.f, dy - 8.f}, kLeftMid);
    }

    if (level) {
        auto playBtn = spriteButton(
            this, menu_selector(QuickPlayPopup::onPlayLevel),
            "GJ_playBtn2_001.png", ">", 26.f);
        playBtn->setTag((int)index);
        playBtn->setID("quickplay-row-play");
        menu->addChildAtPosition(playBtn, Anchor::Right, {-18.f, dy}, kHalf);
    }

    if (open && level) {
        int pct = quickplay::levelProgress(level);
        auto meta = label(
            fmt::format("{}  ·  {}%", quickplay::diffText(level), pct),
            "chatFont.fnt", 0.4f, {210, 200, 160});
        meta->setAnchorPoint(kLeftMid);
        fitWidth(meta, 170.f, 0.4f, 0.28f);
        row->addChildAtPosition(meta, Anchor::Left, {38.f, 14.f - rowH / 2.f}, kLeftMid);

        if (pin) {
            auto remBtn = spriteButton(
                this, menu_selector(QuickPlayPopup::onRemoveLevel),
                "GJ_deleteBtn_001.png", "X", 22.f);
            remBtn->setTag((int)index);
            remBtn->setID("quickplay-row-remove");
            menu->addChildAtPosition(remBtn, Anchor::BottomRight, {-18.f, 16.f}, kHalf);
        }
    }

    return row;
}

void QuickPlayPopup::showEmpty() {
    auto content = m_scroll->m_contentLayer;

    auto title = label("No levels yet", "goldFont.fnt", 0.42f);
    content->addChildAtPosition(title, Anchor::Center, {0.f, 18.f}, kHalf);

    auto sub = label("Play a level, or pin one with +", "chatFont.fnt", 0.48f, {185, 185, 185});
    fitWidth(sub, kListW - 24.f, 0.48f, 0.35f);
    content->addChildAtPosition(sub, Anchor::Center, {0.f, -6.f}, kHalf);
}

void QuickPlayPopup::onToggleRow(CCObject* sender) {
    int i = sender ? sender->getTag() : -1;
    if (i < 0 || i >= (int)m_entries.size()) return;
    m_expanded = (m_expanded == i) ? -1 : i;
    rebuildContent();
}

void QuickPlayPopup::onPlayLevel(CCObject* sender) {
    int i = sender ? sender->getTag() : -1;
    if (i < 0 || i >= (int)m_levels.size()) return;
    auto lvl = m_levels[i];
    if (lvl && quickplay::play(lvl)) return;
    Notification::create("Couldn't open that level", NotificationIcon::Error)->show();
    refreshList();
}

void QuickPlayPopup::onRemoveLevel(CCObject* sender) {
    int i = sender ? sender->getTag() : -1;
    if (i < 0 || i >= (int)m_entries.size()) return;
    auto list = quickplay::loadList();
    for (auto it = list.begin(); it != list.end(); it++) {
        if (quickplay::sameEntry(*it, m_entries[i])) {
            list.erase(it);
            break;
        }
    }
    quickplay::saveList(list);
    if (m_expanded == i) m_expanded = -1;
    else if (m_expanded > i) m_expanded--;
    Notification::create("Removed from Quick Play", NotificationIcon::Info)->show();
    refreshList();
}

void QuickPlayPopup::onAddLevel(CCObject*) {
    if (auto popup = QuickPlayAddPopup::create(this)) popup->show();
}
