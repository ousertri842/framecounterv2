#include "Overlay.hpp"
#include "Lab.hpp"
#include "Sounds.hpp"
#include "Style.hpp"

#include <Geode/binding/LevelSettingsObject.hpp>
#include <Geode/binding/PlayerObject.hpp>

#include <cmath>

using namespace geode::prelude;

namespace fwl {

namespace {
    Overlay* s_current = nullptr;

    ccColor4F toColor4F(ccColor3B c, float alpha = 1.f) {
        return {c.r / 255.f, c.g / 255.f, c.b / 255.f, alpha};
    }

    std::string rowCaption(CounterRow const& row) {
        if (row.from == row.to) return fmt::format("{} {}", row.from, row.from == 1 ? "frame" : "frames");
        if (row.to == INT_MAX) return fmt::format("{}+ frames", row.from);
        return fmt::format("{}-{} frames", row.from, row.to);
    }
}

Overlay* Overlay::create(PlayLayer* layer) {
    auto ret = new Overlay();
    if (ret->initWithLayer(layer)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

Overlay* Overlay::get() {
    return s_current;
}

Overlay::~Overlay() {
    if (s_current == this) s_current = nullptr;
    if (m_circles) m_circles->removeFromParent();
}

bool Overlay::initWithLayer(PlayLayer* layer) {
    if (!CCNode::init()) return false;
    m_layer = layer;
    s_current = this;

    auto win = CCDirector::get()->getWinSize();
    this->setContentSize(win);
    this->setID("overlay"_spr);

    // Circles live in the object layer so they scroll with the level.
    m_circles = CCNode::create();
    m_circles->setID("circles"_spr);
    if (layer->m_objectLayer) layer->m_objectLayer->addChild(m_circles, 1000);

    buildCounter();
    buildHud();
    onModeChanged();
    this->scheduleUpdate();
    return true;
}

// ---- counter rows --------------------------------------------------------

void Overlay::buildCounter() {
    if (m_counter) m_counter->removeFromParent();
    m_countLabels.clear();

    auto const& s = Lab::get().settings();
    m_counts.assign(s.rows.size(), 0);

    float rowH = 15.f;
    float width = 112.f;
    float height = 22.f + rowH * static_cast<float>(s.rows.size());

    m_counter = CCNode::create();
    m_counter->setContentSize({width, height});
    m_counter->setAnchorPoint({0.f, 1.f});

    auto bg = style::panel({width, height}, style::kPanel, 190);
    bg->setPosition({width / 2, height / 2});
    m_counter->addChild(bg);

    auto header = style::label("FRAME WINDOWS", style::kTitleFont, 0.26f, style::kMuted);
    header->setAnchorPoint({0.f, 0.5f});
    header->setPosition({8.f, height - 10.f});
    m_counter->addChild(header);

    for (size_t i = 0; i < s.rows.size(); i++) {
        auto const& row = s.rows[i];
        float y = height - 24.f - rowH * static_cast<float>(i);

        auto dot = CCDrawNode::create();
        dot->drawDot({0.f, 0.f}, 3.f, toColor4F(row.color));
        dot->setPosition({11.f, y});
        m_counter->addChild(dot);

        auto name = style::label(rowCaption(row), style::kTitleFont, 0.3f, row.color);
        name->setAnchorPoint({0.f, 0.5f});
        name->setPosition({19.f, y});
        m_counter->addChild(name);

        auto count = style::label("0", style::kTitleFont, 0.34f, style::kText);
        count->setAnchorPoint({1.f, 0.5f});
        count->setPosition({width - 8.f, y});
        m_counter->addChild(count);
        m_countLabels.push_back(count);
    }

    float scale = static_cast<float>(s.counterScale);
    m_counter->setScale(scale);
    m_counter->setPosition({6.f, this->getContentHeight() - 26.f});
    this->addChild(m_counter, 5);
}

void Overlay::refreshCounter() {
    for (size_t i = 0; i < m_countLabels.size() && i < m_counts.size(); i++) {
        m_countLabels[i]->setString(fmt::format("{}", m_counts[i]).c_str());
    }
}

// ---- status pill and scan panel ---------------------------------------

void Overlay::buildHud() {
    auto win = this->getContentSize();

    // Small status pill at the top.
    m_pill = CCNode::create();
    m_pill->setContentSize({130.f, 20.f});
    m_pill->setAnchorPoint({0.5f, 0.5f});
    auto pillBg = style::panel({130.f, 20.f}, style::kPanel, 200);
    pillBg->setPosition({65.f, 10.f});
    m_pill->addChild(pillBg);

    auto dot = CCDrawNode::create();
    dot->drawDot({0.f, 0.f}, 3.5f, toColor4F(style::kRecord));
    dot->setPosition({12.f, 10.f});
    m_pill->addChild(dot);
    m_pillDot = dot;

    m_pillLabel = style::label("REC", style::kTitleFont, 0.3f, style::kText);
    m_pillLabel->setAnchorPoint({0.f, 0.5f});
    m_pillLabel->setPosition({21.f, 10.5f});
    m_pill->addChild(m_pillLabel);

    m_pill->setPosition({win.width / 2, win.height - 34.f});
    this->addChild(m_pill, 6);

    // Scan: dim the game and show a progress card.
    m_dim = CCLayerColor::create({8, 10, 18, 150});
    m_dim->setContentSize(win);
    this->addChild(m_dim, 1);

    float w = 220.f, h = 70.f;
    m_scanPanel = CCNode::create();
    m_scanPanel->setContentSize({w, h});
    m_scanPanel->setAnchorPoint({0.5f, 0.5f});
    auto card = style::panel({w, h}, style::kPanel, 245);
    card->setPosition({w / 2, h / 2});
    m_scanPanel->addChild(card);

    m_scanTitle = style::label("Scanning", style::kTitleFont, 0.45f, style::kAccent);
    m_scanTitle->setPosition({w / 2, h - 16.f});
    m_scanPanel->addChild(m_scanTitle);

    m_scanStatus = style::label("", style::kBodyFont, 0.6f, style::kMuted);
    m_scanStatus->setPosition({w / 2, h - 34.f});
    m_scanPanel->addChild(m_scanStatus);

    m_barWidth = w - 30.f;
    auto track = CCLayerColor::create({44, 58, 88, 255}, m_barWidth, 6.f);
    track->setPosition({15.f, 14.f});
    m_scanPanel->addChild(track);
    m_scanFill = CCLayerColor::create({136, 180, 255, 255}, 0.f, 6.f);
    m_scanFill->setPosition({15.f, 14.f});
    m_scanPanel->addChild(m_scanFill);

    m_scanPanel->setPosition({win.width / 2, win.height / 2 - 40.f});
    this->addChild(m_scanPanel, 7);
}

void Overlay::onModeChanged() {
    auto mode = Lab::get().mode();
    auto const& s = Lab::get().settings();

    // Rebuild on every playback start so colour and row changes apply.
    if (mode == Mode::Playing || m_counts.size() != s.rows.size()) buildCounter();
    if (m_counter) m_counter->setVisible(mode == Mode::Playing && s.counters);

    m_pill->setVisible(mode == Mode::Recording || mode == Mode::Playing);
    m_pillDot->setVisible(mode == Mode::Recording);
    m_dim->setVisible(mode == Mode::Scanning);
    m_scanPanel->setVisible(mode == Mode::Scanning);
    if (m_circles) m_circles->setVisible(mode != Mode::Scanning);

    if (mode == Mode::Playing) {
        m_pillLabel->setString("PLAYBACK");
        m_pillLabel->setColor(style::kAccent);
        m_pillLabel->setPositionX(12.f);
    }
    else if (mode == Mode::Recording) {
        m_pillLabel->setColor(style::kText);
        m_pillLabel->setPositionX(21.f);
    }
    if (mode != Mode::Playing) resetPlayback();
}

void Overlay::update(float dt) {
    auto& lab = Lab::get();
    auto mode = lab.mode();

    if (mode == Mode::Recording) {
        m_blink += dt;
        m_pillDot->setVisible(std::fmod(m_blink, 1.f) < 0.6f);
        int secs = lab.tick() / kTicksPerSecond;
        m_pillLabel->setString(fmt::format("REC  {}:{:02}", secs / 60, secs % 60).c_str());
    }
    else if (mode == Mode::Scanning) {
        float progress = lab.scanProgress();
        m_scanTitle->setString(fmt::format("Scanning  {}%", static_cast<int>(progress * 100.f)).c_str());
        m_scanStatus->setString(lab.scanStatus().c_str());
        m_scanFill->setContentSize({m_barWidth * progress, 6.f});
    }
}

// ---- playback ------------------------------------------------------------

void Overlay::resetPlayback() {
    if (m_circles) m_circles->removeAllChildren();
    std::fill(m_counts.begin(), m_counts.end(), 0);
    refreshCounter();
}

void Overlay::addCircle(Input const& in, CCPoint where, ccColor3B color, std::string const& text, double ms) {
    auto const& s = Lab::get().settings();
    float scale = static_cast<float>(s.circleScale);
    float radius = 7.5f * scale;

    auto node = CCNode::create();
    node->setPosition(where);

    auto ring = CCDrawNode::create();
    // A hollow polygon: drawPolygon is plain cocos2d, available on every platform.
    constexpr int kSegments = 36;
    CCPoint verts[kSegments];
    for (int i = 0; i < kSegments; i++) {
        float a = static_cast<float>(i) / kSegments * 6.2831853f;
        verts[i] = CCPoint(std::cos(a) * radius, std::sin(a) * radius);
    }
    ring->drawPolygon(verts, kSegments, {0.f, 0.f, 0.f, 0.f}, 0.8f * scale, toColor4F(color));
    if (in.down) ring->drawDot({0.f, 0.f}, 1.8f * scale, toColor4F(color, 0.9f));
    node->addChild(ring);

    float textScale = 0.42f * static_cast<float>(s.numberScale) * scale;
    auto number = style::label(text, style::kTitleFont, textScale, color);
    if (s.numberBeside) {
        // Press on the left, release on the right, as in frame window videos.
        number->setAnchorPoint({in.down ? 1.f : 0.f, 0.5f});
        number->setPosition({(in.down ? -1.f : 1.f) * (radius + 4.f), 0.f});
    }
    else {
        number->setAnchorPoint({0.5f, 0.f});
        number->setPosition({0.f, radius + 3.f});
    }
    node->addChild(number);

    if (s.showMilliseconds && ms > 0.0) {
        auto msLabel = style::label(fmt::format("{:.1f}ms", ms), style::kBodyFont, 0.45f * scale, style::kText);
        if (s.numberBeside) {
            msLabel->setAnchorPoint(number->getAnchorPoint());
            msLabel->setPosition(number->getPosition() - CCPoint{0.f, 9.f * scale});
        }
        else {
            // Straight under the ring, clear of the number above it.
            msLabel->setAnchorPoint({0.5f, 1.f});
            msLabel->setPosition({0.f, -radius - 2.f});
        }
        node->addChild(msLabel);
    }

    m_circles->addChild(node);
}

void Overlay::onPlaybackInput(size_t index) {
    auto& lab = Lab::get();
    auto rec = lab.recording();
    if (!rec || index >= rec->inputs.size() || index >= rec->windows.size()) return;

    auto const& s = lab.settings();
    auto const& in = rec->inputs[index];
    auto const& w = rec->windows[index];
    if (!w.measured || in.button != 1) return;
    if (!in.down && !s.showReleases) return;
    if (in.mode == Gamemode::Ship && !s.countShip) return;

    bool twoPlayer = m_layer->m_levelSettings && m_layer->m_levelSettings->m_twoPlayerMode;
    PlayerObject* player = (!in.player1 && twoPlayer) ? m_layer->m_player2 : m_layer->m_player1;
    CCPoint where{in.x, in.y};
    if (player && player->getParent() && m_layer->m_objectLayer) {
        where = m_layer->m_objectLayer->convertToNodeSpace(
            player->getParent()->convertToWorldSpace(player->getPosition()));
    }

    if (w.unknown) {
        if (s.circles) addCircle(in, where, style::kMuted, "?", 0.0);
        if (s.sounds) sounds::play(0, false, true);
        return;
    }
    if (!s.counts(in, w)) return;

    int frames = s.framesFor(w);
    ccColor3B color = style::kMuted;
    for (size_t r = 0; r < s.rows.size(); r++) {
        if (s.rows[r].contains(frames)) {
            color = s.rows[r].color;
            if (r < m_counts.size() && (!w.open() || s.countOpenWindows)) m_counts[r]++;
            break;
        }
    }
    refreshCounter();

    if (frames > s.maxCircleFrames) return;
    if (s.circles) {
        double ms = w.ticks() * 1000.0 / kTicksPerSecond;
        addCircle(in, where, color, fmt::format("{}{}", frames, w.open() ? "+" : ""), ms);
    }
    if (s.sounds) sounds::play(frames, !in.down, false);
}

} // namespace fwl
