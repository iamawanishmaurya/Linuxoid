## Exact Error

```text
Unknown input method org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME cannot be enabled for user #0
Unknown input method org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME cannot be selected for user #0
```

## Reproduction Steps

1. Connect ADB to the live Waydroid device at `192.168.240.112:5555`.
2. Install the keyboard APK and verify that `adb -s 192.168.240.112:5555 shell ime list -a` contains `org.futo.inputmethod.latin/.LatinIME`.
3. Run:
   - `adb -s 192.168.240.112:5555 shell ime enable org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME`
   - `adb -s 192.168.240.112:5555 shell ime set org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME`
4. Observe both commands fail even though the IME is registered in short component form.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Project: Linuxoid
- Target: Waydroid device `192.168.240.112:5555`
- Date: 2026-05-16

## First Hypothesis

Linuxoid accepts equivalent short and fully qualified Android component forms during readback, but it still sends the fully qualified IME identifier into the mutating `ime enable` and `ime set` commands. Waydroid expects the short form that `ime list -a` reports.
