# Solution: Application Bootstrap Infers Non-Application Target

Problem: [2026-05-17-application-bootstrap-infers-non-application-target.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-application-bootstrap-infers-non-application-target.md)

## What failed

The first application-bootstrap implementation chose the first non-activity target from the classloader target set when the APK did not declare `application android:name`. On the staged Calculator bundle, that incorrectly surfaced `com.android.calculator2.Licenses` as the application class.

## What worked

- Added APK manifest inspection to the activity-bootstrap fixture.
- Normalized the manifest application class directly when present.
- Left `selected_application_class_name` empty when the manifest does not declare one.

## Why it worked

An application bootstrap seam must reflect real manifest intent. Falling back to arbitrary non-activity classes made the artifact look richer than the APK actually is. Using the manifest directly restores truthful bootstrap metadata and keeps replay output stable.

## Commands run

```bash
cd /home/astra/codex/wine-for-android
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl native-art-activity-bootstrap-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
```
