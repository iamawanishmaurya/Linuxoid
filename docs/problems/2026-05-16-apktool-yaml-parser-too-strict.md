# Problem: apktool-yaml-parser-too-strict

- Date: 2026-05-16
- Exact error:

```text
Test failure: apktool metadata is missing required version fields
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Parse an `apktool.yml` fixture where `versionCode` and `versionName` appear under `versionInfo:` with indentation.
  3. Run `ctest --test-dir build --output-on-failure`.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Compiler: `g++ 16.1.1`
  - CMake: `4.3.2`
  - Date: 2026-05-16

- First hypothesis:
  The current parser expects keys at column zero, but real `apktool.yml` uses indentation for nested sections, so required version fields are being missed.
