#pragma once

#include <Geode/Geode.hpp>

#include <climits>
#include <optional>
#include <string>
#include <vector>

namespace fwl {

// Physics ticks per second the game simulates at.
constexpr int kTicksPerSecond = 240;

enum class Gamemode : uint8_t { Cube, Ship, Ball, Ufo, Wave, Robot, Spider, Swing };

char const* gamemodeName(Gamemode mode);

// One button event, stamped with the physics tick it was applied on.
struct Input {
    int tick = 0;
    bool down = true;
    int button = 1;
    bool player1 = true;
    Gamemode mode = Gamemode::Cube;
    float x = 0.f;
    float y = 0.f;
};

// What a scan found out about one input.
struct Window {
    bool measured = false;
    bool unknown = false;   // the run could not be reproduced at this click
    bool useless = false;   // the run survives with the click taken out
    int left = 0;           // ticks earlier that still pass
    int right = 0;          // ticks later that still pass
    bool openLeft = false;  // the search stopped at its limit while still passing
    bool openRight = false;

    int ticks() const { return left + right + 1; }
    bool open() const { return openLeft || openRight; }
};

struct Recording {
    std::string levelKey;
    std::string levelName;
    int endTick = 0;
    std::vector<Input> inputs;
    std::vector<Window> windows;  // same size as inputs once a scan has started
    bool scanComplete = false;
    std::string scanSignature;  // algorithm and scan settings used for these results

    bool hasAnyResults() const;
    void resetResults();
};

std::filesystem::path recordingPath(std::string const& levelKey);
geode::Result<> saveRecording(Recording const& rec);
std::optional<Recording> loadRecording(std::string const& levelKey);
bool deleteRecording(std::string const& levelKey);

// ---- counter rows ------------------------------------------------------

struct CounterRow {
    int from = 1;
    int to = 1;  // INT_MAX for "N+"
    cocos2d::ccColor3B color{255, 255, 255};
    std::string label;

    bool contains(int frames) const { return frames >= from && frames <= to; }
};

std::vector<CounterRow> parseCounterRows(std::string const& rows, std::string const& colors);
cocos2d::ccColor3B defaultRowColor(int index);

// ---- settings snapshot -------------------------------------------------

struct Settings {
    bool circles = true;
    bool counters = true;
    bool sounds = true;
    bool countShip = false;
    bool showReleases = true;
    bool numberBeside = false;
    bool showMilliseconds = false;
    int maxCircleFrames = 15;
    int countFps = 240;
    double circleScale = 1.0;
    double numberScale = 1.0;
    double counterScale = 1.0;
    std::vector<CounterRow> rows;

    int scanRange = 12;
    int checkTicks = 120;
    int followingInputs = 2;
    bool skipUseless = true;
    bool exactScan = true;
    bool checkToEnd = true;
    bool resumeScan = true;
    bool roundWindowsUp = true;
    bool countOpenWindows = false;
    int scanBudgetMs = 12;

    static Settings load();

    std::string scanSignature() const;
    // A window in ticks, converted to counted frames.
    int framesFor(Window const& w) const;
    CounterRow const* rowFor(int frames) const;
    // Whether an input takes part in the counter and gets a circle at all.
    bool counts(Input const& in, Window const& w) const;
};

} // namespace fwl
