# Problem: calculator-apk-has-no-native-lib-for-p1

## Exact Error

The staged Calculator APK used for `P1` does not contain any native shared libraries:

```text
$ zipinfo -1 /tmp/linuxoid-native-calculator.apk | rg '^lib/.+\.so$|^assets/'
assets/licenses.html
```

The full archive listing also shows `classes.dex` and Android resources, but no `lib/x86_64/*.so`, `lib/arm64-v8a/*.so`, or `libcalculator.so`.

## Reproduction Steps

1. Use the current staged Calculator APK at `/tmp/linuxoid-native-calculator.apk`.
2. Run:
   - `unzip -l /tmp/linuxoid-native-calculator.apk`
   - `zipinfo -1 /tmp/linuxoid-native-calculator.apk | rg '^lib/.+\\.so$|^assets/'`
3. Observe there is no native shared library payload inside the APK.

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Date: `2026-05-16`
- Host: local Linux workspace
- Planned P1 target: `com.android.calculator2`

## First Hypothesis

The current local `com.android.calculator2` APK is a Java/Kotlin app bundle for this device image, not the pure-`ANativeActivity` NDK target assumed by the draft `P1` plan. Linuxoid therefore cannot satisfy the literal `dlopen(libcalculator.so)` gate with this exact APK.
