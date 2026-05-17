# Solution: Runtime health bootstrap execution duplicates work

Linked problem: [2026-05-17-runtime-health-bootstrap-execution-duplicates-work.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-runtime-health-bootstrap-execution-duplicates-work.md)

## What failed

The live staged Calculator run for:

- `native-runtime-health-fixture <bootstrap-manifest> baseline`

timed out repeatedly even though the unit tests were green. The timeout was not caused by the top-level recovery logic itself. The deeper problem was that Linuxoid kept repeating expensive work while building health records:

- reopening and reparsing the same APK zip over and over
- rereading the same dex entries from disk
- falling back to `apktool` manifest decode even when the staged native bundle already had a plain manifest copy

## Solutions considered

1. Reuse one opened APK archive per command and add archive-aware read helpers.
   - Pros: removes repeated zip parsing and repeated file rereads inside one command.
   - Cons: requires new archive surface and call-site changes.

2. Prefer the staged bundle manifest before `apktool` fallback.
   - Pros: avoids the slowest manifest-read path on native staged bundles.
   - Cons: needs a careful deterministic candidate search.

3. Reuse existing on-disk JSON artifacts instead of recomputing classloader/class-resolution/runtime-smoke results.
   - Pros: could make repeated runs even cheaper.
   - Cons: freshness rules are trickier; higher risk of stale evidence.

4. Reduce dex inspection depth when ART is unavailable.
   - Pros: cheaper immediately.
   - Cons: weakens the correctness of the pre-ART seam and loses real evidence we still need.

5. Add in-process memoization only inside `RunRuntimeHealthFixture`.
   - Pros: smallest code change.
   - Cons: would not fix the standalone classloader/class-resolution commands, which were also slow.

## What worked

Linuxoid now uses the combined fix from options `1` and `2`:

- one opened APK archive is reused across classloader and class-resolution work inside a command
- resource readiness now prefers the already-staged bundle manifest before any `apktool` decode fallback

That dropped the live staged Calculator timings from tens of seconds down to low single digits:

- `native-art-classloader-fixture`: about `26s` -> under `1s`
- `native-art-class-resolution-fixture`: about `26s` -> about `1s`
- `native-runtime-health-fixture`: timeout / over `20s` -> about `2.5s`

## Why it worked

This fix removed the repeated heavyweight work without weakening the runtime contract:

- archive inspection is still real
- DEX class resolution is still real
- manifest parsing is still real
- Linuxoid still falls back honestly when only the slower path is available

The important change is that the native staged-bundle path now uses the artifacts it already owns before reaching for expensive fallback behavior.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
time ./build/compatctl native-art-classloader-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json >/tmp/classloader.json
time ./build/compatctl native-art-class-resolution-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json >/tmp/classres.json
time ./build/compatctl native-runtime-health-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline >/tmp/runtimehealth.json
./build/compatctl native-runtime-diagnostic-replay /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
```
