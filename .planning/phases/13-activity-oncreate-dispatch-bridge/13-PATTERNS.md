# Phase 13 Patterns

## Existing patterns to reuse

### Seam narrowing

- `src/native_execute_stub.cpp` narrows native and managed launch blockers one real seam at a time.
- `src/apk_native_launch.cpp` mirrors those seams into top-level launch JSON, nested first-app-start JSON, and window/runtime/session reports.
- `src/apk_self_healing_watchdog.cpp` keeps the earliest blocker authoritative and recovery wording stable.

### Managed execution reporting

- `src/apk_dex_bridge.cpp` already reports exact class-loading, method lookup, receiver, bundle placeholder, and framework-boundary truth for deterministic managed checkpoints.
- The first-app-start proof already keeps class, method, receiver, and bundle state explicit for the keyboard-identity path.

### Window truth

- `src/apk_window_bridge.cpp` already keeps `visible_target_state`, `wayland_surface_available`, and `egl_surface_available` honest.
- Window proof should consume the new post-dispatch seam, not invent a separate launch truth.

### Regression style

- `tests/test_main.cpp` pins exact blocker strings and JSON fields.
- Offline JNI and keyboard-identity fixtures remain the preferred regression surface.

## Planning constraints

Phase 13 is about moving from bound runtime context into lifecycle dispatch, not about full visible launch or IME behavior.

Keep these seams distinct:

1. JNI registration completion
2. Linuxoid runtime-context binding
3. `Activity.onCreate(Bundle)` dispatch
4. Post-dispatch framework, resource, or visible-surface blocker

## Files most likely to change

- `src/native_execute_stub.cpp`
- `include/wfa/native_execute_stub.hpp`
- `src/apk_dex_bridge.cpp`
- `include/wfa/apk_dex_bridge.hpp`
- `src/apk_native_launch.cpp`
- `include/wfa/apk_native_launch.hpp`
- `src/apk_window_bridge.cpp`
- `src/apk_self_healing_watchdog.cpp`
- `tests/test_main.cpp`
