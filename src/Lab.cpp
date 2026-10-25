#include "Lab.hpp"
#include "Overlay.hpp"
#include "Sounds.hpp"
#include "ScanMath.hpp"

#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>
#include <Geode/binding/PauseLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/ui/Notification.hpp>

#include <algorithm>
#include <cctype>

using namespace geode::prelude;

namespace fwl {

namespace {
    constexpr float kStep = 1.f / 240.f;
    // Ticks simulated per unit of scan work before the time budget is checked.
    constexpr int kTicksPerWork = 48;
    // Extra ticks past the recorded end before a test counts as finished.
    constexpr int kEndSlack = 60;

    Gamemode gamemodeOf(PlayerObject* player) {
        if (!player) return Gamemode::Cube;
        if (player->m_isShip) return Gamemode::Ship;
        if (player->m_isBird) return Gamemode::Ufo;
        if (player->m_isBall) return Gamemode::Ball;
        if (player->m_isDart) return Gamemode::Wave;
        if (player->m_isRobot) return Gamemode::Robot;
        if (player->m_isSpider) return Gamemode::Spider;
        if (player->m_isSwing) return Gamemode::Swing;
        return Gamemode::Cube;
    }

    void notify(std::string const& text, NotificationIcon icon = NotificationIcon::Info) {
        Notification::create(text, icon, 3.f)->show();
    }
template <class Map>
static void copyMap(Map& destination, Map const& source) {
    destination.clear();
    for (auto const& entry : source) {
        destination.insert(entry);
    }
}
    void closePauseMenu() {
        if (auto scene = CCScene::get()) {
            if (auto pause = scene->getChildByType<PauseLayer>(0)) {
                pause->onResume(nullptr);
            }
        }
    }
}

// ---- player extras ----------------------------------------------------
PlayerExtras& PlayerExtras::operator=(PlayerExtras const& other) {
    if (this == &other) return *this;

    valid = other.valid;
    jumpBuffered = other.jumpBuffered;
    stateJumpBuffered = other.stateJumpBuffered;
    stateRingJump = other.stateRingJump;
    stateRingJump2 = other.stateRingJump2;
    touchedRing = other.touchedRing;
    touchedPad = other.touchedPad;
    ringJumpRelated = other.ringJumpRelated;
    padRingRelated = other.padRingRelated;
    holdingLeft = other.holdingLeft;
    holdingRight = other.holdingRight;

    copyMap(holdingButtons, other.holdingButtons);
    copyMap(jumpPadRelated, other.jumpPadRelated);

    touchedRings.clear();
    for (auto const& item : other.touchedRings)
        touchedRings.insert(item);

    ringRelatedSet.clear();
    for (auto const& item : other.ringRelatedSet)
        ringRelatedSet.insert(item);

    return *this;
}
void PlayerExtras::save(PlayerObject* p) {
    valid = p != nullptr;
    if (!p) return;
    jumpBuffered = p->m_jumpBuffered;
    stateJumpBuffered = p->m_stateJumpBuffered;
    stateRingJump = p->m_stateRingJump;
    stateRingJump2 = p->m_stateRingJump2;
    touchedRing = p->m_touchedRing;
    touchedPad = p->m_touchedPad;
    ringJumpRelated = p->m_ringJumpRelated;
    padRingRelated = p->m_padRingRelated;
    holdingLeft = p->m_holdingLeft;
    holdingRight = p->m_holdingRight;
    copyMap(holdingButtons, p->m_holdingButtons);
    copyMap(jumpPadRelated, p->m_jumpPadRelated);
    touchedRings = p->m_touchedRings;
    ringRelatedSet = p->m_ringRelatedSet;
}

void PlayerExtras::restore(PlayerObject* p) const {
    if (!valid || !p) return;
    p->m_jumpBuffered = jumpBuffered;
    p->m_stateJumpBuffered = stateJumpBuffered;
    p->m_stateRingJump = stateRingJump;
    p->m_stateRingJump2 = stateRingJump2;
    p->m_touchedRing = touchedRing;
    p->m_touchedPad = touchedPad;
    p->m_ringJumpRelated = ringJumpRelated;
    p->m_padRingRelated = padRingRelated;
    p->m_holdingLeft = holdingLeft;
    p->m_holdingRight = holdingRight;
    copyMap(p->m_holdingButtons, holdingButtons);
    copyMap(p->m_jumpPadRelated, jumpPadRelated);
    p->m_touchedRings = touchedRings;
    p->m_ringRelatedSet = ringRelatedSet;
}

// ---- basics ----------------------------------------------------------------

Lab& Lab::get() {
    static Lab lab;
    return lab;
}

void Lab::reloadSettings() {
    m_settings = Settings::load();
    sounds::clearCache();
}

std::string Lab::makeLevelKey(GJGameLevel* level) const {
    if (!level) return "unknown";
    int id = level->m_levelID.value();
    if (level->m_levelType == GJLevelType::Main) return fmt::format("main-{}", id);
    if (id > 0) return fmt::format("online-{}", id);

    // Local levels have no ID: use the name plus a hash of the level data, so
    // editing the level starts a fresh recording.
    std::string name = level->m_levelName;
    std::string clean;
    for (char c : name) {
        clean += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
    }
    if (clean.empty()) clean = "untitled";
    auto hash = std::hash<std::string>{}(std::string(level->m_levelString));
    return fmt::format("local-{}-{:08x}", clean, static_cast<uint32_t>(hash));
}

bool Lab::streamKey(Input const& in) const {
    // Outside two-player mode both sides of the keyboard drive the same
    // icon, so treat every jump input as one stream.
    return m_scan.twoPlayer ? in.player1 : true;
}

void Lab::ownReset() {
    if (!m_layer) return;
    m_scan.ownReset = true;
    m_layer->resetLevelFromStart();
    m_scan.ownReset = false;
}

// ---- level lifecycle ---------------------------------------------------

void Lab::attach(PlayLayer* layer) {
    setMuted(false);
    m_testModeSet = false;
    m_layer = layer;
    m_mode = Mode::Idle;
    m_tick = 0;
    m_lastInjectedTick = -1;
    m_source = nullptr;
    m_scan = ScanState{};
    m_held.clear();
    reloadSettings();
    m_levelKey = makeLevelKey(layer->m_level);
    m_recording = loadRecording(m_levelKey);
    if (m_recording) m_recording->levelName = std::string(layer->m_level->m_levelName);
}

void Lab::detach(PlayLayer* layer) {
    if (!layer || layer != m_layer) return;
    if (m_mode == Mode::Scanning && m_recording) {
        // Keep whatever was measured so far.
        (void)saveRecording(*m_recording);
    }
    m_scan = ScanState{};
    setMuted(false);
    m_testModeSet = false;
    m_mode = Mode::Idle;
    m_layer = nullptr;
    m_source = nullptr;
    m_recording.reset();
}

void Lab::setMode(Mode mode) {
    m_mode = mode;
    m_accumulator = 0.0;
    m_source = nullptr;
    m_held.clear();
    if (mode == Mode::Playing && m_recording) m_source = &m_recording->inputs;
    if (mode != Mode::Scanning) setMuted(false);
    // Replays and scans run as test runs, so they never save progress.
    setTestMode(mode == Mode::Playing || mode == Mode::Scanning);
    if (auto overlay = Overlay::get()) overlay->onModeChanged();
}

// ---- commands ----------------------------------------------------------

Result<> Lab::startRecording() {
    if (!m_layer) return Err("Open a level first.");
    if (m_layer->m_levelSettings && m_layer->m_levelSettings->m_platformerMode) {
        return Err("Platformer levels aren't supported yet.");
    }
    if (m_mode == Mode::Scanning) return Err("A scan is running. Cancel it first.");

    reloadSettings();
    if (m_layer->m_isPracticeMode) m_layer->togglePracticeMode(false);
    m_live = Recording{};
    m_live.levelKey = m_levelKey;
    m_live.levelName = std::string(m_layer->m_level->m_levelName);
    setMode(Mode::Recording);
    closePauseMenu();
    m_layer->resetLevelFromStart();
    return Ok();
}

Result<> Lab::startPlayback() {
    if (!m_layer) return Err("Open a level first.");
    if (!m_recording || m_recording->inputs.empty()) return Err("Record a run first.");
    if (m_mode == Mode::Scanning) return Err("A scan is running. Cancel it first.");

    reloadSettings();
    if (m_layer->m_isPracticeMode) m_layer->togglePracticeMode(false);
    setMode(Mode::Playing);
    closePauseMenu();
    m_layer->resetLevelFromStart();
    return Ok();
}

Result<> Lab::startScan() {
    if (!m_layer) return Err("Open a level first.");
    if (!m_recording || m_recording->inputs.empty()) return Err("Record a run first.");
    if (m_mode == Mode::Scanning) return Err("A scan is already running.");

    reloadSettings();
    if (m_layer->m_isPracticeMode) m_layer->togglePracticeMode(false);
    m_scan = ScanState{};
    m_scan.startedAt = std::chrono::steady_clock::now();
    setMode(Mode::Scanning);
    setMuted(true);
    closePauseMenu();
    return Ok();
}

void Lab::stop() {
    switch (m_mode) {
        case Mode::Scanning:
            scanFinish(true);
            break;
        case Mode::Recording:
            m_live = Recording{};
            setMode(Mode::Idle);
            break;
        case Mode::Playing:
            setMode(Mode::Idle);
            if (auto overlay = Overlay::get()) overlay->resetPlayback();
            break;
        case Mode::Idle:
            break;
    }
}

void Lab::deleteCurrent() {
    stop();
    deleteRecording(m_levelKey);
    m_recording.reset();
    if (auto overlay = Overlay::get()) overlay->onModeChanged();
}

// ---- per-step hooks ----------------------------------------------------

void Lab::injectFor(GJBaseGameLayer* layer, int tick) {
    if (!m_source || tick <= m_lastInjectedTick) return;
    m_lastInjectedTick = tick;

    auto begin = m_source->begin();
    auto it = std::lower_bound(begin, m_source->end(), tick, [](Input const& in, int t) {
        return in.tick < t;
    });
    bool fromRecording = m_recording && m_source == &m_recording->inputs;
    for (; it != m_source->end() && it->tick == tick; ++it) {
        if (m_mode == Mode::Playing && fromRecording) {
            size_t index = static_cast<size_t>(it - begin);
            if (auto overlay = Overlay::get()) overlay->onPlaybackInput(index);
        }
        m_injecting = true;
        layer->handleButton(it->down, it->button, it->player1);
        m_injecting = false;
    }
}

void Lab::beforeStep(GJBaseGameLayer* layer) {
    if (m_mode == Mode::Scanning && m_scan.phase == ScanState::Phase::Advance &&
        !m_scan.snapshot && !m_scan.snapshotFailed && m_scan.snapshotAt == m_tick &&
        !m_died && !m_reachedEnd) {
        // Snapshot right at the start of the tick, before its inputs, even
        // when one update runs several ticks (time warp).
        scanTakeSnapshot();
    }
    if (m_mode == Mode::Playing || m_mode == Mode::Scanning) {
        injectFor(layer, m_tick);
    }
}

void Lab::afterStep(GJBaseGameLayer*) {
    m_tick++;
}

void Lab::onRealInput(GJBaseGameLayer*, bool down, int button, bool player1) {
    if (m_mode != Mode::Recording || !m_layer) return;
    bool twoPlayer = m_layer->m_levelSettings && m_layer->m_levelSettings->m_twoPlayerMode;
    PlayerObject* player = (!player1 && twoPlayer) ? m_layer->m_player2 : m_layer->m_player1;

    Input in;
    in.tick = m_tick;
    in.down = down;
    in.button = button;
    in.player1 = player1;
    in.mode = gamemodeOf(player);
    if (player) {
        in.x = player->getPositionX();
        in.y = player->getPositionY();
    }
    m_live.inputs.push_back(in);

    if (down) m_held.insert({player1, button});
    else m_held.erase({player1, button});
}

void Lab::onPause() {
    // The game lets go of held buttons when it pauses without telling the
    // input hook, so record those releases ourselves.
    if (m_mode != Mode::Recording || !m_layer) return;
    auto held = m_held;
    for (auto const& [player1, button] : held) {
        m_layer->handleButton(false, button, player1);
    }
}

void Lab::onResetBefore() {
    if (m_layer && m_layer->m_isPracticeMode &&
        (m_mode == Mode::Recording || m_mode == Mode::Playing)) {
        notify("Practice mode stops Frame Window Lab.", NotificationIcon::Warning);
        stop();
    }
    if (m_mode == Mode::Scanning && !m_scan.ownReset) {
        // Restart button, practice toggle...: handled on the next scan frame,
        // not inside the game's reset.
        m_scan.externalReset = true;
    }
    m_tick = 0;
    m_lastInjectedTick = -1;
    m_accumulator = 0.0;
    m_died = false;
    m_reachedEnd = false;
    m_stalled = false;
    m_held.clear();
    if (m_mode == Mode::Recording) m_live.inputs.clear();
    if (m_mode == Mode::Playing) {
        if (auto overlay = Overlay::get()) overlay->resetPlayback();
    }
}

void Lab::onResetAfter() {
    // Inputs the game re-applies during the reset (a held button) were
    // stamped at tick 0 already; make sure the first step is tick 0 too.
    m_tick = 0;
    m_lastInjectedTick = -1;
}

bool Lab::onDeath(PlayerObject* player, GameObject* object) {
    if (!m_layer || object == m_layer->m_anticheatSpike) return false;
    if (player != m_layer->m_player1 && player != m_layer->m_player2) return false;
    if (m_mode == Mode::Scanning) {
        m_died = true;
        return true;
    }
    return false;
}

bool Lab::onLevelEnd() {
    switch (m_mode) {
        case Mode::Scanning:
            m_reachedEnd = true;
            return true;
        case Mode::Recording:
            finishRecording();
            return false;
        default:
            return false;
    }
}

void Lab::finishRecording() {
    m_live.endTick = m_tick;
    m_live.levelKey = m_levelKey;
    m_live.resetResults();
    m_recording = std::move(m_live);
    m_live = Recording{};

    if (auto res = saveRecording(*m_recording); !res) {
        log::error("Saving the recording failed: {}", res.unwrapErr());
        notify("Recorded, but saving failed. See the log.", NotificationIcon::Error);
    }
    else {
        notify(fmt::format("Recorded {} inputs. Pause and open Frame Window Lab to scan.",
            m_recording->inputs.size()), NotificationIcon::Success);
    }
    setMode(Mode::Idle);
}

bool Lab::onFrame(float dt) {
    if (!m_layer) return false;

    if (m_mode == Mode::Scanning) {
        scanFrame();
        return true;
    }

    if (m_mode == Mode::Recording || m_mode == Mode::Playing) {
        // Feed the game whole 1/240 s updates, as it gets at 240 FPS, so
        // recording, playback and scans all see the same physics steps.
        m_accumulator = std::min(m_accumulator + static_cast<double>(dt), 0.25);
        m_manualStep = true;
        int steps = 0;
        while (m_accumulator >= kStep - 1e-7 && steps < 64 && m_layer) {
            m_layer->GJBaseGameLayer::update(kStep);
            m_accumulator -= kStep;
            steps++;
        }
        m_manualStep = false;
        return true;
    }
    return false;
}

void Lab::stepOnce() {
    int before = m_tick;
    m_manualStep = true;
    for (int i = 0; i < 64 && m_tick == before && m_layer; i++) {
        m_layer->GJBaseGameLayer::update(kStep);
    }
    m_manualStep = false;
    if (m_tick == before) m_stalled = true;
}

void Lab::setTestMode(bool on) {
    if (!m_layer || on == m_testModeSet) return;
    if (on) {
        m_savedTestMode = m_layer->m_isTestMode;
        m_layer->m_isTestMode = true;
    }
    else {
        m_layer->m_isTestMode = m_savedTestMode;
    }
    m_testModeSet = on;
    m_layer->updateTestModeLabel();
}

void Lab::setMuted(bool muted) {
    if (m_muted == muted) return;
    auto engine = FMODAudioEngine::sharedEngine();
    if (!engine || !engine->m_system) return;
    FMOD::ChannelGroup* master = nullptr;
    if (engine->m_system->getMasterChannelGroup(&master) == FMOD_OK && master) {
        master->setMute(muted);
        m_muted = muted;
    }
}

// ---- summary -----------------------------------------------------------

std::vector<int> Lab::summaryCounts() const {
    std::vector<int> counts(m_settings.rows.size(), 0);
    if (!m_recording) return counts;
    auto const& rec = *m_recording;
    for (size_t i = 0; i < rec.inputs.size() && i < rec.windows.size(); i++) {
        auto const& w = rec.windows[i];
        if (!m_settings.counts(rec.inputs[i], w)) continue;
        if (w.open() && !m_settings.countOpenWindows) continue;
        int frames = m_settings.framesFor(w);
        for (size_t r = 0; r < m_settings.rows.size(); r++) {
            if (m_settings.rows[r].contains(frames)) {
                counts[r]++;
                break;
            }
        }
    }
    return counts;
}

float Lab::scanProgress() const {
    if (m_mode != Mode::Scanning || m_scan.total == 0) return 0.f;
    return static_cast<float>(m_scan.measuredCount) / static_cast<float>(m_scan.total);
}

std::string Lab::scanStatus() const {
    if (m_mode != Mode::Scanning) return "";
    if (m_scan.phase == ScanState::Phase::Begin) return "Preparing...";
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_scan.startedAt).count();
    return fmt::format("Input {} of {}  ·  {}:{:02}",
        std::min(m_scan.measuredCount + 1, m_scan.total), m_scan.total,
        secs / 60, secs % 60);
}

