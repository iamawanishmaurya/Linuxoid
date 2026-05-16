## Exact Error

```text
/home/astra/codex/wine-for-android/tests/test_main.cpp: In function ‘void {anonymous}::TestAutoDesktopLaunchArtifactsInferLauncherAndSplitRoots()’:
/home/astra/codex/wine-for-android/tests/test_main.cpp:756:31: error: ‘CreateDesktopLaunchArtifactsForLoadedApkAuto’ is not a member of ‘wfa’; did you mean ‘CreateDesktopLaunchArtifactsForLoadedApk’?
  756 |   const auto artifacts = wfa::CreateDesktopLaunchArtifactsForLoadedApkAuto(
      |                               ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      |                               CreateDesktopLaunchArtifactsForLoadedApk
/home/astra/codex/wine-for-android/tests/test_main.cpp: In function ‘void {anonymous}::TestAutoDesktopLaunchArtifactsRejectHeadlessApp()’:
/home/astra/codex/wine-for-android/tests/test_main.cpp:832:16: error: ‘CreateDesktopLaunchArtifactsForLoadedApkAuto’ is not a member of ‘wfa’; did you mean ‘CreateDesktopLaunchArtifactsForLoadedApk’?
  832 |     (void)wfa::CreateDesktopLaunchArtifactsForLoadedApkAuto(
      |                ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      |                CreateDesktopLaunchArtifactsForLoadedApk
```

## Reproduction Steps

1. Add the new auto-desktopification tests to `/home/astra/codex/wine-for-android/tests/test_main.cpp`.
2. Run `cmake --build build` from `/home/astra/codex/wine-for-android`.
3. Observe the compile failure for the missing `CreateDesktopLaunchArtifactsForLoadedApkAuto` entry point.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Toolchain: CMake + GCC for the C++20 MVP
- Date: 2026-05-16

## First Hypothesis

The existing desktop integration layer only supports explicit caller-provided launcher components and a single artifact root. The new host-integration slice needs a new auto-inference entry point plus split launcher and desktop-entry root support in the desktop artifact generator.
