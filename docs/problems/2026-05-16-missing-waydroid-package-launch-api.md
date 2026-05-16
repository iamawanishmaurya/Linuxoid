## Exact Error

```text
/home/astra/codex/wine-for-android/tests/test_main.cpp:852:12: error: ‘LaunchWaydroidAppWithRunner’ is not a member of ‘wfa’
/home/astra/codex/wine-for-android/tests/test_main.cpp:855:30: error: ‘RenderWaydroidAppLaunchReport’ is not a member of ‘wfa’
/home/astra/codex/wine-for-android/tests/test_main.cpp:877:31: error: ‘CreateWaydroidDesktopLaunchArtifacts’ is not a member of ‘wfa’; did you mean ‘CreateDesktopLaunchArtifacts’?
/home/astra/codex/wine-for-android/tests/test_main.cpp:915:16: error: ‘CreateWaydroidDesktopLaunchArtifacts’ is not a member of ‘wfa’; did you mean ‘CreateDesktopLaunchArtifacts’?
```

## Reproduction Steps

1. Add the new Waydroid-native launch and desktopify tests to `/home/astra/codex/wine-for-android/tests/test_main.cpp`.
2. Run `cmake --build build`.
3. Observe the test compile fail because Linuxoid does not yet expose a Waydroid-native package launch API.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Project: Linuxoid
- Date: 2026-05-16

## First Hypothesis

The current Linuxoid runtime surface is still centered on ADB/component launches and APK-based desktopification. The next slice needs a Waydroid-native package launch report plus a separate desktop artifact generator for installed packages.
