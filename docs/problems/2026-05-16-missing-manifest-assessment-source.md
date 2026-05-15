# Problem: missing-manifest-assessment-source

- Date: 2026-05-16
- Exact error:

```text
CMake Error at CMakeLists.txt:15 (add_library):
  Cannot find source file:

    src/manifest_assessment.cpp


CMake Error at CMakeLists.txt:15 (add_library):
  No SOURCES given to target: wfa_core


CMake Generate step failed.  Build files cannot be regenerated correctly.
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Update `CMakeLists.txt` to include `src/manifest_assessment.cpp`.
  3. Add tests that depend on the manifest-assessment layer.
  4. Run `cmake -S . -B build && cmake --build build`.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Compiler: `g++ 16.1.1`
  - CMake: `4.3.2`
  - Date: 2026-05-16

- First hypothesis:
  The new Phase 4 slice is correctly failing its red step because the manifest-assessment implementation file has not been created yet.
