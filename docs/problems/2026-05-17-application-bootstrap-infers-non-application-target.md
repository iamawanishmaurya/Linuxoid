# Application Bootstrap Infers Non-Application Target

- Exact error:
  - Live `native-art-activity-bootstrap-fixture` output for the staged Calculator bundle reported:
    - `"selected_application_class_name": "com.android.calculator2.Licenses"`
  - The value came from the first non-activity manifest target rather than an actual manifest `application android:name`.
- Reproduction steps:
  1. `cd /home/astra/codex/wine-for-android`
  2. `./build/compatctl native-art-activity-bootstrap-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json`
- Environment:
  - Host: Codex desktop local workspace
  - Date: 2026-05-17
  - OS/runtime: Linux, C++20 build
- First hypothesis:
  - The new application-bootstrap seam is selecting the first non-activity class from `target_class_names` when the manifest does not declare an application class. Linuxoid should instead normalize the manifest application class directly and leave the field empty when no application class is declared.