// ---- scanning ----------------------------------------------------------
//
// For every jump input, in order of where its search starts:
//  1. play the recording up to `lo`, the earliest tick the input may move to,
//     and save the game state there (a checkpoint plus a few player fields);
//  2. Verify: from the checkpoint, the unchanged recording must survive
//     (catches checkpoint drift; on failure the target is redone once from
//     the start of the level, which is exact);
//  3. Earlier: move the input 1, 2, 3... ticks earlier until a test dies;
//  4. Later: the same, later;
//  5. Remove: take the press (and its release) out; if the run still lives
//     the press does nothing useful;
//  6. Reverify: the unchanged recording again, so leftovers from the tests
//     can't go unnoticed.
// Completion is required by default. Short checks are optional, but removing
// a press always requires completion before it can be labelled unnecessary.

void Lab::scanFrame() {
    if (!m_layer || m_layer->m_isPaused) return;
    auto start = std::chrono::steady_clock::now();
    auto budget = std::chrono::milliseconds(std::clamp(m_settings.scanBudgetMs, 4, 100));
    while (m_mode == Mode::Scanning && std::chrono::steady_clock::now() - start < budget) {
        scanWork();
    }
}

void Lab::scanWork() {
    if (m_layer->m_isPracticeMode) {
        m_scan.error = "Practice mode stops scans.";
        scanFinish(true);
        return;
    }
    if (m_scan.externalReset) {
        m_scan.externalReset = false;
        if (m_scan.phase != ScanState::Phase::Begin && m_scan.phase != ScanState::Phase::Finish) {
            // The level was restarted from outside: redo the current input.
            m_scan.snapshot = nullptr;
            m_scan.testRunning = false;
            m_scan.cleanBaseline = true;
            m_source = &m_recording->inputs;
            m_scan.phase = ScanState::Phase::Advance;
        }
    }

    switch (m_scan.phase) {
        case ScanState::Phase::Begin: scanBegin(); break;
        case ScanState::Phase::Advance: scanAdvance(); break;
        case ScanState::Phase::Testing: scanRunTest(); break;
        case ScanState::Phase::Finish: scanFinish(false); break;
    }
}

