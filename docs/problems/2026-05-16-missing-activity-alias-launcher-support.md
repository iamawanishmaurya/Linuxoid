# Problem: missing-activity-alias-launcher-support

- Date: 2026-05-16
- Exact error:

```text
Test failure: expected launcher activity through activity-alias
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Add a decoded manifest test case where the launcher entry is defined on `activity-alias`.
  3. Run `ctest --test-dir build --output-on-failure`.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Compiler: `g++ 16.1.1`
  - CMake: `4.3.2`
  - Date: 2026-05-16

- First hypothesis:
  The manifest parser only scans `<activity>` blocks, so it misses launcher components published through `<activity-alias>`.
