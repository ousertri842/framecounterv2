# Frame Window Lab

Find out how tight every click in your run really was.

## How to use it

1. Open a level, pause, and press the **ring button** on the right of the pause menu.
2. **Record**: beat the level once. Every click is saved with the physics tick it landed on.
3. **Scan**: each click is replayed earlier and later, one tick at a time, to find how many frames it could have been on and still survive.
4. **Play**: watch the run back with a ring at every click, the frame window above it, counter rows in the top left, and a sound for each click.

Recording and playback run the physics at 240 steps a second, so a 1-frame click is a 1-tick window at 240 FPS. Set **Count frames at** to 120 or 60 to count at those frame rates instead.

## Good to know

- Platformer levels and practice mode aren't supported.
- Exact scan and Require level completion are recommended and enabled by default. Existing users should enable Exact scan manually if it was previously off. Faster checkpoint and short checks remain available, but are approximate.
- Count frames at converts the measured duration using ticks x FPS / 240, with selectable rounding. N+ means the scan found only a lower bound; these windows are excluded from counter rows by default.
- Updating from v1.0.0 keeps recorded inputs but clears old measurements. Scan again to calculate corrected results.
- Recordings are saved per level in the mod's save folder, and nothing needs the internet.
