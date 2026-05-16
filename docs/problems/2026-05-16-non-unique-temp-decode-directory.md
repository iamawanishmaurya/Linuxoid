# Problem: non-unique-temp-decode-directory

- Date: 2026-05-16
- Exact error:

```text
Reviewer finding: the decode root is derived from filename and size, so same-name/same-size loads can clobber each other and leave temp data behind.
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Start two loads for the same APK name and size at nearly the same time.
  3. Observe that both use the same temp decode root.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Compiler: `g++ 16.1.1`
  - CMake: `4.3.2`
  - Date: 2026-05-16

- First hypothesis:
  The decode temp directory needs a unique per-invocation path and explicit cleanup.
