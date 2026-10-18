#include "Menu.hpp"
#include "Lab.hpp"
#include "Style.hpp"

#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/Notification.hpp>

using namespace geode::prelude;

namespace fwl {

namespace {
    constexpr float kWidth = 340.f;
    constexpr float kHeight = 236.f;

    std::string formatTicks(int ticks) {
        int secs = ticks / kTicksPerSecond;
        return fmt::format("{}:{:02}", secs / 60, secs % 60);
    }

    Result<> doRecord() { return Lab::get().startRecording(); }
    Result<> doPlay() { return Lab::get().startPlayback(); }
    Result<> doScan() { return Lab::get().startScan(); }
}

LabPopup* LabPopup::create() {
    auto ret = new LabPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool LabPopup::init() {
    if (!Popup::init(kWidth, kHeight, style::kRoundedSquare)) return false;

    m_bgSprite->setColor(style::kPanel);
    m_bgSprite->setOpacity(250);
    this->setTitle("Frame Window Lab", style::kTitleFont, 0.55f, 18.f);
    if (m_title) m_title->setColor(style::kAccent);
    this->setID("popup"_spr);

    rebuild();
    return true;
}

void LabPopup::rebuild() {
    if (m_content) m_content->removeFromParent();
    if (m_actions) m_actions->removeFromParent();

    auto& lab = Lab::get();
    auto const& s = lab.settings();
    auto rec = lab.recording();
    auto layer = lab.layer();

    m_content = CCNode::create();
    m_content->setContentSize({kWidth, kHeight});
    m_mainLayer->addChildAtPosition(m_content, Anchor::BottomLeft, {0.f, 0.f}, false);

    // Level name.
    std::string levelName = layer && layer->m_level ? std::string(layer->m_level->m_levelName) : "No level";
    auto name = style::label(levelName, style::kBodyFont, 0.7f, style::kMuted);
    name->limitLabelWidth(kWidth - 40.f, 0.7f, 0.3f);
    name->setPosition({kWidth / 2, kHeight - 38.f});
    m_content->addChild(name);

    // Status card.
    float cardW = kWidth - 30.f, cardH = 74.f;
    auto card = style::panel({cardW, cardH}, style::kPanelRaised, 255);
    card->setPosition({kWidth / 2, kHeight - 54.f - cardH / 2});
    m_content->addChild(card);

    std::string line1, line2, line3;
    ccColor3B line3Color = style::kAccent;
    if (!rec) {
        line1 = "No recording for this level yet.";
        line2 = "Press Record and beat the level once.";
    }
    else {
        line1 = fmt::format("Recording: {} inputs  ·  {}", rec->inputs.size(), formatTicks(rec->endTick));
        size_t measured = 0, unknown = 0, useless = 0;
        for (auto const& w : rec->windows) {
            if (!w.measured) continue;
            measured++;
            if (w.unknown) unknown++;
            if (w.useless) useless++;
        }
        if (rec->scanComplete) {
            line2 = fmt::format("Scanned  ·  {} left out as useless", useless);
            if (unknown) line2 += fmt::format("  ·  {} unchecked", unknown);
        }
        else if (measured) {
            line2 = fmt::format("Partly scanned ({} inputs). Scan again to finish.", measured);
        }
        else {
            line2 = "Not scanned yet. Press Scan to measure every input.";
        }
    }
    switch (lab.mode()) {
        case Mode::Recording: line3 = "Recording now. Beat the level to save it."; line3Color = style::kRecord; break;
        case Mode::Playing: line3 = "Playing back."; break;
        case Mode::Scanning: line3 = fmt::format("Scanning: {}% done.", static_cast<int>(lab.scanProgress() * 100.f)); break;
        case Mode::Idle: break;
    }

    float y = kHeight - 54.f - 16.f;
    for (auto [text, color] : {
        std::pair{line1, style::kText}, std::pair{line2, style::kMuted}, std::pair{line3, line3Color}
    }) {
        if (text.empty()) continue;
        auto lbl = style::label(text, style::kBodyFont, 0.62f, color);
        lbl->limitLabelWidth(cardW - 20.f, 0.62f, 0.3f);
        lbl->setPosition({kWidth / 2, y});
        m_content->addChild(lbl);
        y -= 19.f;
    }

    // Counter summary chips.
    if (rec && rec->hasAnyResults()) {
        auto counts = lab.summaryCounts();
        auto chips = CCNode::create();
        float x = 0.f;
        for (size_t i = 0; i < s.rows.size() && i < counts.size(); i++) {
            auto const& row = s.rows[i];
            auto name = row.from == row.to ? fmt::format("{}f", row.from) : row.label;
            auto text = fmt::format("{}  {}", name, counts[i]);
            auto lbl = style::label(text, style::kTitleFont, 0.32f, row.color);
            lbl->setAnchorPoint({0.f, 0.5f});
            lbl->setPosition({x, 0.f});
            chips->addChild(lbl);
            x += lbl->getScaledContentWidth() + 14.f;
        }
        x -= 14.f;
        float maxW = kWidth - 30.f;
        if (x > maxW) chips->setScale(maxW / x);
        chips->setPosition({kWidth / 2 - std::min(x, maxW) / 2, 88.f});
        m_content->addChild(chips);
    }

    // Actions.
    m_actions = CCMenu::create();
    m_actions->setContentSize({kWidth, kHeight});
    m_actions->ignoreAnchorPointForPosition(false);
    m_mainLayer->addChildAtPosition(m_actions, Anchor::Center, {0.f, 0.f}, false);

    auto addButton = [&](std::string const& text, ccColor3B color, CCPoint pos, float width, auto callback) {
        auto face = style::buttonFace(text, color, width);
        auto item = CCMenuItemExt::createSpriteExtra(face, std::move(callback));
        item->setPosition(pos);
        m_actions->addChild(item);
        return item;
    };

    float bw = 96.f;
    float row1 = 56.f, row2 = 24.f;
    float cx = kWidth / 2;
    bool busy = lab.mode() != Mode::Idle;

    addButton("Record", style::kRecord, {cx - bw - 8.f, row1}, bw, [this](auto*) { runAndClose(doRecord); });
    addButton("Play", style::kAccent, {cx, row1}, bw, [this](auto*) { runAndClose(doPlay); });
    addButton("Scan", style::kGood, {cx + bw + 8.f, row1}, bw, [this](auto*) { runAndClose(doScan); });

    if (busy) {
        std::string stopText = lab.mode() == Mode::Scanning ? "Cancel scan" : "Stop";
        addButton(stopText, style::kText, {cx - bw - 8.f, row2}, bw, [this](auto*) {
            Lab::get().stop();
            rebuild();
        });
    }
    else {
        addButton("Delete", style::kMuted, {cx - bw - 8.f, row2}, bw, [this](auto*) { confirmDelete(); });
    }
    addButton("Settings", style::kText, {cx, row2}, bw, [](auto*) {
        openSettingsPopup(Mod::get(), false);
    });
    addButton("Help", style::kMuted, {cx + bw + 8.f, row2}, bw, [](auto*) {
        createQuickPopup(
            "How it works",
            "<cy>Record</c>: beat the level once; every click is saved with its tick.\n"
            "<cg>Scan</c>: each click is replayed earlier and later, one tick at a time, "
            "to find how many frames it could have been on.\n"
            "<cl>Play</c>: watch the run back with circles, counters and sounds.\n\n"
            "Recording and playback run the physics at 240 steps a second. "
            "Platformer levels and practice mode are not supported.",
            "OK", nullptr, 360.f, [](auto, bool) {}
        );
    });
}

void LabPopup::runAndClose(Result<> (*action)()) {
    // Close first: starting a mode resumes the game and leaves the pause menu.
    // Keep this popup alive until the call returns, since closing it frees
    // the button whose callback we are running in.
    Ref<LabPopup> keepAlive = this;
    this->onClose(nullptr);
    if (auto res = action(); !res) {
        Notification::create(res.unwrapErr(), NotificationIcon::Error, 3.f)->show();
    }
}

void LabPopup::confirmDelete() {
    if (!Lab::get().recording()) return;
    Ref<LabPopup> self = this;
    createQuickPopup(
        "Delete recording?",
        "This removes the recording and its scan results for this level.",
        "Cancel", "Delete",
        [self](auto, bool yes) {
            if (!yes) return;
            Lab::get().deleteCurrent();
            self->rebuild();
        }
    );
}

CCNode* createPauseButtonSprite() {
    auto sprite = CCSprite::createWithSpriteFrameName("fwl-button.png"_spr);
    if (sprite) {
        sprite->setScale(0.62f);
        return sprite;
    }
    // Fallback if the sprite sheet failed to load.
    return style::buttonFace("FW", style::kAccent, 34.f, 34.f, style::kPanel);
}

} // namespace fwl