void Lab::scanBegin() {
    auto& rec = *m_recording;
    size_t n = rec.inputs.size();

    // Carry on from a partial scan; start over after a complete one.
    auto signature = m_settings.scanSignature();
    bool resume = m_settings.resumeScan && !rec.scanComplete &&
        rec.scanSignature == signature && rec.windows.size() == n && rec.hasAnyResults();
    if (!resume) rec.resetResults();
    rec.scanSignature = signature;

    m_scan.twoPlayer = m_layer->m_levelSettings && m_layer->m_levelSettings->m_twoPlayerMode;
    m_scan.range = std::max(1, m_settings.scanRange);
    m_scan.minOffset.assign(n, 0);
    m_scan.maxOffset.assign(n, 0);
    m_scan.order.clear();

    for (size_t i = 0; i < n; i++) {
        auto const& in = rec.inputs[i];
        if (in.button != 1 || rec.windows[i].measured) continue;

        // Keep the input strictly between its neighbours on the same
        // button stream, so a press never jumps past its own release.
        int prevTick = -1;
        int nextTick = rec.endTick + 1;
        for (size_t j = i; j-- > 0;) {
            auto const& o = rec.inputs[j];
            if (o.button == in.button && streamKey(o) == streamKey(in)) {
                prevTick = o.tick;
                break;
            }
        }
        for (size_t j = i + 1; j < n; j++) {
            auto const& o = rec.inputs[j];
            if (o.button == in.button && streamKey(o) == streamKey(in)) {
                nextTick = o.tick;
                break;
            }
        }
        m_scan.minOffset[i] = std::min(0, std::max(-m_scan.range, prevTick + 1 - in.tick));
        m_scan.maxOffset[i] = std::max(0, std::min(m_scan.range, nextTick - 1 - in.tick));
        m_scan.order.push_back(i);
    }

    std::stable_sort(m_scan.order.begin(), m_scan.order.end(), [&](size_t a, size_t b) {
        return rec.inputs[a].tick + m_scan.minOffset[a] < rec.inputs[b].tick + m_scan.minOffset[b];
    });

    m_scan.total = m_scan.order.size();
    m_scan.cursor = 0;
    m_source = &rec.inputs;
    scanResyncFromStart();
    m_scan.phase = ScanState::Phase::Advance;
}

