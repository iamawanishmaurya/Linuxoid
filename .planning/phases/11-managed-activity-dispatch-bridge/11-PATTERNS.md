# Phase 11 Patterns

## Existing patterns to reuse

### Native seam narrowing

- `src/native_execute_stub.cpp` narrows native startup blockers one seam at a time.
- `src/apk_native_launch.cpp` mirrors those seams into top-level launch JSON and nested first-app-start proof JSON.
- `src/apk_self_healing_watchdog.cpp` keeps the earliest blocker authoritative and recovery wording stable.

### Managed seam reporting

- `src/apk_dex_bridge.cpp` already reports exact class-loading, method lookup, and framework-boundary truth for the deterministic managed checkpoints.
- The first-app-start proof already keeps class, method, receiver, and bundle placeholder state explicit.

### Regression style

- `tests/test_main.cpp` pins exact blocker strings and JSON fields.
- Offline JNI fixtures are preferred over external dependencies.

## Planning constraints

Phase 11 is about managed activity dispatch after JNI registration, not about broad framework completion.

Keep these seams distinct:

1. JNI registration completion
2. Linuxoid-managed activity dispatch selection
3. Runtime-context binding
4. Framework boundary or `Activity.onCreate(Bundle)` dispatch

## Files most likely to change

- `src/native_execute_stub.cpp`
- `include/wfa/native_execute_stub.hpp`
- `src/apk_native_launch.cpp`
- `include/wfa/apk_native_launch.hpp`
- `src/apk_dex_bridge.cpp`
- `include/wfa/apk_dex_bridge.hpp`
- `src/apk_self_healing_watchdog.cpp`
- `tests/test_main.cpp`

