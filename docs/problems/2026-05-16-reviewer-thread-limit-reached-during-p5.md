# Problem: Reviewer thread limit reached during P5

- Date: 2026-05-16
- Slug: reviewer-thread-limit-reached-during-p5

## Exact Error

```text
collab spawn failed: agent thread limit reached
```

## Reproduction Steps

1. Keep several earlier review/research subagents in the current thread history.
2. Attempt to spawn a fresh reviewer subagent during the P5 host-integration closeout.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Context: Codex desktop thread with existing subagent history
- Date: 2026-05-16

## First Hypothesis

The current thread has reached the active or retained subagent limit, so opening another fresh reviewer is blocked even though the implementation work itself is ready for review.
