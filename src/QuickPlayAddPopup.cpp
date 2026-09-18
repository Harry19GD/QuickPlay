#include "QuickPlayAddPopup.hpp"
#include "QuickPlayPopup.hpp"
#include "QuickPlayUI.hpp"

#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;
using namespace quickplay::ui;

namespace {

constexpr float kPopupW = 340.f;
constexpr float kPopupH = 250.f;
constexpr float kListW = 300.f;
constexpr float kListH = 128.f;
constexpr float kRowW = 336.f;
constexpr float kRowH = 42.f;
constexpr float kGap = 4.f;
constexpr int kMaxSaved = 80;

} // namespace

QuickPlayAddPopup* QuickPlayAddPopup::create(QuickPlayPopup* parent) {
    auto ret = new QuickPlayAddPopup();
    if (ret->init(kPopupW, kPopupH) && ret->build(parent)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool QuickPlayAddPopup::build(QuickPlayPopup* parent) {
    m_parent = parent;
    setTitle("Add Level", "goldFont.fnt", 0.68f, 18.f);

    m_idInput = TextInput::create(210.f, "Level ID");
    m_idInput->setCommonFilter(CommonFilter::Uint);
    m_idInput->setMaxCharCount(10);
    m_idInput->setLabel("By ID");
    m_idInput->setID("quickplay-id-input");
    m_mainLayer->addChildAtPosition(
        m_idInput, Anchor::Top, {-28.f, -68.f}, kHalf);

    auto idMenu = CCMenu::create();
    idMenu->setContentSize({48.f, 48.f});
    m_mainLayer->addChildAtPosition(idMenu, Anchor::Top, {132.f, -72.f}, kHalf);

    auto addBtn = spriteButton(
        this, menu_selector(QuickPlayAddPopup::onAddByID),
        "GJ_plusBtn_001.png", "+", 28.f);
    addBtn->setID("quickplay-id-add");
    idMenu->addChildAtPosition(addBtn, Anchor::Center, {0.f, 0.f}, kHalf);

    auto savedLabel = label("Saved levels", "goldFont.fnt", 0.38f, {255, 255, 255});
    m_mainLayer->addChildAtPosition(
        savedLabel, Anchor::Top, {0.f, -104.f}, kHalf);

    auto host = listHost({kListW, kListH}, m_scroll);
    m_mainLayer->addChildAtPosition(host, Anchor::Bottom, { -4.f, 22.f + kListH / 2.f }, kHalf);

    refreshSaved();
    return true;
}

void QuickPlayAddPopup::refreshSaved() {
    m_saved.clear();
    if (auto glm = GameLevelManager::sharedState()) {
        if (auto arr = glm->getSavedLevels(false, 0)) {
            for (int i = 0; i < arr->count() && (int)m_saved.size() < kMaxSaved; i++) {
                if (auto lvl = typeinfo_cast<GJGameLevel*>(arr->objectAtIndex(i))) {
                    m_saved.push_back(lvl);
                }
            }
        }
    }

    auto pinned = quickplay::loadList();
    auto content = m_scroll->m_contentLayer;
    content->removeAllChildren();
    content->setContentSize({kListW, kListH});

    if (m_saved.empty()) {
        auto hint = label("No saved levels on this account.", "chatFont.fnt", 0.55f, {180, 180, 180});
        fitWidth(hint, kListW - 24.f, 0.55f, 0.4f);
        content->addChildAtPosition(hint, Anchor::Center, {0.f, 0.f}, kHalf);
        return;
    }

    float total = 4.f + (float)m_saved.size() * (kRowH + kGap);
    float contentH = total > kListH ? total : kListH;
    content->setContentSize({kListW, contentH});

    float y = contentH - 4.f;
    for (size_t i = 0; i < m_saved.size(); i++) {
        auto level = m_saved[i];
        y -= kRowH;

        quickplay::Entry e;
        try { e.id = level->m_levelID.value(); } catch (...) {}
        e.type = (int)GJLevelType::Saved;
        e.name = level->m_levelName.c_str();
        bool isPinned = false;
        for (auto& p : pinned) {
            if (quickplay::sameEntry(p, e)) { isPinned = true; break; }
        }

        auto row = CCNode::create();
        row->setContentSize({kRowW, kRowH});
        row->setAnchorPoint(kHalf);
        row->setPosition({kListW / 2.f, y + kRowH / 2.f});
        content->addChild(row);

        auto menu = CCMenu::create();
        menu->setContentSize({kRowW, kRowH});
        row->addChildAtPosition(menu, Anchor::Center, {0.f, 0.f}, kHalf);

        auto stripe = card({kRowW, kRowH}, isPinned);
        auto toggle = CCMenuItemSpriteExtra::create(
            stripe, this, menu_selector(QuickPlayAddPopup::onToggleSaved));
        toggle->setTag((int)i);
        toggle->setID("quickplay-saved-toggle");
        menu->addChildAtPosition(toggle, Anchor::Center, {0.f, 0.f}, kHalf);

        if (auto face = quickplay::faceForLevel(level)) {
            face->setScale(26.f / face->getContentSize().height);
            row->addChildAtPosition(face, Anchor::Left, {20.f, 0.f}, kHalf);
        }

        std::string name = level->m_levelName.c_str();
        auto nameLabel = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
        nameLabel->setAnchorPoint(kLeftMid);
        fitWidth(nameLabel, 250.f, 0.35f, 0.22f);
        row->addChildAtPosition(nameLabel, Anchor::Left, {40.f, 0.f}, kLeftMid);

        auto mark = label(isPinned ? "PINNED" : "ADD", "chatFont.fnt", 0.5f,
            isPinned ? ccColor3B{90, 255, 120} : ccColor3B{255, 225, 90});
        row->addChildAtPosition(mark, Anchor::Right, {-16.f, 0.f}, kRightMid);

        y -= kGap;
    }
    m_scroll->scrollToTop();
}

void QuickPlayAddPopup::onAddByID(CCObject*) {
    int id = 0;
    try {
        std::string s = m_idInput->getString().c_str();
        if (!s.empty()) id = std::stoi(s);
    } catch (...) {}
    if (id <= 0) {
        Notification::create("Enter a level ID first", NotificationIcon::Warning)->show();
        return;
    }

    GJGameLevel* lvl = nullptr;
    if (auto glm = GameLevelManager::sharedState()) {
        lvl = glm->getSavedLevel(id);
        if (!lvl) lvl = glm->getMainLevel(id, false);
    }
    if (!lvl) {
        Notification::create("Level not found", NotificationIcon::Error)->show();
        return;
    }

    quickplay::Entry e;
    e.id = id;
    e.type = (int)lvl->m_levelType;
    e.name = lvl->m_levelName.c_str();

    auto list = quickplay::loadList();
    for (auto& p : list) {
        if (quickplay::sameEntry(p, e)) {
            Notification::create("Already in Quick Play", NotificationIcon::Info)->show();
            return;
        }
    }
    if ((int)list.size() >= quickplay::kMaxEntries) {
        Notification::create("Quick Play is full", NotificationIcon::Warning)->show();
        return;
    }
    list.push_back(e);
    quickplay::saveList(list);
    m_idInput->setString("");
    Notification::create("Added to Quick Play", NotificationIcon::Success)->show();
    refreshSaved();
}

void QuickPlayAddPopup::onToggleSaved(CCObject* sender) {
    int i = sender ? sender->getTag() : -1;
    if (i < 0 || i >= (int)m_saved.size()) return;
    auto level = m_saved[i];

    quickplay::Entry e;
    try { e.id = level->m_levelID.value(); } catch (...) {}
    e.type = (int)GJLevelType::Saved;
    e.name = level->m_levelName.c_str();

    auto list = quickplay::loadList();
    for (auto it = list.begin(); it != list.end(); it++) {
        if (quickplay::sameEntry(*it, e)) {
            list.erase(it);
            quickplay::saveList(list);
            refreshSaved();
            return;
        }
    }
    if ((int)list.size() >= quickplay::kMaxEntries) {
        Notification::create("Quick Play is full", NotificationIcon::Warning)->show();
        return;
    }
    list.push_back(e);
    quickplay::saveList(list);
    refreshSaved();
}

void QuickPlayAddPopup::onClose(CCObject* sender) {
    if (m_parent) m_parent->refreshList();
    Popup::onClose(sender);
}
