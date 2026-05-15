# Solution: missing-cpp-scaffold-files

- Problem file: [2026-05-16-missing-cpp-scaffold-files.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-missing-cpp-scaffold-files.md)
- What failed:
  The first configure pass failed because the TDD setup referenced source files and headers that had not been created yet.

- What worked:
  Adding the minimal production scaffold resolved the failure:
  - checkpoint progress engine
  - package-layout planner
  - project-status report builder
  - `compatctl` CLI entrypoint
  - local test executable

- Why it worked:
  The red step was caused by missing implementation files, not by a build-system misconfiguration. Creating the expected source tree gave CMake valid targets and satisfied the tests.

- All commands run:

```text
cmake -S . -B build
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure && ./build/compatctl status && ./build/compatctl foundation && ./build/compatctl layout com.example.demo alpha01 42 /var/lib/wfa
```
