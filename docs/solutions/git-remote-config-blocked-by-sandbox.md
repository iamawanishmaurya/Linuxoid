Related problem: [2026-05-16-git-remote-config-blocked-by-sandbox.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-git-remote-config-blocked-by-sandbox.md)

## What Failed

Adding the Linuxoid GitHub remote from the sandbox failed because Git could not write `.git/config`.

## What Worked

Ran the `git remote add origin https://github.com/iamawanishmaurya/Linuxoid.git` command outside the sandbox.

## Why It Worked

The repository metadata lives under `.git`, and this environment treats that path differently from normal working-tree files. Once the command ran at the host level, Git could write the remote configuration normally.

## Commands Run

```bash
git remote add origin https://github.com/iamawanishmaurya/Linuxoid.git
git remote -v
```
