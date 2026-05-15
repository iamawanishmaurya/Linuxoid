# Solution: outdated-phase-progress-test-expectation

- Problem file: [2026-05-16-outdated-phase-progress-test-expectation.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-outdated-phase-progress-test-expectation.md)
- What failed:
  The tests still expected the old average phase-loading value after Phase 4 was raised from `25` to `45`.

- What worked:
  Updating the test expectation from `54` to `58` aligned the assertion with the new verified phase model.

- Why it worked:
  The implementation had already been updated correctly. The failure was only stale test data.

- All commands run:

```text
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure && apktool d -f -s -o /tmp/keyboard-apktool /home/astra/Downloads/keyboard-0.1.28.apk >/tmp/keyboard-apktool.log && ./build/compatctl status && ./build/compatctl assess-manifest /tmp/keyboard-apktool/AndroidManifest.xml
```
