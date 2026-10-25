#pragma once

#include "Data.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/CheckpointObject.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

#include <chrono>
#include <set>

namespace fwl {

enum class Mode { Idle, Recording, Playing, Scanning };

// Player fields the game's checkpoints don't bring back: held buttons, the
// jump buffer, and which orbs and pads were already used. Without these a
// test could start with a button still held from the previous test.
struct PlayerExtras {
    // Not an aggregate: some platforms' containers have explicit default
    // constructors, which aggregate initialisation can't call.
    PlayerExtras() {}
PlayerExtras& operator=(PlayerExtras const& other);

    bool valid = false;
    bool jumpBuffered = false;
    unsigned char stateJumpBuffered = 0;
    bool stateRingJump = false;
    bool stateRingJump2 = false;
    bool touchedRing = false;
    bool touchedPad = false;
    bool ringJumpRelated = false;
    bool padRingRelated = false;
    bool holdingLeft = false;
    bool holdingRight = false;
    gd::map<int, bool> holdingButtons;
    gd::map<int, bool> jumpPadRelated;
    gd::unordered_set<int> touchedRings;
    gd::unordered_set<int> ringRelatedSet;

    void save(PlayerObject* player);
    void restore(PlayerObject* player) const;
};

// Scanning measures each input's frame window by replaying the run with
// that one input moved earlier and later, one tick at a time, and checking
// whether the icon still survives.
struct ScanState {
    enum class Phase { Begin, Advance, Testing, Finish };
    enum class Test { Verify, Earlier, Later, Remove, Reverify };

    Phase phase = Phase::Begin;
    std::vector<size_t> order;     // input indices still to measure, by start tick
    std::vector<int> minOffset;    // per input: most negative offset allowed
    std::vector<int> maxOffset;    // per input: most positive offset allowed
    int range = 12;
    bool twoPlayer = false;
    size_t cursor = 0;             // position in `order`

    geode::Ref<CheckpointObject> snapshot;
    int snapshotTick = 0;
    int snapshotAt = -1;           // take the snapshot when this tick starts
    bool snapshotFailed = false;   // the game gave no checkpoint: test this input exactly
    int overshoots = 0;            // times the baseline ran past this input's start
    PlayerExtras extras1;
    PlayerExtras extras2;

    // True while the baseline has only been played from a level reset, with
    // no checkpoint loads since: such a replay is exact.
    bool cleanBaseline = true;
    bool retried = false;          // this target was already redone from the start
    bool ownReset = false;         // the reset in progress is the scan's own
    bool externalReset = false;    // something else reset the level mid-scan

    Test test = Test::Verify;
    int offset = 0;
    bool testRunning = false;
    int testEndTick = 0;
    std::vector<Input> testInputs;
    Window result;

    size_t total = 0;              // inputs this scan measures
    size_t measuredCount = 0;
    std::string error;
    std::chrono::steady_clock::time_point startedAt;
};

class Lab {
public:
    static Lab& get();

    // ---- level lifecycle ----
    void attach(PlayLayer* layer);
    void detach(PlayLayer* layer);
    bool attached(GJBaseGameLayer* layer) const { return layer && layer == m_layer; }
    PlayLayer* layer() const { return m_layer; }

    Mode mode() const { return m_mode; }
    Recording* recording() { return m_recording ? &*m_recording : nullptr; }
    Settings const& settings() const { return m_settings; }
    void reloadSettings();

    // ---- commands from the menu ----
    geode::Result<> startRecording();
    geode::Result<> startPlayback();
    geode::Result<> startScan();
    void stop();
    void deleteCurrent();
    std::string levelKey() const { return m_levelKey; }

    // ---- hooks ----
    int tick() const { return m_tick; }
    bool manualStep() const { return m_manualStep; }
    bool injecting() const { return m_injecting; }

    void beforeStep(GJBaseGameLayer* layer);
    void afterStep(GJBaseGameLayer* layer);
    void onRealInput(GJBaseGameLayer* layer, bool down, int button, bool player1);
    void onPause();
    void onResetBefore();
    void onResetAfter();
    // Returns true when the hook should swallow the death.
    bool onDeath(PlayerObject* player, GameObject* object);
    // Returns true when the hook should swallow the end animation.
    bool onLevelEnd();
    // Frame driver: returns true when it handled the frame itself.
    bool onFrame(float dt);

    // ---- scan progress for the HUD ----
    float scanProgress() const;
    std::string scanStatus() const;

    // Counts per counter row for the whole recording.
    std::vector<int> summaryCounts() const;

private:
    Lab() = default;

    void setMode(Mode mode);
    void injectFor(GJBaseGameLayer* layer, int tick);
    void stepOnce();
    void finishRecording();
    std::string makeLevelKey(GJGameLevel* level) const;
    bool streamKey(Input const& in) const;
    void ownReset();

    // scanning
    void scanFrame();
    void scanWork();
    void scanBegin();
    void scanAdvance();
    void scanTakeSnapshot();
    void scanBaselineFailed();
    void scanResyncFromStart();
    void scanStartTarget();
    void scanRetryOrUnknown();
    void scanNextTest();
    void scanRunTest();
    void scanFinishTarget();
    void scanFinish(bool cancelled);
    void scanRestoreSnapshot(std::vector<Input> const* source);
    int scanTestEnd(size_t inputIndex, int movedTick) const;
    void setMuted(bool muted);
    void setTestMode(bool on);

    PlayLayer* m_layer = nullptr;
    Mode m_mode = Mode::Idle;
    Settings m_settings;
    std::string m_levelKey;
    std::optional<Recording> m_recording;
    Recording m_live;
    std::set<std::pair<bool, int>> m_held;  // buttons held while recording

    int m_tick = 0;
    int m_lastInjectedTick = -1;
    bool m_injecting = false;
    bool m_manualStep = false;
    double m_accumulator = 0.0;
    std::vector<Input> const* m_source = nullptr;

    bool m_died = false;
    bool m_reachedEnd = false;
    bool m_stalled = false;

    ScanState m_scan;
    bool m_muted = false;
    bool m_testModeSet = false;
    bool m_savedTestMode = false;
};

} // namespace fwl
