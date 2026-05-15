# Problem: unsupported-subagent-model

- Date: 2026-05-16
- Exact error:

```text
{"type":"error","status":400,"error":{"type":"invalid_request_error","message":"The 'gpt-5.3-codex-spark' model is not supported when using Codex with a ChatGPT account."}}
```

- Reproduction steps:
  1. Spawn a subagent role whose pinned model is `gpt-5.3-codex-spark`.
  2. Use the current ChatGPT-account-backed Codex session.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Date: 2026-05-16

- First hypothesis:
  Some specialized subagent roles are tied to a model that is unavailable in this account context, so those tasks must be reassigned to compatible default or supported agent types.
