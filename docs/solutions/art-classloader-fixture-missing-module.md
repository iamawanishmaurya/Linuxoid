# ART Classloader Fixture Missing Module Solution

Problem: [docs/problems/2026-05-17-art-classloader-fixture-missing-module.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-art-classloader-fixture-missing-module.md)

## What failed

The new tests for a Linuxoid-owned ART/classloader preparation seam were added before the production module existed, so the build stopped immediately at `#include "wfa/art_classloader_fixture.hpp"`.

## What worked

Added the missing production seam:

- [include/wfa/art_classloader_fixture.hpp](/home/astra/codex/wine-for-android/include/wfa/art_classloader_fixture.hpp)
- [src/art_classloader_fixture.cpp](/home/astra/codex/wine-for-android/src/art_classloader_fixture.cpp)

and wired it into:

- [src/main.cpp](/home/astra/codex/wine-for-android/src/main.cpp)
- [src/runtime_health.cpp](/home/astra/codex/wine-for-android/src/runtime_health.cpp)
- [CMakeLists.txt](/home/astra/codex/wine-for-android/CMakeLists.txt)

## Why it worked

The new module now gives Linuxoid a concrete classpath-preparation surface: dex inventory, manifest target normalization, stable ART-path artifacts, and a machine-readable CLI command. That satisfies the tests and turns `prepare_art_sidecar_classpath` from a pure recovery label into a real artifact-producing step.

## Commands run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl native-art-classloader-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl native-runtime-health-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline
```
