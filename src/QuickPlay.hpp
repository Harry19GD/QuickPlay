#pragma once

// QuickPlay shared core: pinned-level storage, level resolution, and the
// instant-play launcher used by the menu button, the popup, and the
// level-screen add button.

#include <Geode/Geode.hpp>
#include <matjson.hpp>

#include <string>
#include <vector>
#include <optional>

using namespace geode::prelude;

namespace quickplay {

inline constexpr int kMaxEntries = 15;
inline constexpr const char* kSaveKey = "qp-levels";
inline constexpr int kMaxRecents = 12;
inline constexpr const char* kRecentKey = "qp-recent";
inline constexpr const char* kLastKey = "qp-last";

// How many recent levels are kept, from the setting (falls back to the
// built-in cap when the setting is missing or invalid).
inline int maxRecents() {
    try {
        auto mod = Mod::get();
        if (mod) return std::max(1, (int)mod->getSettingValue<int64_t>("max-recents"));
    } catch (...) {}
    return kMaxRecents;
}

struct Entry {
    int id = 0;
    int type = 0; // (int)GJLevelType
    std::string name;
    // Rating snapshotted at play time (GJDifficulty value incl. demon
    // tiers, plus stars). The local saved copy that resolve() returns can
    // lose this metadata, so we keep it on the record as a display fallback.
    int diff = -1;
    int stars = 0;
};

// Final GJDifficulty value for a level. GJDifficultySprite expects the
// base difficulty (-1..10). Demons' base 6 must be upgraded to the tier
// enum (7..10). Locally saved copies of online/featured levels ship with
// m_difficulty = 0 (Auto) even when rated — the true rating must be
// reconstructed from stars, exactly like the game's own display.
inline int levelDiffValue(GJGameLevel* level) {
    if (!level) return -1;

    int diff = -1;
    try { diff = (int)level->m_difficulty; } catch (...) {}

    bool isDemon = false;
    try { isDemon = level->m_demon.value(); } catch (...) {}

    // Demons: tier first, base Demon fallback.
    if (isDemon) {
        int tier = 0;
        try { tier = level->m_demonDifficulty; } catch (...) {}
        switch (tier) {
            case 3: return 7;
            case 4: return 8;
            case 5: return 9;
            case 6: return 10;
            default: return 6;
        }
    }

    // Rated copies carry stars; rebuild the difficulty from them.
    if (diff <= 0) {
        int stars = 0;
        try { stars = level->m_stars.value(); } catch (...) {}
        if (stars <= 0) return -1;              // genuinely unrated
        if (level->m_autoLevel || stars <= 1) return 0; // Auto
        if (stars == 2) return 1;               // Easy
        if (stars == 3) return 2;               // Normal
        if (stars <= 5) return 3;               // Hard
        if (stars <= 7) return 4;               // Harder
        if (stars <= 9) return 5;               // Insane
        return 6;                               // Demon
    }

    if (diff < -1 || diff > 10) return -1;
    return diff;
}

// Base difficulty-face sprite for a GJDifficulty value (no feature glow).
inline GJDifficultySprite* faceForDifficulty(int diff) {
    return GJDifficultySprite::create(diff, GJDifficultyName::Short);
}

inline std::vector<Entry> loadList() {
    std::vector<Entry> out;
    try {
        auto mod = Mod::get();
        if (!mod) return out;
        std::string raw = mod->getSavedValue<std::string>(kSaveKey);
        if (raw.empty()) return out;
        auto parsed = matjson::parse(raw);
        if (!parsed) return out;
        for (auto& v : parsed.unwrap()) {
            Entry e;
            e.id = v["id"].asInt().unwrapOr(0);
            e.type = v["type"].asInt().unwrapOr(0);
            e.name = v["name"].asString().unwrapOr("");
            if (e.id != 0 || !e.name.empty()) out.push_back(std::move(e));
            if ((int)out.size() >= kMaxEntries) break;
        }
    } catch (...) {}
    return out;
}

inline void saveList(std::vector<Entry> const& list) {
    try {
        auto mod = Mod::get();
        if (!mod) return;
        auto arr = matjson::Value::array();
        for (auto& e : list) {
            auto obj = matjson::Value::object();
            obj["id"] = e.id;
            obj["type"] = e.type;
            obj["name"] = e.name;
            arr.push(obj);
        }
        mod->setSavedValue<std::string>(kSaveKey, arr.dump());
    } catch (...) {}
}

inline Entry entryFromLevel(GJGameLevel* level) {
    Entry e;
    if (!level) return e;
    try { e.id = level->m_levelID.value(); } catch (...) {}
    e.type = (int)level->m_levelType;
    e.name = level->m_levelName.c_str();
    e.diff = levelDiffValue(level);
    try { e.stars = level->m_stars.value(); } catch (...) {}
    return e;
}

inline void saveLastPlayed(GJGameLevel* level) {
    if (!level) return;
    try {
        auto mod = Mod::get();
        if (!mod) return;
        auto e = entryFromLevel(level);
        auto obj = matjson::Value::object();
        obj["id"] = e.id;
        obj["type"] = e.type;
        obj["name"] = e.name;
        obj["diff"] = e.diff;
        obj["stars"] = e.stars;
        mod->setSavedValue<std::string>(kLastKey, obj.dump());
    } catch (...) {}
}

inline std::optional<Entry> loadLastPlayed() {
    try {
        auto mod = Mod::get();
        if (!mod) return std::nullopt;
        std::string raw = mod->getSavedValue<std::string>(kLastKey);
        if (raw.empty()) return std::nullopt;
        auto parsed = matjson::parse(raw);
        if (!parsed) return std::nullopt;
        auto v = parsed.unwrap();
        Entry e;
        e.id = v["id"].asInt().unwrapOr(0);
        e.type = v["type"].asInt().unwrapOr(0);
        e.name = v["name"].asString().unwrapOr("");
        e.diff = v["diff"].asInt().unwrapOr(-1);
        e.stars = v["stars"].asInt().unwrapOr(0);
        if (e.id == 0 && e.name.empty()) return std::nullopt;
        return e;
    } catch (...) {
        return std::nullopt;
    }
}

inline bool sameEntry(Entry const& a, Entry const& b) {
    if (a.type != b.type) return false;
    // Editor (local) levels may share id 0 pre-upload: compare by name.
    if (a.type == (int)GJLevelType::Editor) return a.name == b.name;
    return a.id == b.id;
}

// Creator display name with fallback: the cached object sometimes carries
// an empty m_creatorName (unrated/stale records) while the account lookup
// still knows the username. Never throws, never empty-checks by caller.
inline std::string creatorName(GJGameLevel* level) {
    if (!level) return "";
    try {
        if (!level->m_creatorName.empty()) return level->m_creatorName.c_str();
    } catch (...) {}
    int acc = 0;
    try { acc = level->m_accountID.value(); } catch (...) {}
    if (acc == 0) return "";
    try {
        if (auto glm = GameLevelManager::sharedState()) {
            std::string u = glm->tryGetUsername(acc).c_str();
            if (!u.empty()) return u;
        }
    } catch (...) {}
    try {
        if (auto gsm = GameStatsManager::sharedState()) {
            std::string u = gsm->usernameForAccountID(acc).c_str();
            if (!u.empty()) return u;
        }
    } catch (...) {}
    return "";
}

// Recently-played ring, most-recent-first. Updated by the PlayLayer hook
// every time a level starts; drives the menu pill + the recents popup.
inline std::vector<Entry> loadRecents() {
    std::vector<Entry> out;
    int cap = maxRecents();
    try {
        auto mod = Mod::get();
        if (!mod) return out;
        std::string raw = mod->getSavedValue<std::string>(kRecentKey);
        if (raw.empty()) return out;
        auto parsed = matjson::parse(raw);
        if (!parsed) return out;
        for (auto& v : parsed.unwrap()) {
            Entry e;
            e.id = v["id"].asInt().unwrapOr(0);
            e.type = v["type"].asInt().unwrapOr(0);
            e.name = v["name"].asString().unwrapOr("");
            e.diff = v["diff"].asInt().unwrapOr(-1);
            e.stars = v["stars"].asInt().unwrapOr(0);
            if (e.id != 0 || !e.name.empty()) out.push_back(std::move(e));
            if ((int)out.size() >= cap) break;
        }
    } catch (...) {}
    return out;
}

inline void saveRecents(std::vector<Entry> const& list) {
    try {
        auto mod = Mod::get();
        if (!mod) return;
        auto arr = matjson::Value::array();
        for (auto& e : list) {
            auto obj = matjson::Value::object();
            obj["id"] = e.id;
            obj["type"] = e.type;
            obj["name"] = e.name;
            obj["diff"] = e.diff;
            obj["stars"] = e.stars;
            arr.push(obj);
        }
        mod->setSavedValue<std::string>(kRecentKey, arr.dump());
    } catch (...) {}
}

// Move-to-front insert: replays bubble to the top instead of duplicating.
inline void pushRecent(Entry const& e) {
    if (e.id == 0 && e.name.empty()) return;
    int cap = maxRecents();
    auto list = loadRecents();
    for (auto it = list.begin(); it != list.end();) {
        if (sameEntry(*it, e)) it = list.erase(it);
        else ++it;
    }
    list.insert(list.begin(), e);
    if ((int)list.size() > cap) list.resize(cap);
    saveRecents(list);
}

// Resolve a stored entry back to a live level object. Returns nullptr when
// the level can no longer be found (deleted, un-saved, renamed...).
inline GJGameLevel* resolve(Entry const& e) {
    auto glm = GameLevelManager::sharedState();
    if (!glm) return nullptr;
    GJGameLevel* lvl = nullptr;
    try {
        if (e.type == (int)GJLevelType::Main) {
            lvl = glm->getMainLevel(e.id, false);
        } else if (e.type == (int)GJLevelType::Editor) {
            if (!e.name.empty()) lvl = glm->getLocalLevelByName(e.name.c_str());
        } else {
            // Prefer the in-memory online cache: it carries full metadata
            // (difficulty, feature, stars). Saved copies fall back — they
            // can arrive stripped, but display code derives from stars.
            if (e.id != 0 && glm->m_onlineLevels) {
                try {
                    if (auto key = glm->getLevelKey(e.id)) {
                        lvl = typeinfo_cast<GJGameLevel*>(
                            glm->m_onlineLevels->objectForKey(gd::string(key)));
                    }
                } catch (...) {}
            }
            if (!lvl) lvl = glm->getSavedLevel(e.id);
            if (!lvl) lvl = glm->getMainLevel(e.id, false);
        }
    } catch (...) {
        return nullptr;
    }
    return lvl;
}

inline GJGameLevel* resolveLastPlayed() {
    auto e = loadLastPlayed();
    if (!e) return nullptr;
    return resolve(*e);
}

// Instant-play: jump straight into a level the same way the game's own Play
// buttons do (real PlayLayer scene + fade transition, so attempts, stats,
// music and completion all behave normally).
inline bool play(GJGameLevel* level) {
    if (!level) return false;
    auto scene = PlayLayer::scene(level, false, false);
    if (!scene) return false;
    try {
        FMODAudioEngine::sharedEngine()->playEffect("playSound_01.ogg");
    } catch (...) {}
    CCDirector::sharedDirector()->replaceScene(
        CCTransitionFade::create(0.5f, scene));
    return true;
}

// Sprite-frame image with a text fallback, so the UI survives any missing
// asset: returns a node sized roughly like a small round GD button.
inline CCNode* iconOrText(const char* frame, const char* fallbackText, float fallbackScale = 0.7f) {
    if (auto spr = CCSprite::createWithSpriteFrameName(frame)) {
        return spr;
    }
    auto label = CCLabelBMFont::create(fallbackText, "bigFont.fnt");
    if (label) label->setScale(fallbackScale);
    return label;
}

// Difficulty face for a level row. GJDifficultySprite takes GJDifficulty
// values (-1..10): the level's base difficulty already is one, except
// demons (base Demon=6) whose tier maps to DemonEasy..DemonExtreme.
// Unrated (-1) passes through to the NA face instead of showing Auto.
inline GJDifficultySprite* faceForLevel(GJGameLevel* level) {
    if (!level) return nullptr;
    auto face = faceForDifficulty(levelDiffValue(level));
    if (face) {
        try {
            face->updateFeatureStateFromLevel(level);
        } catch (...) {}
    }
    return face;
}

// Human-readable difficulty line, ASCII-only (GD bitmap fonts lack glyphs
// like stars/bullets): "Easy Demon | 10 stars", "Hard | 5 stars", "Unrated".
inline std::string diffText(GJGameLevel* level) {
    if (!level) return "Unknown";
    std::string d;
    switch (levelDiffValue(level)) {
        case 0: d = "Auto"; break;
        case 1: d = "Easy"; break;
        case 2: d = "Normal"; break;
        case 3: d = "Hard"; break;
        case 4: d = "Harder"; break;
        case 5: d = "Insane"; break;
        case 6: d = "Demon"; break;
        case 7: d = "Easy Demon"; break;
        case 8: d = "Medium Demon"; break;
        case 9: d = "Insane Demon"; break;
        case 10: d = "Extreme Demon"; break;
        default: d = "Unrated"; break;
    }
    int stars = 0;
    try { stars = level->m_stars.value(); } catch (...) {}
    if (stars > 0 && levelDiffValue(level) >= 0) d += fmt::format(" | {} stars", stars);
    return d;
}

inline int levelAttempts(GJGameLevel* level) {
    if (!level) return 0;
    try { return level->m_attempts.value(); } catch (...) { return 0; }
}

inline int levelProgress(GJGameLevel* level) {
    if (!level) return 0;
    try { return level->m_normalPercent.value(); } catch (...) { return 0; }
}

inline std::string trunc(std::string const& s, size_t n) {
    if (s.size() <= n) return s;
    return s.substr(0, n) + "...";
}

} // namespace quickplay
