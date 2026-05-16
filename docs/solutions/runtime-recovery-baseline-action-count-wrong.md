# Runtime recovery baseline action count wrong

Problem: [2026-05-17-runtime-recovery-baseline-action-count-wrong.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-runtime-recovery-baseline-action-count-wrong.md)

## What failed

The new baseline recovery-plan test assumed the fixture would always produce two recovery actions. That was not true: the baseline fixture already has native loading ready, so only the DEX/ART recovery action is guaranteed.

## What worked

The test was corrected to assert the real invariant:

- at least one deterministic baseline recovery action exists
- the DEX/ART recovery action exists
- its artifact and replay-trace paths are stable

## Why it worked

Linuxoid's baseline recovery surface is scenario-sensitive. The native-loading recovery action appears only when native loading is actually blocked. Tightening the assertion to the true contract preserves deterministic coverage without baking in a false assumption about unrelated subsystems.

## Commands run

```bash
cd /home/astra/codex/wine-for-android
cmake --build build && ctest --test-dir build --output-on-failure
```