void Lab::scanResyncFromStart() {
    m_scan.snapshot = nullptr;
    m_scan.snapshotFailed = false;
    m_scan.snapshotAt = -1;
    m_source = &m_recording->inputs;
    ownReset();
    m_scan.cleanBaseline = true;
    m_died = false;
    m_reachedEnd = false;
    m_stalled = false;
}

void Lab::scanBaselineFailed() {
    if (m_scan.cleanBaseline) {
        // Even a replay from the very start doesn't match the recording.
        m_scan.error = "The recording doesn't replay the same way, so the scan stopped. "
            "Turn off mods that change physics or input timing and record again.";
        scanFinish(true);
        return;
    }
    // The baseline drifted after checkpoint loads: replay from the start.
    scanResyncFromStart();
}

void Lab::scanTakeSnapshot() {
    m_scan.snapshot = m_layer->createCheckpoint();
    m_scan.snapshotFailed = !m_scan.snapshot;
    m_scan.snapshotTick = m_tick;
    m_scan.extras1.save(m_layer->m_player1);
    m_scan.extras2.save(m_layer->m_player2);
}

void Lab::scanAdvance() {
    if (m_scan.cursor >= m_scan.order.size()) {
        m_scan.phase = ScanState::Phase::Finish;
        return;
    }
    if (m_settings.exactScan || m_scan.retried) {
        scanStartTarget();
        return;
    }

    size_t index = m_scan.order[m_scan.cursor];
    int lo = m_recording->inputs[index].tick + m_scan.minOffset[index];
    m_source = &m_recording->inputs;
    m_scan.snapshotAt = lo;

    auto hasStart = [&] { return m_scan.snapshot || m_scan.snapshotFailed; };
    if (m_tick > lo && !hasStart()) {
        // Already past it (shouldn't happen with sorted targets): start over.
        if (++m_scan.overshoots > 2) {
            m_scan.error = "The scan lost its place in the recording and stopped.";
            scanFinish(true);
            return;
        }
        scanResyncFromStart();
        return;
    }
    for (int i = 0; i < kTicksPerWork && !hasStart(); i++) {
        if (m_tick == lo) {
            scanTakeSnapshot();
            break;
        }
        stepOnce();
        if (m_died || m_reachedEnd || m_stalled) {
            scanBaselineFailed();
            return;
        }
    }
    if (hasStart()) {
        // With no checkpoint from the game, this input is tested exactly:
        // scanRestoreSnapshot resets the level when there is no snapshot.
        m_scan.snapshotAt = -1;
        scanStartTarget();
    }
}

