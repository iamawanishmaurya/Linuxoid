# Solution: Activity launch fixture missing component evidence

Related problem: [2026-05-16-activity-launch-fixture-missing-component-evidence.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-activity-launch-fixture-missing-component-evidence.md)

## What Failed

The new explicit-component launch bridge required launch output to confirm the requested component, but the original unit-test fixture only returned generic success lines.

## What Worked

The unit-test fixture was updated to return realistic `am start -W -n` output with `cmp=<component>` evidence, which matches the stricter behavior of the new launch bridge.

## Why It Worked

The stricter bridge is supposed to fail closed unless the requested explicit component is what Android actually acknowledged. Updating the fixture made the test reflect the intended runtime contract instead of weakening the code.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```
