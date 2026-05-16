# Problem: Component-form mismatch in wrapper provisioning

- Date: 2026-05-16
- Slug: component-form-mismatch-in-wrapper-provisioning

## Exact Error

```text
Provisioning Loading: [#######---] 70/100
ADB Serial: emulator-5590
APK Path: /home/astra/Downloads/keyboard-0.1.28.apk
Package: org.futo.inputmethod.latin
APK Declared Package: org.futo.inputmethod.latin
APK package match: yes
IME ID: org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME
Settings Component: org.futo.inputmethod.latin/.uix.settings.SettingsActivity
Install OK: yes
Enable OK: yes
Set Default OK: yes
Readback OK: yes
Package installed: yes
IME registered: no
IME enabled: no
Enabled IMEs: com.google.android.inputmethod.latin/com.android.inputmethod.latin.LatinIME:com.google.android.googlequicksearchbox/com.google.android.voicesearch.ime.VoiceInputMethodService:org.futo.inputmethod.latin/.LatinIME
Default IME: org.futo.inputmethod.latin/.LatinIME
Default matches target: no
Settings launch OK: yes
Ready for typing: no
```

## Reproduction Steps

1. Build the current P5 host-launch slice.
2. Run `./build/compatctl desktopify-apk emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin/.uix.settings.SettingsActivity /tmp/wfa-load /tmp/wfa-desktop`.
3. Run the generated wrapper script `/tmp/wfa-desktop/org.futo.inputmethod.latin.sh`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Toolchain: `cmake`, `g++`, local `build/` tree
- Live target: `emulator-5590`
- Date: 2026-05-16

## First Hypothesis

The generated wrapper uses the fully qualified IME component form, while the live Android runtime reports and stores the same IME using the short component form. The current exact-match logic treats those as different values even though they point to the same component.
