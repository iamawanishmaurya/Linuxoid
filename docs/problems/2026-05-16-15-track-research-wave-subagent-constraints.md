# Problem: 15-track-research-wave-subagent-constraints

## Exact Errors

### Unsupported pinned-role model

```json
{"type":"error","status":400,"error":{"type":"invalid_request_error","message":"The 'gpt-5.3-codex-spark' model is not supported when using Codex with a ChatGPT account."}}
```

### Thread-cap failure during bulk spawn

```text
collab spawn failed: agent thread limit reached
```

## Reproduction Steps

1. Start a 15-track Linuxoid research wave with one spawn request per architecture gap.
2. Include specialized roles that are pinned to `gpt-5.3-codex-spark`.
3. Attempt to launch the full set in one burst.
4. Observe:
   - pinned-role model failures for incompatible roles
   - repeated thread-limit failures once active agent capacity is exhausted

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Date: `2026-05-16`
- Context: Linuxoid 15-track research dispatch
- Account mode: ChatGPT account-backed Codex session

## First Hypothesis

Two independent platform constraints are interacting:

1. some specialized roles are pinned to a model unsupported in this account
2. the session has a hard cap on simultaneously active subagent threads

The recovery needs to use compatible agent types and a bounded wave queue instead of another full-burst retry.
