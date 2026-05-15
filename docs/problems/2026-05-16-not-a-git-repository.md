# Problem: not-a-git-repository

- Date: 2026-05-16
- Exact error:

```text
fatal: not a git repository (or any parent up to mount point /)
Stopping at filesystem boundary (GIT_DISCOVERY_ACROSS_FILESYSTEM not set).
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Run `git status --short --branch`.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Date: 2026-05-16

- First hypothesis:
  The workspace has not been initialized with `git init`, so any Git status or commit workflow will fail until a repository is created.
