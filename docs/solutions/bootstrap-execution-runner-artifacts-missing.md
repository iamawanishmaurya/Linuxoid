# Bootstrap execution runner artifacts missing

## What failed

The bootstrap-execution fixture tests expected runner-facing artifacts, but `NativeArtBootstrapExecutionFixtureReport` and its writer logic only exposed plan, trace, and result files.

## What worked

We extended the bootstrap-execution report and implementation to write and expose:

- `art/bootstrap-execution-context.json`
- `art/bootstrap-execution-runner.sh`
- `art/bootstrap-execution-application.log`
- `art/bootstrap-execution-activity.log`

We also made `native-art-activity-bootstrap-fixture` the planning seam and kept `native-art-bootstrap-execution-fixture` as the execution seam.

## Why it worked

The execution fixture now owns the artifacts a harness needs to inspect or replay the next bootstrap step without blurring planning and execution together. That makes the self-healing runtime more diagnosable and keeps the runtime-health model aligned with the real artifact surfaces.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl native-art-activity-bootstrap-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl native-art-bootstrap-execution-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
```

## Linked problem

- [Bootstrap execution runner artifacts missing](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-bootstrap-execution-runner-artifacts-missing.md)