void Lab::scanRestoreSnapshot(std::vector<Input> const* source) {
    if (m_settings.exactScan || m_scan.retried || !m_scan.snapshot) {
        ownReset();
        m_scan.cleanBaseline = true;
    }
    else {
        m_scan.ownReset = true;
        m_layer->loadFromCheckpoint(m_scan.snapshot);
        m_scan.ownReset = false;
        m_scan.extras1.restore(m_layer->m_player1);
        m_scan.extras2.restore(m_layer->m_player2);
        m_tick = m_scan.snapshotTick;
        m_lastInjectedTick = m_scan.snapshotTick - 1;
        m_layer->m_levelEndAnimationStarted = false;
        m_scan.cleanBaseline = false;
    }
    m_source = source;
    m_died = false;
    m_reachedEnd = false;
    m_stalled = false;
}

void Lab::scanStartTarget() {
    m_scan.result = Window{};
    m_scan.test = ScanState::Test::Verify;
    m_scan.offset = 0;
    m_scan.testRunning = false;
    m_scan.phase = ScanState::Phase::Testing;
}

int Lab::scanTestEnd(size_t index, int movedTick) const {
    auto const& rec = *m_recording;
    // Removing an input may cause a much later death. Only a completed run
    // can prove that a press is unnecessary, even in the faster scan mode.
    if (m_settings.checkToEnd || m_scan.test == ScanState::Test::Remove) {
        return rec.endTick + kEndSlack;
    }
    size_t k = static_cast<size_t>(std::max(0, m_settings.followingInputs));
    int kthTick = index + k < rec.inputs.size() ? rec.inputs[index + k].tick : rec.endTick;
    return scanmath::endTick(movedTick, kthTick, rec.endTick, m_settings.checkTicks);
}

