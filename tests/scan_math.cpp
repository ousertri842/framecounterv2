#include "../src/ScanMath.hpp"

#include <cstdlib>
#include <iostream>

using namespace fwl::scanmath;

void check(bool condition, char const* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main() {
    check(frames(1, 240, true) == 1, "a single tick counts once");
    check(frames(25, 240, true) == 25, "both scan edges plus the original tick");
    check(frames(4, 60, true) == 1, "four ticks at 60 FPS");
    check(frames(5, 60, true) == 2, "fractional frames round up when requested");
    check(frames(5, 60, false) == 1, "legacy rounding remains available");
    check(frames(3, 120, true) == 2, "odd widths at 120 FPS");
    check(frames(4, 180, true) == 3, "180 FPS uses the actual ratio");
    check(frames(240, 144, true) == 144, "arbitrary allowed FPS values");
    check(frames(1, 60, false) == 1, "sub-frame widths never display zero");
    for (int fps = 60; fps <= 240; ++fps) {
        for (int ticks = 1; ticks <= 121; ++ticks) {
            auto up = frames(ticks, fps, true);
            auto down = frames(ticks, fps, false);
            check(up >= down && up - down <= 1, "rounding bounds");
            check(up * 240 >= ticks * fps, "round-up never understates duration");
            check(frames(ticks + 1, fps, true) >= up, "longer windows never count less");
        }
    }
    check(endTick(100, 200, 1000, 120) == 320, "following input determines horizon");
    check(endTick(300, 200, 1000, 120) == 420, "moved input must happen before checking");
    check(endTick(999, 1000, 1000, 2400) == 1060, "bounded end timeout");
    check(!passed(false, false, false, true, true), "survival alone cannot pass completion checks");
    check(passed(false, false, true, false, true), "actual completion passes");
    check(!passed(true, false, true, true, true), "death wins over completion");
    check(!passed(false, true, false, true, false), "stalled simulation never passes");
    check(passed(false, false, false, true, false), "short checks remain opt-in");
    check(!passed(false, false, false, false, false), "unfinished tests cannot pass");
    check(retryExactly(false, false), "every failed checkpoint verification gets an exact retry");
    check(!retryExactly(false, true), "exact retry cannot loop forever");
    check(!retryExactly(true, false), "exact mode already starts from the beginning");
    std::cout << "Scan regression tests passed\n";
}
