# Problem: adb-ime-status-throws-away-partial-status

- Date: 2026-05-16
- Exact error:

```text
Reviewer finding: adb-ime-status always launches settings and throws the whole query away if the launch step fails.
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Query `adb-ime-status` with a wrong settings component.
  3. Observe that the launch step can abort the command before the already-known install and IME facts are returned.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Compiler: `g++ 16.1.1`
  - CMake: `4.3.2`
  - Date: 2026-05-16

- First hypothesis:
  The runtime bridge treats the launch check as mandatory and fatal, even though the earlier ADB queries already contain useful status.
