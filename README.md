# Frame Window Lab

A Geode mod for Geometry Dash 2.2081 that records a run, measures every click's frame window, and plays the run back with click circles, counter rows and click sounds.

It's written from scratch. It doesn't use FB Frame Counter's code, sounds or art, and it runs fully offline.

## Build it (no setup on your PC)

1. Make a new **GitHub repository** and upload everything in this folder, including the hidden `.github` folder.
2. Open the repo's **Actions** tab. The *Build Geode Mod* workflow runs on every push; you can also start it by hand with **Run workflow**.
3. When it finishes (about 5–10 minutes), open the run and download **Build Output**. Inside is `razor.frame-window-lab.geode`.
4. Put that file in your Geometry Dash `geode/mods` folder and restart the game.

The workflow builds for Windows, macOS, Android32 and Android64.

For an existing repository, extract the updated source ZIP, replace the matching files in the repository (including `.github/workflows/build.yml`), and commit them. The push starts the regression checks and platform builds. The source ZIP itself is not an installable mod; use the `.geode` file from **Build Output** after the workflow succeeds.

### Building locally instead

With the [Geode CLI and SDK](https://docs.geode-sdk.org/getting-started/geode-cli) installed:

```
geode build
```

## Using it

Pause in a level and press the ring button on the right of the pause menu.

| Button | What it does |
| --- | --- |
| **Record** | Restarts the level and records your clicks until you beat it. Dying restarts the recording. |
| **Scan** | Measures every click. The game dims and shows progress. Pausing pauses the scan; **Cancel scan** stops it, and scanning again carries on where it stopped. |
| **Play** | Replays the run with circles, counters and sounds. It loops on each retry until you press Stop. Playback and scans run as test runs (like a start position), so they aren't saved as your own progress. |
| **Delete** | Removes this level's recording. |
| **Settings** | Opens the mod settings. |

## How the scan works

1. Each test starts from a level reset by default. If Exact scan is off, the recording is replayed up to just before a click's search range and the game state is saved there as a checkpoint.
2. **Verify**: the unchanged run must pass the selected check. Exact scan is now on by default and resets from the level start for every test. In checkpoint mode, failed baseline checks retry the whole input exactly once, even if the checkpoint was captured from a clean run, and show **?** if the exact replay also fails.
3. **Earlier / Later**: the click is moved 1, 2, 3… ticks earlier, then later, until the icon dies. A click never moves past its own press or release.
4. **Remove**: the press and its release are taken out. Only if the level still completes are both events left out as useless. This always checks the whole run.
5. **Reverify**: the unchanged run is checked once more, so nothing left over from the tests can skew the result.

A test passes only when the level completes with **Require level completion** enabled (the default). Turn it off to use the faster, approximate check: survive until the next *Following inputs* events have played, plus *Check length* ticks. A timeout without completion fails a completion check. Checkpoint mode remains approximate because the game's checkpoint does not restore every possible state.

The window is `earlier + later + 1` ticks. A window that hits the *Scan range* limit while still passing shows as `N+`. A click can't move past the next or previous input on the same button, so those edges are real edges, not `+`.

Display counts use `ticks × Count frames at / 240`, with selectable rounding (up by default, minimum 1). For example, 5 ticks at 60 FPS is 1.25 frames and displays as 2 with rounding up, or 1 with rounding down. This converts a duration; it does not resimulate a lower-FPS input grid. Rates such as 144 and 180 are supported correctly.

`N+` windows are lower bounds, so they are excluded from counter rows by default while their rings remain visible. Enable **Count unfinished window bounds** to include them. Increase **Scan range** to measure wider windows. Cancelled scans resume only with matching scan settings; disable **Resume matching partial scans** to start fresh.

### Updating from v1.0.0

Existing recordings keep their inputs, but old scan results are cleared and need a rescan. Existing saved settings may retain **Exact scan: off**: enable **Exact scan** and **Require level completion** for the most reliable mode. Completion checks can be much slower on long levels. Old v1.0.0 binaries cannot read recordings saved by v1.1.0.

## Settings

| Group | Setting | Default |
| --- | --- | --- |
| Playback display | Click circles, Frame counters, Click sounds | on |
| | Show releases | on |
| | Count ship clicks | off |
| | Max circle frames | 15 |
| | Count frames at | 240 |
| | Round fractional frames up | on |
| | Count unfinished window bounds | off |
| | Number beside the ring, Milliseconds | off |
| | Circle / number / counter size | 1.0 |
| Counter rows | Rows | `1,2,3,4,5+` |
| | Colours | built-in palette |
| Scan | Scan range | 12 ticks |
| | Check length | 120 ticks |
| | Following inputs | 2 |
| | Leave out useless clicks | on |
| | Exact scan | on |
| | Require level completion | on |
| | Resume matching partial scans | on |
| | Scan time per frame | 12 ms |
| Custom sounds | 1–5 frame sounds, wide sound | bundled |

## Limits

- Classic levels only. Platformer levels and practice mode aren't supported.
- Exact scan replays from the start for every test. Turning it off enables approximate checkpoint scanning; successful baseline replays alone cannot prove every moved-input test restores correctly.
- Other mods that change physics or input timing, such as a TPS bypass or Click Between Frames, can make recordings replay differently. Turn them off while you record, scan and play.
- Recordings are saved as text files in `geode/mods/razor.frame-window-lab/recordings` (inside your save folder).

## Source layout

| File | What's in it |
| --- | --- |
| `src/main.cpp` | Game hooks: input, physics steps, reset, death, level end, pause menu button |
| `src/Lab.*` | Recording, playback and the scan engine |
| `src/Overlay.*` | In-level drawing: rings, counter rows, status pill and scan progress |
| `src/Menu.*` | The pause-menu popup |
| `src/Data.*` | Recording format, counter rows and settings |
| `src/ScanMath.hpp`, `tests/` | FPS conversion, scan decisions and regression checks |
| `src/Sounds.*` | Click sounds via FMOD |
| `src/Style.hpp` | Shared colours and UI helpers for the dark theme |

## Licence

MIT. See `LICENSE`.
