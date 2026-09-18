#pragma once

#include "QuickPlay.hpp"

#include <Geode/ui/General.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/ui/NineSlice.hpp>

namespace quickplay::ui {

inline constexpr CCPoint kHalf = {0.5f, 0.5f};
inline constexpr CCPoint kLeftMid = {0.f, 0.5f};
inline constexpr CCPoint kRightMid = {1.f, 0.5f};

inline CCNode* card(CCSize size, bool highlight) {
    auto color = highlight ? ccColor3B{92, 68, 18} : ccColor3B{0, 0, 0};
    GLubyte opacity = highlight ? 210 : 85;

    NineSlice* spr = NineSlice::createWithSpriteFrameName("square02b_001.png");
    if (!spr) spr = NineSlice::create("square02b_001.png");
    if (spr) {
        spr->setContentSize(size);
        spr->setColor(color);
        spr->setOpacity(opacity);
        spr->setAnchorPoint(kHalf);
        return spr;
    }

    auto layer = CCLayerColor::create(
        {color.r, color.g, color.b, opacity}, size.width, size.height);
    layer->ignoreAnchorPointForPosition(false);
    layer->setAnchorPoint(kHalf);
    return layer;
}

inline CCNode* listHost(CCSize size, ScrollLayer*& outScroll, bool scrollbar = true) {
    auto host = CCNode::create();
    host->setContentSize(size);
    host->setAnchorPoint(kHalf);
    host->setID("quickplay-list-host");

    auto bg = CCLayerColor::create({0, 0, 0, 95}, size.width, size.height);
    bg->setID("quickplay-list-bg");
    host->addChild(bg);

    outScroll = ScrollLayer::create(size);
    outScroll->setID("quickplay-scroll");
    host->addChild(outScroll);

    if (auto borders = ListBorders::create()) {
        borders->setContentSize(size);
        borders->setID("quickplay-list-borders");
        host->addChild(borders, 2);
    }
    if (scrollbar) {
        if (auto bar = Scrollbar::create(outScroll)) {
            bar->setID("quickplay-scrollbar");
            bar->setPosition({size.width - 5.f, size.height / 2.f});
            host->addChild(bar, 3);
        }
    }
    return host;
}

inline CCNode* pill(CCSize size) {
    NineSlice* spr = NineSlice::createWithSpriteFrameName("GJ_button_04.png");
    if (!spr) spr = NineSlice::create("GJ_button_04.png");
    if (spr) {
        spr->setContentSize(size);
        spr->setAnchorPoint(kHalf);
        return spr;
    }
    return card(size, false);
}

inline CCMenuItemSpriteExtra* spriteButton(
    CCNode* target,
    SEL_MenuHandler sel,
    char const* frame,
    char const* fallback,
    float height
) {
    auto img = iconOrText(frame, fallback, 0.75f);
    if (auto spr = typeinfo_cast<CCSprite*>(img)) {
        float h = spr->getContentSize().height;
        if (h > 1.f) spr->setScale(height / h);
    }
    return CCMenuItemSpriteExtra::create(img, target, sel);
}

inline CCLabelBMFont* label(
    std::string const& text,
    char const* font,
    float scale,
    ccColor3B color = {255, 255, 255}
) {
    auto lab = CCLabelBMFont::create(text.c_str(), font);
    lab->setScale(scale);
    lab->setColor(color);
    return lab;
}

inline void fitWidth(CCLabelBMFont* lab, float width, float maxScale, float minScale) {
    if (!lab) return;
    lab->limitLabelWidth(width, maxScale, minScale);
}

inline ccColor3B progressColor(int pct, int attempts) {
    if (pct <= 0 && attempts <= 0) return {120, 220, 255};
    if (pct >= 100) return {90, 255, 120};
    return {255, 225, 90};
}

} // namespace quickplay::ui
