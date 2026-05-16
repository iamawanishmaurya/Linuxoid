# Native Spike Planner Rejects Normal Multi-Activity Apps

## Exact Error

The first live native spike verification against Calculator produced a false blocker:

```text
Package: com.android.calculator2
Install ID: vc33-13
Launcher Component: com.android.calculator2/.Calculator
Min SDK: 33
Target SDK: 33
Native Spike Candidate: no
Staged APK: /tmp/linuxoid-native-compat/users/0/packages/com.android.calculator2/vc33-13/base.apk
Package Root: /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13
Bundle Root: /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bundle
Sandbox Root: /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/sandbox
DEX Cache Root: /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/dex-cache
Resource Root: /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/resources
Library Root: /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/lib
Bootstrap Spec: /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/native-plan.json
Plan Written: yes
Blockers:
  - Native spike target must expose exactly one declared activity.
Next Steps:
  - Keep using Waydroid and attached-target verification as regression oracles.
  - Reduce manifest/runtime blockers until the app fits the first native slice.
  - Re-run the native spike planner after trimming app complexity.
```

## Reproduction Steps

1. Connect to the attached Android target.
2. Pull the Calculator APK from the device:
   - `adb -s 192.168.240.112:5555 pull /system/product/app/ExactCalculator/ExactCalculator.apk /tmp/linuxoid-native-calculator.apk`
3. Run the new planner:
   - `./build/compatctl plan-native-spike /tmp/linuxoid-native-calculator.apk /tmp/linuxoid-native-compat /tmp/linuxoid-native-spike`

## Environment

- Project: Linuxoid
- Repository: `/home/astra/codex/wine-for-android`
- Host OS: Linux
- Runtime target: attached ADB target `192.168.240.112:5555`
- Build state: local native spike planner implementation compiled and tests passed before live verification

## First Hypothesis

The native spike gate is too strict. A simple first native slice should accept apps with multiple declared activities as long as Linuxoid can resolve one launcher activity and the package does not require advanced runtime features such as services, IME binding, boot receivers, or secondary processes.
