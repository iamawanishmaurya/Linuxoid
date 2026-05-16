# Solution: Desktopify false-success paths

Related problem: [2026-05-16-desktopify-false-success-paths.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-desktopify-false-success-paths.md)

## What Failed

The first P5 artifact path could report `Host launch ready: yes` even when the generated wrapper still depended on the original source APK path or captured the wrong `compatctl` binary path. It also understated the side effects of IME launchers.

## What Worked

The host-launch path was hardened to:

- generate IME wrappers against the staged `base.apk` inside the compat root
- resolve the running `compatctl` path from `/proc/self/exe` instead of trusting `argv[0]`
- require the relevant staged files and binary path to exist before `host_launch_ready` becomes `yes`
- surface IME launcher side effects directly in the desktop artifact report and project documentation

## Why It Worked

Those changes make the generated Linux launcher self-contained and honest. The wrapper now depends on durable project-managed paths rather than caller-managed paths, and the report no longer overstates what a desktopified IME launcher actually does.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
PATH=/home/astra/codex/wine-for-android/build:$PATH compatctl desktopify-apk emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin/.uix.settings.SettingsActivity /tmp/wfa-load /tmp/wfa-desktop-path
sed -n '1,20p' /tmp/wfa-desktop-path/org.futo.inputmethod.latin.sh
/tmp/wfa-desktop-path/org.futo.inputmethod.latin.sh
```
