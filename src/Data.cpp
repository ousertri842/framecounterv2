#include "Data.hpp"
#include "ScanMath.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

using namespace geode::prelude;

namespace fwl {

char const* gamemodeName(Gamemode mode) {
    switch (mode) {
        case Gamemode::Cube: return "cube";
        case Gamemode::Ship: return "ship";
        case Gamemode::Ball: return "ball";
        case Gamemode::Ufo: return "ufo";
        case Gamemode::Wave: return "wave";
        case Gamemode::Robot: return "robot";
        case Gamemode::Spider: return "spider";
        case Gamemode::Swing: return "swing";
    }
    return "cube";
}

bool Recording::hasAnyResults() const {
    for (auto const& w : windows) {
        if (w.measured) return true;
    }
    return false;
}

void Recording::resetResults() {
    windows.assign(inputs.size(), Window{});
    scanComplete = false;
}

// ---- persistence -------------------------------------------------------
//
// A small line-based text format, so files stay readable and diffable:
//
//   FWL 2
//   level <key>
//   name <level name>
//   end <tick>
//   complete <0|1>
//   scan <algorithm and scan settings signature, or empty>
//   inputs <count>
//   <tick> <down> <button> <player1> <mode> <x> <y>        (count lines)
//   windows <count>
//   <measured> <unknown> <useless> <left> <right> <openL> <openR>

std::filesystem::path recordingPath(std::string const& levelKey) {
    return Mod::get()->getSaveDir() / "recordings" / (levelKey + ".fwl");
}

Result<> saveRecording(Recording const& rec) {
    auto dir = Mod::get()->getSaveDir() / "recordings";
    GEODE_UNWRAP(file::createDirectoryAll(dir));

    std::ostringstream out;
    out << "FWL 2\n";
    out << "level " << rec.levelKey << "\n";
    out << "name " << rec.levelName << "\n";
    out << "end " << rec.endTick << "\n";
    out << "complete " << (rec.scanComplete ? 1 : 0) << "\n";
    out << "scan " << rec.scanSignature << "\n";
    out << "inputs " << rec.inputs.size() << "\n";
    for (auto const& in : rec.inputs) {
        out << in.tick << ' ' << (in.down ? 1 : 0) << ' ' << in.button << ' '
            << (in.player1 ? 1 : 0) << ' ' << static_cast<int>(in.mode) << ' '
            << in.x << ' ' << in.y << "\n";
    }
    out << "windows " << rec.windows.size() << "\n";
    for (auto const& w : rec.windows) {
        out << (w.measured ? 1 : 0) << ' ' << (w.unknown ? 1 : 0) << ' '
            << (w.useless ? 1 : 0) << ' ' << w.left << ' ' << w.right << ' '
            << (w.openLeft ? 1 : 0) << ' ' << (w.openRight ? 1 : 0) << "\n";
    }
    return file::writeStringSafe(recordingPath(rec.levelKey), out.str());
}

std::optional<Recording> loadRecording(std::string const& levelKey) {
    auto path = recordingPath(levelKey);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return std::nullopt;

    auto text = file::readString(path);
    if (!text) {
        log::warn("Could not read {}: {}", path, text.unwrapErr());
        return std::nullopt;
    }

    std::istringstream in(text.unwrap());
    std::string word;
    int version = 0;
    if (!(in >> word >> version) || word != "FWL" || (version != 1 && version != 2)) {
        log::warn("{} is not a Frame Window Lab recording", path);
        return std::nullopt;
    }

    Recording rec;
    rec.levelKey = levelKey;
    size_t count = 0;
    while (in >> word) {
        if (word == "level") {
            in >> rec.levelKey;
        }
        else if (word == "name") {
            in >> std::ws;
            std::getline(in, rec.levelName);
        }
        else if (word == "end") {
            in >> rec.endTick;
        }
        else if (word == "complete") {
            int c = 0;
            in >> c;
            rec.scanComplete = c != 0;
        }
        else if (word == "scan") {
            // An unscanned recording has an empty signature; don't consume
            // the next line's key with formatted extraction.
            std::getline(in, rec.scanSignature);
            if (!rec.scanSignature.empty() && rec.scanSignature.front() == ' ') {
                rec.scanSignature.erase(0, 1);
            }
        }
        else if (word == "inputs") {
            in >> count;
            rec.inputs.reserve(count);
            for (size_t i = 0; i < count; i++) {
                Input input;
                int down = 0, p1 = 0, mode = 0;
                if (!(in >> input.tick >> down >> input.button >> p1 >> mode >> input.x >> input.y)) {
                    log::warn("{} is damaged (inputs)", path);
                    return std::nullopt;
                }
                input.down = down != 0;
                input.player1 = p1 != 0;
                input.mode = static_cast<Gamemode>(std::clamp(mode, 0, 7));
                rec.inputs.push_back(input);
            }
        }
        else if (word == "windows") {
            in >> count;
            rec.windows.reserve(count);
            for (size_t i = 0; i < count; i++) {
                Window w;
                int measured = 0, unknown = 0, useless = 0, openL = 0, openR = 0;
                if (!(in >> measured >> unknown >> useless >> w.left >> w.right >> openL >> openR)) {
                    log::warn("{} is damaged (windows)", path);
                    return std::nullopt;
                }
                w.measured = measured != 0;
                w.unknown = unknown != 0;
                w.useless = useless != 0;
                w.openLeft = openL != 0;
                w.openRight = openR != 0;
                rec.windows.push_back(w);
            }
        }
        else {
            // Unknown key from a newer version: skip the rest of the line.
            std::string rest;
            std::getline(in, rest);
        }
    }

    // Keep old recordings, but don't display measurements from the old
    // algorithm as if they had been checked by this version.
    if (version == 1 || rec.windows.size() != rec.inputs.size()) rec.resetResults();
    return rec;
}

bool deleteRecording(std::string const& levelKey) {
    std::error_code ec;
    return std::filesystem::remove(recordingPath(levelKey), ec);
}

// ---- counter rows ------------------------------------------------------

static std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

static std::vector<std::string> splitComma(std::string const& text) {
    std::vector<std::string> parts;
    std::string part;
    std::istringstream in(text);
    while (std::getline(in, part, ',')) {
        part = trim(part);
        if (!part.empty()) parts.push_back(part);
    }
    return parts;
}

cocos2d::ccColor3B defaultRowColor(int index) {
    // Hot for tight windows, cool for roomy ones.
    static cocos2d::ccColor3B const palette[] = {
        {255, 84, 104},   // 1: red
        {255, 154, 72},   // 2: orange
        {255, 214, 84},   // 3: yellow
        {136, 232, 120},  // 4: green
        {96, 200, 255},   // 5: sky
        {150, 140, 255},  // 6: violet
        {230, 130, 255},  // 7: pink
        {200, 210, 225},  // 8+: grey
    };
    int n = static_cast<int>(sizeof(palette) / sizeof(palette[0]));
    return palette[std::clamp(index, 0, n - 1)];
}

static std::optional<cocos2d::ccColor3B> parseHex(std::string hex) {
    if (!hex.empty() && hex[0] == '#') hex.erase(0, 1);
    if (hex.size() != 6) return std::nullopt;
    for (char c : hex) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return std::nullopt;
    }
    auto value = std::stoul(hex, nullptr, 16);
    return cocos2d::ccColor3B{
        static_cast<GLubyte>((value >> 16) & 0xff),
        static_cast<GLubyte>((value >> 8) & 0xff),
        static_cast<GLubyte>(value & 0xff),
    };
}

