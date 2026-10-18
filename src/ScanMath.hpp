#pragma once

#include <algorithm>
#include <cstdint>

namespace fwl::scanmath {

// Convert a duration directly: a rounded ticks-per-frame divisor is wrong
// at rates which do not divide 240 (for example, 180 FPS).
constexpr int frames(int ticks, int fps, bool roundUp) {
    auto scaled = static_cast<int64_t>(std::max(1, ticks)) * std::clamp(fps, 60, 240);
    return static_cast<int>(std::max<int64_t>(1, (scaled + (roundUp ? 239 : 0)) / 240));
}

constexpr int endTick(int moved, int following, int recordedEnd, int checkTicks) {
    auto end = static_cast<int64_t>(std::max(moved, following)) + std::max(1, checkTicks);
    return static_cast<int>(std::min(end, static_cast<int64_t>(recordedEnd) + 60));
}

// Reaching a timeout is never proof of completion. Death takes precedence
// if both callbacks happened in the same game update.
constexpr bool passed(bool died, bool stalled, bool completed, bool timedOut, bool requireCompletion) {
    return !died && !stalled && (completed || (timedOut && !requireCompletion));
}

constexpr bool retryExactly(bool exactScan, bool alreadyRetried) {
    return !exactScan && !alreadyRetried;
}

} // namespace fwl::scanmath
