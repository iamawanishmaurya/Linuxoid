# Problem: agent-thread-limit-reached-native-runtime-wave

- Timestamp: 2026-05-16T14:10:45+05:30
- Environment: Codex desktop app, repository `/home/astra/codex/wine-for-android`, branch `main`, full-access mode, network enabled

## Exact Error

```text
collab spawn failed: agent thread limit reached
```

## Reproduction Steps

1. Spawn several focused subagents in parallel for planning, research, verification, and testing.
2. Observe three agents start successfully.
3. Attempt to spawn additional agents in the same wave.
4. Observe the platform reject the remaining spawn calls with the thread-limit error.

## First Hypothesis

The current thread already has enough active subagents to hit the platform concurrency cap. The fix likely requires a different dispatch pattern such as closing or reusing existing agents, waiting for one wave to finish before spawning the next, or reducing the number of simultaneously active workers.