void Lab::scanRetryOrUnknown() {
    // A failed check from a checkpoint may just be checkpoint drift: redo
    // the input once from an exact replay before giving up on it.
    if (scanmath::retryExactly(m_settings.exactScan, m_scan.retried)) {
        m_scan.retried = true;
        m_scan.testRunning = false;
        scanResyncFromStart();
        m_scan.phase = ScanState::Phase::Advance;
        return;
    }
    m_scan.result = Window{};
    m_scan.result.unknown = true;
    scanFinishTarget();
}

void Lab::scanRunTest() {
    auto& rec = *m_recording;
    size_t index = m_scan.order[m_scan.cursor];
    auto const& target = rec.inputs[index];
    int tick = target.tick;

    if (!m_scan.testRunning) {
        // Build the inputs for this test.
        std::vector<Input> const* source = &rec.inputs;
        int moved = tick;
        if (m_scan.test == ScanState::Test::Earlier || m_scan.test == ScanState::Test::Later) {
            moved = tick + m_scan.offset;
            m_scan.testInputs = rec.inputs;
            m_scan.testInputs[index].tick = moved;
            std::stable_sort(m_scan.testInputs.begin(), m_scan.testInputs.end(),
                [](Input const& a, Input const& b) { return a.tick < b.tick; });
            source = &m_scan.testInputs;
        }
        else if (m_scan.test == ScanState::Test::Remove) {
            size_t pair = rec.inputs.size();
            for (size_t j = index + 1; j < rec.inputs.size(); j++) {
                auto const& o = rec.inputs[j];
                if (o.button == target.button && streamKey(o) == streamKey(target)) {
                    if (!o.down) pair = j;
                    break;
                }
            }
            m_scan.testInputs.clear();
            m_scan.testInputs.reserve(rec.inputs.size());
            for (size_t j = 0; j < rec.inputs.size(); j++) {
                if (j != index && j != pair) m_scan.testInputs.push_back(rec.inputs[j]);
            }
            source = &m_scan.testInputs;
        }
        scanRestoreSnapshot(source);
        m_scan.testEndTick = scanTestEnd(index, moved);
        m_scan.testRunning = true;
    }

    bool done = false;
    bool passed = false;
    bool requireCompletion = m_settings.checkToEnd || m_scan.test == ScanState::Test::Remove;
    for (int i = 0; i < kTicksPerWork; i++) {
        stepOnce();
        bool timedOut = m_tick >= m_scan.testEndTick;
        if (m_died || m_stalled || m_reachedEnd || timedOut) {
            done = true;
            passed = scanmath::passed(m_died, m_stalled, m_reachedEnd, timedOut, requireCompletion);
            break;
        }
    }
    if (!done) return;

    bool stalled = m_stalled;
    m_scan.testRunning = false;
    auto& r = m_scan.result;
    int minOff = m_scan.minOffset[index];
    int maxOff = m_scan.maxOffset[index];

    auto goLater = [&] {
        if (maxOff > 0) {
            m_scan.test = ScanState::Test::Later;
            m_scan.offset = 1;
        }
        else {
            scanNextTest();
        }
    };

    switch (m_scan.test) {
        case ScanState::Test::Verify:
            if (!passed) {
                scanRetryOrUnknown();
                return;
            }
            if (minOff < 0) {
                m_scan.test = ScanState::Test::Earlier;
                m_scan.offset = -1;
            }
            else {
                goLater();
            }
            return;

        case ScanState::Test::Earlier:
            if (stalled) { scanRetryOrUnknown(); return; }
            if (passed) {
                r.left = -m_scan.offset;
                if (m_scan.offset <= minOff) {
                    // Only a limit set by the scan range leaves the window open;
                    // a neighbouring input is a real edge.
                    r.openLeft = minOff == -m_scan.range;
                    goLater();
                }
                else {
                    m_scan.offset--;
                }
            }
            else {
                goLater();
            }
            return;

        case ScanState::Test::Later:
            if (stalled) { scanRetryOrUnknown(); return; }
            if (passed) {
                r.right = m_scan.offset;
                if (m_scan.offset >= maxOff) {
                    r.openRight = maxOff == m_scan.range;
                    scanNextTest();
                }
                else {
                    m_scan.offset++;
                }
            }
            else {
                scanNextTest();
            }
            return;

        case ScanState::Test::Remove:
            if (stalled) { scanRetryOrUnknown(); return; }
            r.useless = passed;
            m_scan.test = ScanState::Test::Reverify;
            m_scan.offset = 0;
            return;

        case ScanState::Test::Reverify:
            if (!passed) {
                scanRetryOrUnknown();
                return;
            }
            scanFinishTarget();
            return;
    }
}

