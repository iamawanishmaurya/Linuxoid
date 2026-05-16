# Problem: Missing provisioning guards

- Date: 2026-05-16
- Slug: missing-provisioning-guards

## Exact Error

```text
/home/astra/codex/wine-for-android/tests/test_main.cpp: In function ‘void {anonymous}::TestImeProvisioningRejectsWrongApkPackagePairing()’:
/home/astra/codex/wine-for-android/tests/test_main.cpp:490:53: error: too many arguments to function ‘wfa::AdbProvisioningReport wfa::ProvisionAdbImeWithRunner(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&, const CommandRunner&)’
/home/astra/codex/wine-for-android/tests/test_main.cpp: In function ‘void {anonymous}::TestImeProvisioningKeepsPartialEvidenceOnReadbackFailure()’:
/home/astra/codex/wine-for-android/tests/test_main.cpp:542:18: error: ‘const struct wfa::AdbProvisioningReport’ has no member named ‘readback_ok’
/home/astra/codex/wine-for-android/tests/test_main.cpp:546:17: error: ‘const struct wfa::AdbProvisioningReport’ has no member named ‘readback_error’
make[2]: *** [CMakeFiles/wfa_tests.dir/build.make:79: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o] Error 1
make[1]: *** [CMakeFiles/Makefile2:1081: CMakeFiles/wfa_tests.dir/all] Error 2
make: *** [Makefile:101: all] Error 2
```

## Reproduction Steps

1. Extend the provisioning tests to require wrong-APK mismatch detection and non-throwing readback failure reporting.
2. Run `cmake --build build` from `/home/astra/codex/wine-for-android`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Toolchain: `cmake`, `g++`, local `build/` tree
- Date: 2026-05-16

## First Hypothesis

The provisioning report does not yet carry enough state to prove the requested APK matches the expected package name or to preserve partial evidence when the post-action readback query fails.