std::vector<CounterRow> parseCounterRows(std::string const& rowsText, std::string const& colorsText) {
    std::vector<CounterRow> rows;
    for (auto const& part : splitComma(rowsText)) {
        CounterRow row;
        try {
            if (part.back() == '+') {
                row.from = std::stoi(part.substr(0, part.size() - 1));
                row.to = INT_MAX;
            }
            else if (auto dash = part.find('-'); dash != std::string::npos && dash > 0) {
                row.from = std::stoi(part.substr(0, dash));
                row.to = std::stoi(part.substr(dash + 1));
            }
            else {
                row.from = row.to = std::stoi(part);
            }
        }
        catch (...) {
            continue;
        }
        if (row.to < row.from) std::swap(row.to, row.from);
        row.from = std::max(row.from, 1);
        row.to = std::max(row.to, row.from);
        row.label = part;
        row.color = defaultRowColor(static_cast<int>(rows.size()));
        rows.push_back(row);
    }
    if (rows.empty()) {
        return parseCounterRows("1,2,3,4,5+", colorsText);
    }

    // "first frame:colour" pairs, e.g. 1:ff5468,5:60c8ff
    for (auto const& part : splitComma(colorsText)) {
        auto colon = part.find(':');
        if (colon == std::string::npos) continue;
        int frame = 0;
        try {
            frame = std::stoi(part.substr(0, colon));
        }
        catch (...) {
            continue;
        }
        auto color = parseHex(trim(part.substr(colon + 1)));
        if (!color) continue;
        for (auto& row : rows) {
            if (row.from == frame) row.color = *color;
        }
    }
    return rows;
}

