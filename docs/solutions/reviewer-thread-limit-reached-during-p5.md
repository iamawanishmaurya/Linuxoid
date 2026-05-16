# Solution: Reviewer thread limit reached during P5

Related problem: [2026-05-16-reviewer-thread-limit-reached-during-p5.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-reviewer-thread-limit-reached-during-p5.md)

## What Failed

Spawning a fresh reviewer subagent during the P5 closeout failed with `agent thread limit reached`.

## Distinct Options Considered

1. Reuse an existing completed reviewer subagent already in the thread.
   - Pros: no new thread needed, minimal disruption, keeps review context tight.
   - Cons: depends on the old reviewer still being resumable.

2. Close idle subagents and then spawn a fresh reviewer.
   - Pros: restores the original workflow shape.
   - Cons: more moving pieces, higher risk of wasting time on thread bookkeeping.

3. Skip subagents and perform the review locally only.
   - Pros: fastest path.
   - Cons: loses the independent second set of eyes we wanted for this slice.

4. Wait for capacity and retry later.
   - Pros: simple.
   - Cons: stalls progress for no product reason.

## What Worked

The best option was to reuse an existing reviewer-capable subagent instead of spawning a new one. That avoided the thread-limit failure entirely while preserving an independent review pass.

## Why It Worked

The problem was thread capacity, not missing review capability. Reusing an existing reviewer consumed no extra thread slot and kept the workflow aligned with the original quality gate.

## Commands Run

```text
No shell command was required. The recovery used the existing subagent thread instead of spawning a new one.
```
