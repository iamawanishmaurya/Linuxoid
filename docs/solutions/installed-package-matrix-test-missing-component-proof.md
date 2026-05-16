# Solution: installed package matrix test missing component proof

- Related problem: [2026-05-16-installed-package-matrix-test-missing-component-proof](../problems/2026-05-16-installed-package-matrix-test-missing-component-proof.md)

## What failed

The new attached-ADB generic matrix unit test reported:

```text
Test failure: expected first generic matrix package to pass
```

The first implementation attempt improved the mocked launch output, but the next `ctest` invocation was started before the rebuild of `wfa_tests` had finished, so the same failure symptom appeared a second time from a stale binary.

## Distinct solutions considered

1. Weaken `LaunchOutputConfirmsComponent` so `Status: ok` is enough.
   - Rejected: this would make the runtime bridge less honest and would undercut the stricter attached-ADB contract we already rely on elsewhere.

2. Keep the parser strict, but fix the test fixture so mocked `am start -W` output includes the launched component.
   - Strong candidate: matches the real Android output shape and preserves the runtime contract.

3. Add extra debug fields to the matrix report and rewrite the test around those fields.
   - Useful later, but too invasive for this regression because the failure already had a likely cause.

4. Serialize rebuild and `ctest` verification so the freshly edited test binary is always the one being executed.
   - Strong candidate: fixes the validation workflow issue that caused the repeated symptom after the first patch.

## What worked

The best fix was a combination of solutions 2 and 4:

- update the attached-ADB generic matrix fixture so each mocked launch result includes a concrete `cmp=...` component line
- rebuild `wfa_tests` to completion before running `ctest`

## Why it worked

`LaunchInstalledAppWithRunner` intentionally requires successful `am start -W` output to confirm the launched component. The matrix fixture originally returned only `Status: ok` and `Complete`, which was not enough to satisfy that rule. After the fixture was corrected, the remaining repeated failure came from running `ctest` against an old binary while the rebuild was still in progress. Serializing the validation step removed that workflow false negative.

## Commands run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl verify-package waydroid com.android.calculator2 - - /tmp/linuxoid-generic-applications /tmp/linuxoid-generic-launchers
./build/compatctl verify-package-matrix waydroid /tmp/linuxoid-generic-matrix - com.android.calculator2 com.android.settings org.fdroid.fdroid
```
