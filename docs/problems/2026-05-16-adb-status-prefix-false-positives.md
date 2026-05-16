# Problem: adb-status-prefix-false-positives

- Date: 2026-05-16
- Exact error:

```text
Reviewer finding: substring matching can treat package or IME prefixes as exact matches.
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Feed `OutputContainsInstalledPackage` an output line like `package:com.example.app.debug`.
  3. Query for `com.example.app`.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Compiler: `g++ 16.1.1`
  - CMake: `4.3.2`
  - Date: 2026-05-16

- First hypothesis:
  The runtime bridge currently uses raw substring checks instead of exact token matching on output lines.
