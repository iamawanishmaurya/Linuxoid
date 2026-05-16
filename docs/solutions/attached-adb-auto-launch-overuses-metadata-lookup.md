# Solution: attached-ADB auto-launch overuses metadata lookup

- Related problem: [2026-05-16-attached-adb-auto-launch-overuses-metadata-lookup](../problems/2026-05-16-attached-adb-auto-launch-overuses-metadata-lookup.md)

## What failed

The first attached-ADB auto-resolution implementation reused the full package metadata query inside `launch-package`. That made a lightweight launch fallback depend on package visibility, install-path lookup, and full `dumpsys package` parsing.

The failing test exposed that coupling through this symptom:

```text
Test failure: unexpected command in attached-adb auto-resolve launch test
```

## What worked

The fix was to split the behavior in two:

1. keep **full metadata lookup** in the dedicated `inspect-package` path
2. add a **lightweight launcher-resolution helper** for `launch-package`, `preflight-runtime`, and attached-target verification when callers omit a component

## Why it worked

The launch and preflight flows only need one thing: a resolved launcher component. They do not need install-path or version metadata in order to start an app. By separating those paths, Linuxoid keeps the runtime bridge smaller, preserves the stricter component-confirmation proof, and avoids making the native execution path depend on unnecessary shell queries.

## Commands run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl inspect-package attached-adb 192.168.240.112:5555 com.android.settings
./build/compatctl preflight-runtime attached-adb 192.168.240.112:5555 com.android.settings
./build/compatctl launch-package attached-adb com.android.settings 192.168.240.112:5555
./build/compatctl verify-package attached-adb com.android.settings 192.168.240.112:5555 - /tmp/linuxoid-attached-applications /tmp/linuxoid-attached-launchers
./build/compatctl verify-package-matrix attached-adb /tmp/linuxoid-attached-matrix 192.168.240.112:5555 com.android.settings com.android.calculator2 org.fdroid.fdroid
```
