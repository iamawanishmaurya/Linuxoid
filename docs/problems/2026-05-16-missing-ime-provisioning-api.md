# Problem: Missing IME provisioning API

- Date: 2026-05-16
- Slug: missing-ime-provisioning-api

## Exact Error

```text
/home/astra/codex/wine-for-android/tests/test_main.cpp: In function ‘void {anonymous}::TestImeProvisioningSuccessPath()’:
/home/astra/codex/wine-for-android/tests/test_main.cpp:344:63: error: ‘CommandResult’ in namespace ‘wfa’ does not name a type
/home/astra/codex/wine-for-android/tests/test_main.cpp:373:28: error: ‘ProvisionAdbImeWithRunner’ is not a member of ‘wfa’
/home/astra/codex/wine-for-android/tests/test_main.cpp:390:30: error: ‘RenderAdbProvisioningReport’ is not a member of ‘wfa’
/home/astra/codex/wine-for-android/tests/test_main.cpp: In function ‘void {anonymous}::TestImeProvisioningDetectsIncompleteActivation()’:
/home/astra/codex/wine-for-android/tests/test_main.cpp:396:62: error: ‘CommandResult’ in namespace ‘wfa’ does not name a type
/home/astra/codex/wine-for-android/tests/test_main.cpp:421:28: error: ‘ProvisionAdbImeWithRunner’ is not a member of ‘wfa’
/home/astra/codex/wine-for-android/tests/test_main.cpp:431:30: error: ‘RenderAdbProvisioningReport’ is not a member of ‘wfa’
make[2]: *** [CMakeFiles/wfa_tests.dir/build.make:79: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o] Error 1
make[1]: *** [CMakeFiles/Makefile2:1081: CMakeFiles/wfa_tests.dir/all] Error 2
make: *** [Makefile:101: all] Error 2
```

## Reproduction Steps

1. Edit `tests/test_main.cpp` to add IME provisioning tests.
2. Run `cmake --build build` from `/home/astra/codex/wine-for-android`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Toolchain: `cmake`, `g++`, local `build/` tree
- Date: 2026-05-16

## First Hypothesis

The tests describe the next runtime-bridge slice, but the production header and source do not yet expose a reusable command result type, a provision-and-verify orchestration function, or a rendered provisioning report.