void Lab::scanNextTest() {
    size_t index = m_scan.order[m_scan.cursor];
    bool anyMoved = m_scan.minOffset[index] < 0 || m_scan.maxOffset[index] > 0;
    if (m_settings.skipUseless && m_recording->inputs[index].down) {
        m_scan.test = ScanState::Test::Remove;
    }
    else if (anyMoved) {
        m_scan.test = ScanState::Test::Reverify;
    }
    else {
        scanFinishTarget();
        return;
    }
    m_scan.offset = 0;
}

void Lab::scanFinishTarget() {
    auto& rec = *m_recording;
    size_t index = m_scan.order[m_scan.cursor];
    // A release belonging to a removed press is unnecessary too. The
    // press is measured first because its search starts before the release.
    if (!rec.inputs[index].down) {
        for (size_t j = index; j-- > 0;) {
            if (rec.inputs[j].button == rec.inputs[index].button &&
                streamKey(rec.inputs[j]) == streamKey(rec.inputs[index])) {
                if (rec.inputs[j].down && rec.windows[j].useless) m_scan.result.useless = true;
                break;
            }
        }
    }
    m_scan.result.measured = true;
    rec.windows[index] = m_scan.result;
    m_scan.measuredCount++;
    m_scan.cursor++;
    m_scan.retried = false;
    m_scan.overshoots = 0;
    m_scan.testRunning = false;

    if (m_scan.cursor >= m_scan.order.size()) {
        m_scan.phase = ScanState::Phase::Finish;
        return;
    }

    // Save partial results now and then, so a crash loses little.
    if (m_scan.cursor % 25 == 0) (void)saveRecording(rec);

    if (m_settings.exactScan) {
        scanStartTarget();
        return;
    }

    size_t next = m_scan.order[m_scan.cursor];
    int nextLo = rec.inputs[next].tick + m_scan.minOffset[next];
    if (m_scan.snapshot && nextLo == m_scan.snapshotTick) {
        // Same starting tick: reuse the checkpoint as it is.
        scanStartTarget();
        return;
    }

    // Go back to the checkpoint (or the level start, if there was none) and
    // carry on with the unchanged recording.
    scanRestoreSnapshot(&rec.inputs);
    m_scan.snapshot = nullptr;
    m_scan.snapshotFailed = false;
    m_scan.phase = ScanState::Phase::Advance;
}

