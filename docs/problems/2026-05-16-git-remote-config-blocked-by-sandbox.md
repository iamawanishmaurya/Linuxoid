## Exact Error

```text
error: could not lock config file .git/config: Read-only file system
fatal: could not set 'remote.origin.url' to 'https://github.com/iamawanishmaurya/Linuxoid.git'
```

## Reproduction Steps

1. Open `/home/astra/codex/wine-for-android`.
2. Run `git remote add origin https://github.com/iamawanishmaurya/Linuxoid.git`.
3. Observe the failure while Git tries to update `.git/config`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Sandbox mode: workspace-write
- Date: 2026-05-16

## First Hypothesis

The workspace sandbox allows normal file edits in the working tree but blocks writes to Git metadata under `.git`, so updating the remote configuration needs an escalated Git command.
