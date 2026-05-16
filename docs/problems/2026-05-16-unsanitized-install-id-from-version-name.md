# Problem: unsanitized-install-id-from-version-name

- Date: 2026-05-16
- Exact error:

```text
Reviewer finding: valid Android version names such as "1.0 beta 1" or "1.0+1" can produce install IDs that violate the path validator.
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Build an install ID from a version name containing spaces or `+`.
  3. Pass that install ID into the package layout validator.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Compiler: `g++ 16.1.1`
  - CMake: `4.3.2`
  - Date: 2026-05-16

- First hypothesis:
  `BuildInstallId` uses the human-facing `versionName` directly, but the package-layout validator only accepts filesystem-safe characters.