void Lab::scanFinish(bool cancelled) {
    if (m_mode != Mode::Scanning) return;
    m_scan.snapshot = nullptr;
    m_source = nullptr;

    bool complete = !cancelled && m_scan.cursor >= m_scan.order.size();
    size_t unknown = 0;
    if (m_recording) {
        m_recording->scanComplete = complete;
        for (auto const& w : m_recording->windows) {
            if (w.measured && w.unknown) unknown++;
        }
        if (auto res = saveRecording(*m_recording); !res) {
            log::error("Saving scan results failed: {}", res.unwrapErr());
        }
    }

    auto secs = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_scan.startedAt).count();
    size_t measured = m_scan.measuredCount;
    std::string error = m_scan.error;
    m_scan = ScanState{};
    setMode(Mode::Idle);

    if (m_layer) m_layer->resetLevelFromStart();

    if (!error.empty()) {
        notify(error, NotificationIcon::Error);
    }
    else if (cancelled) {
        notify(fmt::format("Scan paused after {} inputs. Scan again to carry on.", measured),
            NotificationIcon::Warning);
    }
    else if (unknown > 0) {
        notify(fmt::format("Scan done in {}:{:02}. {} inputs couldn't be checked; "
            "turn on Exact scan for those.", secs / 60, secs % 60, unknown), NotificationIcon::Warning);
    }
    else {
        notify(fmt::format("Scan done: {} inputs in {}:{:02}. Press Play to watch.",
            measured, secs / 60, secs % 60), NotificationIcon::Success);
    }
}

} // namespace fwl
