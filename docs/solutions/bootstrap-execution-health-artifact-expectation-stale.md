# Solution: Bootstrap execution health artifact expectation stale

Linked problem: [2026-05-17-bootstrap-execution-health-artifact-expectation-stale.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-bootstrap-execution-health-artifact-expectation-stale.md)

## What failed

Once the new bootstrap-execution seam was wired into runtime health, one older regression still expected the `activity_bootstrap_readiness` subsystem to point at:

- `art/activity-bootstrap-result.json`

That expectation was stale because the health contract had moved up one seam. Runtime health was now supposed to point at:

- `art/bootstrap-execution-result.json`

## What worked

Linuxoid now updates the activity-bootstrap health subsystem to point at the bootstrap-execution artifact, and the tests were updated to match that contract.

## Why it worked

The health subsystem should point at the artifact that best represents the currently bounded execution step. After the bootstrap-execution seam was added, that step was no longer plain activity planning. Updating the artifact target kept the contract truthful and avoided splitting the same runtime meaning across two result files.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl native-runtime-health-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline
```
