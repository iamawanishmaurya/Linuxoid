# Problem: Missing enabled-IME verification

- Date: 2026-05-16
- Slug: missing-enabled-ime-verification

## Exact Error

```text
/home/astra/codex/wine-for-android/tests/test_main.cpp: In function ‘void {anonymous}::TestRuntimeBridgeOutputParsers()’:
/home/astra/codex/wine-for-android/tests/test_main.cpp:323:15: error: ‘EnabledInputMethodsContainIme’ is not a member of ‘wfa’
/home/astra/codex/wine-for-android/tests/test_main.cpp:327:16: error: ‘EnabledInputMethodsContainIme’ is not a member of ‘wfa’
/home/astra/codex/wine-for-android/tests/test_main.cpp:347:3: error: ‘wfa::AdbImeStatus’ has no non-static data member named ‘ime_enabled’
/home/astra/codex/wine-for-android/tests/test_main.cpp: In function ‘void {anonymous}::TestImeProvisioningSuccessPath()’:
/home/astra/codex/wine-for-android/tests/test_main.cpp:404:30: error: ‘const struct wfa::AdbImeStatus’ has no member named ‘ime_enabled’
make[2]: *** [CMakeFiles/wfa_tests.dir/build.make:79: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o] Error 1
make[1]: *** [CMakeFiles/Makefile2:1081: CMakeFiles/wfa_tests.dir/all] Error 2
make: *** [Makefile:101: all] Error 2
```

## Reproduction Steps

1. Extend the runtime-bridge tests to require an explicit enabled-IME check.
2. Run `cmake --build build` from `/home/astra/codex/wine-for-android`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Toolchain: `cmake`, `g++`, local `build/` tree
- Date: 2026-05-16

## First Hypothesis

The runtime bridge currently proves IME registration and default selection, but it does not yet model the secure `enabled_input_methods` setting or expose an exact parser for that list.
