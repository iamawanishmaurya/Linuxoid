# Linuxoid

Linuxoid currently contains the first executable MVP scaffold for an Android-on-Linux compatibility project.

## Current Direction

- Core language: `C++20`
- Runtime strategy: container-first Android userspace integration
- Current executable: `compatctl`

## Current Working Architecture

```mermaid
flowchart TB
  User["Linux User"] --> Desktop["Linux Desktop Entry or Shell Launcher"]
  Desktop --> Compatctl["Linuxoid compatctl"]

  subgraph Host["Linux Host"]
    Compatctl --> Status["Status and Checkpoint Engine"]
    Compatctl --> Loader["APK Loader and Manifest Assessor"]
    Compatctl --> Desktopify["Desktop Artifact Generator"]
    Compatctl --> Verifier["Waydroid Verifier and Matrix Verifier"]
    Compatctl --> ApkVerifier["APK-backed Host Verifier"]
    Compatctl --> Runtime["Runtime Bridge Layer"]
    Loader --> CompatRoot["Compat Root and Package Staging"]
    Desktopify --> DesktopFiles[".desktop Files and Launcher Scripts"]
  end

  subgraph Backend["Waydroid Runtime Backend"]
    Runtime --> WaydroidCLI["waydroid app and session commands"]
    Runtime --> AdbBridge["ADB Activity and IME Commands"]
    WaydroidCLI --> Android["Android Userspace"]
    AdbBridge --> Android
    Android --> InstalledApps["Installed Android Apps"]
  end

  Compatctl --> KeyboardFlow["APK-backed IME Provisioning Flow"]
  KeyboardFlow --> Loader
  KeyboardFlow --> Desktopify
  KeyboardFlow --> ApkVerifier
  KeyboardFlow --> Runtime
  Runtime --> KeyboardApp["FUTO Keyboard Ready-for-typing Proof"]

  Compatctl --> InstalledFlow["Installed-package Linux Launch Flow"]
  InstalledFlow --> Verifier
  Verifier --> DesktopFiles
  Verifier --> WaydroidCLI
  InstalledApps --> ProvenApps["Calculator, Settings, and F-Droid"]
```

This diagram is the current working architecture and should stay in sync with the verified Linuxoid flow on GitHub.

## What Exists Today

- A C++ checkpoint engine with weighted runtime gates
- A phase-progress reporter with `0-100` loading output
- A package-layout planner for APK, app data, and OBB storage
- A decoded-manifest assessor for runtime and service requirements
- A real `load-apk` path that stages an APK into a compat root
- A generic `launch-activity` bridge for explicit Android component launches from Linux
- An `adb-ime-status` runtime bridge for installed IME verification on a live Android target
- A `provision-ime` runtime bridge that installs, enables, sets default, and re-verifies an IME on a live Android target
- A `desktopify-apk` path that generates a Linux wrapper script and `.desktop` entry for a caller-selected Android component, and for IME apps it generates a provisioning launcher with explicit side effects
- A `desktopify-apk-auto` path that infers the launcher activity from the APK manifest and splits desktop-entry and launcher-script roots for cleaner host integration
- A `verify-apk-host-launch-auto` path that stages a local APK, generates Linux launcher artifacts for it, and verifies the generated launcher path against a live Android runtime
- A `launch-waydroid-package` path that launches an already installed app through Waydroid without requiring an APK reinstall or a hardcoded ADB serial
- A `desktopify-waydroid-package` path that generates a Linux launcher and `.desktop` entry for an installed Waydroid app
- A `verify-waydroid-package` path that proves direct Linux launch for an installed Waydroid app by checking the runtime launch, the generated host launcher artifacts, and the generated launcher execution
- A `verify-waydroid-matrix` path that runs the direct Linux verification loop across several installed Waydroid apps and reports pass/fail per package
- A live Waydroid-backed proof that a Linuxoid-generated launcher can install the keyboard APK, enable it, set it as default, and return `Ready for typing: yes` from Linux
- A live Waydroid-backed proof that the keyboard APK now verifies end to end from its local file path through the Linuxoid-generated launcher flow
- A live Waydroid-backed proof that a Linuxoid-generated launcher can open `com.android.calculator2` from Linux through the new installed-package path
- A live Waydroid-backed mini-matrix that verifies direct Linux launch for `com.android.calculator2`, `com.android.settings`, and `org.fdroid.fdroid`
- A local test suite that verifies the first scaffold behavior

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Verify

```bash
ctest --test-dir build --output-on-failure
./build/compatctl status
./build/compatctl foundation
./build/compatctl layout com.example.demo alpha01 42 /var/lib/wfa
./build/compatctl assess-manifest /path/to/decoded/AndroidManifest.xml
./build/compatctl load-apk /path/to/app.apk /tmp/wfa-load
./build/compatctl launch-activity emulator-5590 org.example.app/.SettingsActivity
./build/compatctl verify-apk-host-launch-auto emulator-5590 /path/to/app.apk /tmp/linuxoid-apk-verify /tmp/linuxoid-apk-applications /tmp/linuxoid-apk-launchers
./build/compatctl launch-waydroid-package com.android.calculator2
./build/compatctl verify-waydroid-package com.android.calculator2 /tmp/linuxoid-applications /tmp/linuxoid-launchers
./build/compatctl verify-waydroid-matrix /tmp/linuxoid-matrix com.android.calculator2 com.android.settings org.fdroid.fdroid
./build/compatctl adb-ime-status emulator-5590 org.example.app org.example.app/.ImeService
./build/compatctl adb-ime-status emulator-5590 org.example.app org.example.app/.ImeService org.example.app/.SettingsActivity
./build/compatctl provision-ime emulator-5590 /path/to/app.apk org.example.app org.example.app/.ImeService org.example.app/.SettingsActivity
./build/compatctl desktopify-apk emulator-5590 /path/to/app.apk org.example.app/.SettingsActivity /tmp/wfa-load /tmp/wfa-desktop
./build/compatctl desktopify-apk-auto emulator-5590 /path/to/app.apk /tmp/wfa-load /tmp/linuxoid-applications /tmp/linuxoid-launchers
./build/compatctl desktopify-waydroid-package com.android.calculator2 /tmp/linuxoid-applications /tmp/linuxoid-launchers
```

## Current Progress

- Phase loading: `89/100`
- Runtime checkpoint gates: `70/100`

These values are generated by the code, not written by hand.
