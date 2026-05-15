# Solution: optimistic-full-use-phase-for-advanced-runtime-apps

- Problem file: [2026-05-16-optimistic-full-use-phase-for-advanced-runtime-apps.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-optimistic-full-use-phase-for-advanced-runtime-apps.md)
- What failed:
  The assessment logic reported non-IME apps as fully usable in `P6` even when background services, boot-complete handling, or secondary processes clearly required more runtime support.

- What worked:
  The assessment now sends those apps to `POST_P6_ADVANCED_RUNTIME` and adds an explicit blocker for background-service lifecycle handling.

- Why it worked:
  The readiness phase now matches the actual unsupported features instead of contradicting them.

- All commands run:

```text
cmake --build build && ctest --test-dir build --output-on-failure
cmake --build build && ctest --test-dir build --output-on-failure && ./build/compatctl assess-manifest /tmp/keyboard-apktool/AndroidManifest.xml
```
