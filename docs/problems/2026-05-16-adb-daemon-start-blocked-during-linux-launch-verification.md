## Exact Error

```text
Readback Error: command failed: adb -s 'emulator-5590' shell pm list packages
* daemon not running; starting now at tcp:5037
ADB server didn't ACK
Full server startup log: /tmp/adb.1000.log
Server had pid: 54
--- adb starting (pid 54) ---
05-16 09:44:45.154    54    54 I adb     : main.cpp:63 Android Debug Bridge version 1.0.41
05-16 09:44:45.154    54    54 I adb     : main.cpp:63 Version 35.0.2-android-tools
05-16 09:44:45.154    54    54 I adb     : main.cpp:63 Installed as /usr/bin/adb
05-16 09:44:45.154    54    54 I adb     : main.cpp:63 Running on Linux 6.6.87-rt54-arch1-7-rt-lts (x86_64)
05-16 09:44:45.154    54    54 I adb     : main.cpp:63
05-16 09:44:45.154    54    54 W adb     : usb_libusb.cpp:1063 failed to initialize libusb: LIBUSB_ERROR_OTHER
05-16 09:44:45.655    54    54 F adb     : main.cpp:157 could not install *smartsocket* listener: Operation not permitted

* failed to start daemon
adb: cannot connect to daemon
```

## Reproduction Steps

1. Generate the Linux-side launcher with `./build/compatctl desktopify-apk-auto emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk /tmp/linuxoid-load-auto /tmp/linuxoid-apps-auto /tmp/linuxoid-launchers-auto`.
2. Execute `/tmp/linuxoid-launchers-auto/org.futo.inputmethod.latin.sh`.
3. Observe the wrapper fail when `adb` tries to auto-start its daemon inside the sandboxed environment.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Sandbox mode: workspace-write
- Target: `emulator-5590`
- Date: 2026-05-16

## First Hypothesis

The Linux-side launcher path is correct, but the current verification shell cannot start the `adb` daemon because the sandbox blocks the listener bind it needs. Pre-starting the daemon outside the sandbox should allow the same generated launcher to pass.
