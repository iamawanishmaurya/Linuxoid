# Integrations

This repository has a small set of direct external integrations.
Most of the implementation is local and deterministic.

System library integrations:
- optional Wayland client headers and library discovered in `CMakeLists.txt`
- optional `EGL` headers and library discovered in `CMakeLists.txt`
- `dl` for dynamic library loading

Linux and host environment integrations:
- `/proc/self/exe` resolution in `/home/astra/codex/wine-for-android/src/main.cpp`
- filesystem staging under `/tmp/...` and user-selected staging roots
- environment variable overrides for runtime discovery such as ART runtime roots

Runtime strategy integrations surfaced in code:
- `/home/astra/codex/wine-for-android/src/runtime_bridge.cpp`
- `/home/astra/codex/wine-for-android/include/wfa/runtime_bridge.hpp`
- `/home/astra/codex/wine-for-android/src/waydroid_integration.cpp`
- `/home/astra/codex/wine-for-android/include/wfa/waydroid_integration.hpp`

Operator and reporting integration:
- `compatctl` is the single operator-facing CLI surface
- JSON reports are written into staged session directories
- replay and recovery artifacts are intentionally persisted for later inspection

Test and fixture integrations:
- `/home/astra/codex/wine-for-android/tests/test_main.cpp`
- `/home/astra/codex/wine-for-android/tests/p1_fixture_native_activity.cpp`
- deterministic APK-like ZIP fixtures generated locally by tests

The test flow does not require:
- Android SDK
- Gradle
- network access
- emulator
- ADB
- Waydroid
- live Wayland display

Documentation integrations:
- long-form technical notes under `/home/astra/codex/wine-for-android/docs/`
- problem-first writeups under `/home/astra/codex/wine-for-android/docs/problems/`
- solution notes under `/home/astra/codex/wine-for-android/docs/solutions/`

The repo currently favors lightweight host integrations plus strong local diagnostics over deep external service dependencies.
