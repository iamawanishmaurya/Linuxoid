Related problem: [2026-05-16-fully-qualified-ime-id-rejected-by-waydroid.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-fully-qualified-ime-id-rejected-by-waydroid.md)

## What Failed

Linuxoid sent fully qualified IME identifiers like `org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME` into `adb shell ime enable` and `adb shell ime set`. Waydroid reported the IME in short form and rejected the long form for mutation.

## What Worked

- Added short-form IME normalization before the mutating `ime enable` and `ime set` commands.
- Added a Waydroid-style regression test that fails when the long form is sent.
- Re-ran the Linuxoid-generated Waydroid launcher and reached `Ready for typing: yes`.

## Why It Worked

Android component forms are semantically equivalent for identity checks, but the IME mutation commands on Waydroid were strict about the exact registry form. Normalizing the mutation path to `package/.Class` made Linuxoid speak the same identifier format that `ime list -a` exposes.

## Commands Run

```bash
adb -s 192.168.240.112:5555 shell ime enable org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME
adb -s 192.168.240.112:5555 shell ime set org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME
adb -s 192.168.240.112:5555 shell ime list -a
cmake --build build
ctest --test-dir build --output-on-failure
/tmp/linuxoid-launchers-waydroid/org.futo.inputmethod.latin.sh
```
