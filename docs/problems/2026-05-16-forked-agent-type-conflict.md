# Problem: forked-agent-type-conflict

- Timestamp: 2026-05-16T14:09:23+05:30
- Environment: Codex desktop app, repository `/home/astra/codex/wine-for-android`, branch `main`, full-access mode, network enabled

## Exact Error

```text
Full-history forked agents inherit the parent agent type, model, and reasoning effort; omit agent_type, model, and reasoning_effort, or spawn without a full-history fork.
```

## Reproduction Steps

1. Attempt to spawn a subagent with `fork_context=true`.
2. Also provide an explicit `agent_type` and `reasoning_effort`.
3. Observe that the spawn request is rejected before the agent is created.

## First Hypothesis

The spawn API does not allow role overrides when an agent is created with full-history context inheritance. The fix is likely to either remove `fork_context=true` or remove the explicit role and reasoning overrides.
