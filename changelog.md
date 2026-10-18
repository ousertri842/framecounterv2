# v1.1.0

- Correct FPS conversion, including 144 and 180 FPS, with selectable fractional-frame rounding.
- Require actual level completion by default to catch delayed deaths; removal tests always require completion.
- Use exact replays by default. Failed checkpoint verification retries every test for that input from the level start.
- Exclude unfinished window bounds from counter rows by default, with an option to include them.
- Exclude the release of a useless press too.
- Resume partial scans only with matching scan settings; add a fresh-scan option.
- Preserve v1.0.0 recording inputs but invalidate old measurements. Add automated counting and scan-decision checks before platform builds.

# v1.0.0

- First version: record, scan and play back runs with click circles, counter rows and click sounds.
