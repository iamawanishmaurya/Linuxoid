# 05-01 Summary

## Result

Linuxoid now binds the resolved `org.futo.inputmethod.latin/.uix.settings.SettingsActivity` session to one deterministic `window_manager` target on the `launch-apk --window-proof` path.

## What changed

- Reused the existing APK session identity to keep `activity_component`, `process_identity`, `window_id`, and `surface_session_id` tied together.
- Preserved exact upstream native blockers in the window contract instead of collapsing them into generic `launch_not_ready`.
- Let the surface proof keep best-effort Wayland/EGL probe truth even when launch is blocked before a visible app window can proceed.

## Verified truth

- Real keyboard path now reports `blocking_reason: native_dlopen_failed:libandroidx.graphics.path.so`.
- Real keyboard path now keeps `backing_mode`, `wayland_surface_available`, and `egl_surface_available` visible in the same window proof.
