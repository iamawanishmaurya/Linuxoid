# Phase 13 Plan 03 Summary

## Outcome

Phase 13 wave 3 locked the new keyboard APK checkpoint into tests, live smoke, repo docs, and operator status.

## Live keyboard truth

Command used:

```sh
TMPDIR=/home/astra/codex/wine-for-android/.tmp timeout 25 ./build/compatctl launch-apk --window-proof --runtime-proof --first-app-start-proof --self-heal-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk /home/astra/codex/wine-for-android/.tmp/phase13-post
```

Observed stable checkpoint:

- `native_loading_state: managed_activity_post_dispatch_blocked`
- `native_post_dispatch_blocker: framework-boundary-unimplemented:Landroidx/activity/ComponentActivity;->onCreate(Landroid/os/Bundle;)V`
- `first_app_start_health: ready`
- `first_android_app_start.first_executed_opcode: invoke-super`
- `primary_blocker_reason: framework-boundary-unimplemented:Landroidx/activity/ComponentActivity;->onCreate(Landroid/os/Bundle;)V`
- `recommended_next_action: extend_runtime_context_bridge`

## Verification

- `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build`
- `TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure`

## Next handoff

Phase 14 should stay narrow and bridge `ComponentActivity.onCreate(Landroid/os/Bundle;)V` inside the Linuxoid-managed runtime context before attempting broader visible-launch work.
