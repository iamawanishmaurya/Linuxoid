# Testing

The repository uses `CTest` through the root `CMakeLists.txt`.
The primary test executable is `wfa_tests`.

Current build and test commands:
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`

Main test locations:
- `/home/astra/codex/wine-for-android/tests/test_main.cpp`
- `/home/astra/codex/wine-for-android/tests/p1_fixture_native_activity.cpp`

The testing strategy is heavily integration-oriented.
Most new runtime slices are proven by constructing deterministic fixture APKs or bundle artifacts and exercising `compatctl`-level code paths or closely related helpers.

What tests emphasize today:
- direct APK launch proofs
- staged artifact correctness
- DEX parsing and minimal bytecode execution checkpoints
- real APK checkpoints for the keyboard settings activity launch path
- exact live blocker reporting for the current `unsupported-dex-opcode:opcode-0xbb` boundary
- honest failure boundaries
- Self-Healing Android Device recovery diagnostics

The tests are intentionally offline.
They avoid dependency on:
- Android SDK
- Gradle
- network services
- emulator
- ADB
- Waydroid
- live compositor or display server

The suite currently appears to be concentrated in one large file.
That keeps fixtures close to assertions, but it is also a maintainability pressure point.

There is also a shared library fixture target:
- `linuxoid_p1_fixture`

When extending the codebase, prefer:
- deterministic local fixture generation
- assertions on structured JSON or stable report fields
- explicit checks for blockers, degraded states, and recovery actions
