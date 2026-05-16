## Exact Error

```text
Matrix Loading: [----------] 0/100
Runtime Backend: waydroid
Artifact Root: /tmp/linuxoid-matrix
Packages Passed: 0/3
  com.android.calculator2 [###-------] 33/100 fail
  com.android.settings [###-------] 33/100 fail
  org.fdroid.fdroid [###-------] 33/100 fail
```

## Reproduction Steps

1. Build the current Linuxoid tree with `cmake --build build`.
2. Run `ctest --test-dir build --output-on-failure`.
3. Run `./build/compatctl verify-waydroid-matrix /tmp/linuxoid-matrix com.android.calculator2 com.android.settings org.fdroid.fdroid`.
4. Observe that every package fails at `33/100` even though the single-package verifier had already passed for the same apps.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Project: Linuxoid
- Runtime backend: Waydroid
- Date: 2026-05-16

## First Hypothesis

The matrix wrapper is likely reusing the single-package verifier incorrectly, so one of the three subchecks is failing systematically across every package. The most likely candidates are generated-launcher execution or artifact-root handling, because direct launch already works for these packages when run one by one.
