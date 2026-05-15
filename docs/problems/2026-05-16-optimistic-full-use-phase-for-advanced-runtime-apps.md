# Problem: optimistic-full-use-phase-for-advanced-runtime-apps

- Date: 2026-05-16
- Exact error:

```text
Reviewer finding: non-IME apps with unresolved background-service, boot-complete, or multi-process blockers still report Earliest full-use phase: P6.
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Assess a decoded manifest with a launcher activity plus `BOOT_COMPLETED` or a secondary-process service.
  3. Observe that the old logic still reported `Earliest full-use phase: P6` while also listing blockers.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Compiler: `g++ 16.1.1`
  - CMake: `4.3.2`
  - Date: 2026-05-16

- First hypothesis:
  The full-use phase logic only treated IME apps as post-`P6`, so other advanced-runtime requirements were being reported as blockers without affecting the headline readiness phase.
