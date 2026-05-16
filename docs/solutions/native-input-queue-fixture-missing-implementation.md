Links back to: [2026-05-17-native-input-queue-fixture-missing-implementation.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-native-input-queue-fixture-missing-implementation.md)

## What Failed

The new focused-input tests linked against declarations in `/home/astra/codex/wine-for-android/include/wfa/native_input_queue_fixture.hpp`, but Linuxoid had no implementation file or target wiring for:

- `wfa::RunNativeInputQueueFixture(...)`
- `wfa::RenderNativeInputQueueFixtureJson(...)`

That left `wfa_tests` failing at link time.

## What Worked

Linuxoid now implements the missing fixture in `/home/astra/codex/wine-for-android/src/native_input_queue_fixture.cpp`, wires it into `linuxoid_p1` in `/home/astra/codex/wine-for-android/CMakeLists.txt`, exposes it through `compatctl` in `/home/astra/codex/wine-for-android/src/main.cpp`, and exercises it from `/home/astra/codex/wine-for-android/tests/test_main.cpp`.

The fixture builds on the existing `ANativeWindow` bridge seam, then writes:

- deterministic focus ownership metadata
- deterministic pointer and keyboard event counts
- stable JSON metadata at `native-input-queue-metadata.json`
- stable JSONL event output at `native-input-events.jsonl`

## Why It Worked

The missing layer was not runtime complexity; it was absent code and link registration. Implementing the fixture on top of the already verified bridge seam kept the scope narrow, preserved deterministic artifact paths for MCP/harness replay, and let Linuxoid prove a focused native input contract without pretending full IME or text composition already exists.

## Commands Run

```bash
cmake --build build
./build/compatctl native-input-queue-fixture /tmp/linuxoid-native-input-queue-smoke 48 32 1
ctest --test-dir build --output-on-failure
./build/compatctl status
```
