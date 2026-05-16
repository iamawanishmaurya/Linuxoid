# ART Runtime Smoke Missing Module Solution

Problem: [docs/problems/2026-05-17-art-runtime-smoke-missing-module.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-art-runtime-smoke-missing-module.md)

## What failed

The new ART runtime smoke tests were added before the production runtime-smoke seam existed, so the build stopped immediately at `#include "wfa/art_runtime_smoke.hpp"`.

## What worked

Added the missing production seam:

- [include/wfa/art_runtime_smoke.hpp](/home/astra/codex/wine-for-android/include/wfa/art_runtime_smoke.hpp)
- [src/art_runtime_smoke.cpp](/home/astra/codex/wine-for-android/src/art_runtime_smoke.cpp)

and wired it into:

- [src/main.cpp](/home/astra/codex/wine-for-android/src/main.cpp)
- [src/runtime_health.cpp](/home/astra/codex/wine-for-android/src/runtime_health.cpp)
- [CMakeLists.txt](/home/astra/codex/wine-for-android/CMakeLists.txt)

## Why it worked

Linuxoid now has a concrete host-ART smoke layer on top of the classloader plan: deterministic invocation artifacts, safe `dalvikvm` probing when available, and honest fallback when ART is absent. That satisfies the new tests and gives the self-healing runtime a richer DEX/classloader evidence path without pretending real class execution is already done.

## Commands run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl native-art-runtime-smoke /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl native-runtime-health-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline
```