// ---- settings ----------------------------------------------------------

Settings Settings::load() {
    auto mod = Mod::get();
    Settings s;
    s.circles = mod->getSettingValue<bool>("circles");
    s.counters = mod->getSettingValue<bool>("counters");
    s.sounds = mod->getSettingValue<bool>("sounds");
    s.countShip = mod->getSettingValue<bool>("count-ship");
    s.showReleases = mod->getSettingValue<bool>("show-releases");
    s.numberBeside = mod->getSettingValue<bool>("number-beside-ring");
    s.showMilliseconds = mod->getSettingValue<bool>("show-milliseconds");
    s.maxCircleFrames = static_cast<int>(mod->getSettingValue<int64_t>("max-circle-frames"));
    s.countFps = static_cast<int>(mod->getSettingValue<int64_t>("count-fps"));
    s.circleScale = mod->getSettingValue<double>("circle-scale");
    s.numberScale = mod->getSettingValue<double>("number-scale");
    s.counterScale = mod->getSettingValue<double>("counter-scale");
    s.rows = parseCounterRows(
        mod->getSettingValue<std::string>("counter-rows"),
        mod->getSettingValue<std::string>("counter-colors")
    );
    s.scanRange = static_cast<int>(mod->getSettingValue<int64_t>("scan-range"));
    s.checkTicks = static_cast<int>(mod->getSettingValue<int64_t>("check-ticks"));
    s.followingInputs = static_cast<int>(mod->getSettingValue<int64_t>("following-inputs"));
    s.skipUseless = mod->getSettingValue<bool>("skip-useless");
    s.exactScan = mod->getSettingValue<bool>("exact-scan");
    s.checkToEnd = mod->getSettingValue<bool>("check-to-end");
    s.resumeScan = mod->getSettingValue<bool>("resume-scan");
    s.roundWindowsUp = mod->getSettingValue<bool>("round-windows-up");
    s.countOpenWindows = mod->getSettingValue<bool>("count-open-windows");
    s.scanBudgetMs = static_cast<int>(mod->getSettingValue<int64_t>("scan-budget-ms"));
    return s;
}

std::string Settings::scanSignature() const {
    return fmt::format("v2:{}:{}:{}:{}:{}:{}", scanRange, checkTicks,
        followingInputs, skipUseless, exactScan, checkToEnd);
}

int Settings::framesFor(Window const& w) const {
    return scanmath::frames(w.ticks(), countFps, roundWindowsUp);
}

bool Settings::counts(Input const& in, Window const& w) const {
    if (in.button != 1) return false;
    if (!w.measured || w.unknown || w.useless) return false;
    if (in.mode == Gamemode::Ship && !countShip) return false;
    if (!in.down && !showReleases) return false;
    return true;
}

CounterRow const* Settings::rowFor(int frames) const {
    for (auto const& row : rows) {
        if (row.contains(frames)) return &row;
    }
    return nullptr;
}

} // namespace fwl
