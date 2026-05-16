# Problem: unsupported-testing-subagent-model-native-runtime-wave

- Timestamp: 2026-05-16T14:17:31+05:30
- Environment: Codex desktop app, repository `/home/astra/codex/wine-for-android`, branch `main`, full-access mode, network enabled

## Exact Error

```json
{"type":"error","status":400,"error":{"type":"invalid_request_error","message":"The 'gpt-5.3-codex-spark' model is not supported when using Codex with a ChatGPT account."}}
```

## Reproduction Steps

1. Spawn a `test-automator` subagent for the native-runtime transition wave.
2. Wait for the agent to initialize.
3. Observe the platform reject the role because its pinned model is unsupported for this account.

## First Hypothesis

The `test-automator` role is bound to a model that is unavailable in this account context. A supported fallback likely requires either a different role, a default unpinned agent, or reusing an already-running supported agent for the testing memo.
