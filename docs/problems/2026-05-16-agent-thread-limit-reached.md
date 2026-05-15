# Problem: agent-thread-limit-reached

- Date: 2026-05-16
- Exact error:

```text
collab spawn failed: agent thread limit reached
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Attempt to spawn more concurrent subagents than the current session allows.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Date: 2026-05-16

- First hypothesis:
  The Codex session enforces a maximum number of concurrently active subagent threads, so the remaining research tasks must be dispatched in later waves after existing agents complete or are closed.
