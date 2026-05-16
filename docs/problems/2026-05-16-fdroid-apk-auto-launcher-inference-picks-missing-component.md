## Exact Error

```text
Verification Loading: [#######---] 67/100
Runtime Path: apk-backed-linux-launch
APK Path: /home/astra/Downloads/F-Droid.apk
Package: org.fdroid.fdroid
Install ID: vc1023052-1.23.2
Launch Mode: launch-activity
Launcher Selection: inferred
Launcher Component: org.fdroid.fdroid/org.fdroid.fdroid.panic.CalculatorActivity
IME Component:
APK Load OK: yes
Launcher Generation OK: yes
Generated Launcher OK: no
Generated Launcher Output:
ADB Serial: 192.168.240.112:5555
Component: org.fdroid.fdroid/org.fdroid.fdroid.panic.CalculatorActivity
Launch OK: no
Launch Output:
Starting: Intent { cmp=org.fdroid.fdroid/.panic.CalculatorActivity }
Error type 3
Error: Activity class {org.fdroid.fdroid/org.fdroid.fdroid.panic.CalculatorActivity} does not exist.
```

## Reproduction Steps

1. Build the current Linuxoid tree with `cmake --build build`.
2. Run `ctest --test-dir build --output-on-failure`.
3. Run `./build/compatctl verify-apk-host-launch-auto 192.168.240.112:5555 /home/astra/Downloads/F-Droid.apk /tmp/linuxoid-apk-verify-fdroid /tmp/linuxoid-apk-applications-fdroid /tmp/linuxoid-apk-launchers-fdroid` outside the sandbox.
4. Observe Linuxoid infer `org.fdroid.fdroid.panic.CalculatorActivity` as the launcher component and fail the generated launcher execution.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Project: Linuxoid
- Runtime backend: Waydroid-backed Android target `192.168.240.112:5555`
- Date: 2026-05-16

## First Hypothesis

Linuxoid's automatic launcher inference is selecting the first manifest component that looks launchable, but F-Droid's manifest likely contains a special-purpose or alias-based launcher component that is not the real foreground entry point on this runtime. The inference logic probably needs a stronger filter or precedence rule.
