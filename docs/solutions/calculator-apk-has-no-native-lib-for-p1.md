# Solution: calculator-apk-has-no-native-lib-for-p1

Problem reference: [2026-05-16-calculator-apk-has-no-native-lib-for-p1.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-calculator-apk-has-no-native-lib-for-p1.md)

## What Failed

The draft `P1` plan assumed `com.android.calculator2` shipped `libcalculator.so` and exposed `ANativeActivity_onCreate`. The actual staged APK on disk does not contain any native library payload.

## Options Evaluated

1. Keep treating the current Calculator APK as the literal `P1` gate app.
   - Pros: matches the draft plan text.
   - Cons: impossible to satisfy with the current artifact because there is no `.so` to load.

2. Stop implementation and wait for a different APK before writing code.
   - Pros: avoids building the runner against a placeholder.
   - Cons: stalls `P1` progress even though the runner, signal handling, JNI, asset, and looper plumbing can be built and tested now.

3. Build the `P1` native execution core against a tiny Linuxoid-owned fixture library, while keeping the current Calculator APK as a negative oracle for “no native library present.”
   - Pros: lets Linuxoid implement and verify the runner honestly now, preserves a real failure check for dex-only APKs, and keeps the architecture aligned with the future direct-execution path.
   - Cons: the final gate app still needs to be swapped to a real native Android target later.

## What Worked

Option 3 is the best near-term path. Linuxoid can:

- detect and report that the current Calculator APK contains no native library payload
- implement the real `P1` runner against a minimal shared-library fixture that exports `ANativeActivity_onCreate`
- keep Calculator as the negative regression oracle until a true native gate app is staged

## Why It Worked

It separates two different truths cleanly:

- the current Calculator APK is not suitable for the literal NDK gate
- Linuxoid still needs the real native execution machinery, and that machinery can be built and tested without pretending the wrong artifact is native

## Commands Run

```bash
unzip -l /tmp/linuxoid-native-calculator.apk
zipinfo -1 /tmp/linuxoid-native-calculator.apk | rg '^lib/.+\.so$|^assets/'
```
