# Solution: Bootstrap execution fixture surface missing

Linked problem: [2026-05-17-bootstrap-execution-fixture-surface-missing.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-bootstrap-execution-fixture-surface-missing.md)

## What failed

The new bootstrap-execution regression tests expected a dedicated Linuxoid surface:

- `include/wfa/art_bootstrap_execution_fixture.hpp`
- `src/art_bootstrap_execution_fixture.cpp`
- `compatctl native-art-bootstrap-execution-fixture`

The build failed immediately because none of those surfaces existed yet.

## What worked

Linuxoid now has a dedicated bootstrap-execution fixture that:

- builds deterministic execution-plan, trace, and result artifacts
- reuses the lower-level activity-bootstrap evidence instead of inventing a new parallel seam
- exposes a stable CLI command:
  - `native-art-bootstrap-execution-fixture <bootstrap-manifest>`

## Why it worked

The existing activity-bootstrap seam already contained the right execution evidence, but it was still implicit. Promoting that evidence into its own fixture gave Linuxoid a stable place to talk about the first host-side bootstrap execution attempt without pretending ART-owned app execution is already complete.

That also gave runtime health and replay a cleaner contract, because they can now point at a specific bootstrap-execution artifact instead of overloading the activity-bootstrap result for two different meanings.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl native-art-bootstrap-execution-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
```
