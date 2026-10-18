#pragma once

namespace fwl::sounds {

// Plays the cue for a window `frames` wide. Releases and unmeasured inputs
// get their own cues.
void play(int frames, bool release, bool unknown);

// Drops cached sounds, e.g. after the custom sound settings change.
void clearCache();

} // namespace fwl::sounds
