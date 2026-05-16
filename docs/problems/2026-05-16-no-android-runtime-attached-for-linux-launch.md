## Exact Error

```text
Readback Error: command failed: adb -s 'emulator-5590' shell pm list packages
adb: device 'emulator-5590' not found
```

## Reproduction Steps

1. Start the ADB daemon outside the sandbox.
2. Execute `/tmp/linuxoid-launchers-auto/org.futo.inputmethod.latin.sh`.
3. Observe the launcher fail while verifying the target because `emulator-5590` is not attached.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Verification path: Linux-side launcher outside the sandbox
- Expected serial: `emulator-5590`
- Date: 2026-05-16

## First Hypothesis

The Linuxoid host-launch path is now reaching the real host environment, but the Android runtime that earlier backed the golden-app proof is no longer attached to ADB. Either the emulator was closed, the serial changed, or the runtime needs to be restarted before direct-launch verification can continue.
