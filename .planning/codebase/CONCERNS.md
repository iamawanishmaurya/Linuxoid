# Concerns

This codebase is productive, but there are several clear risk areas.

1. Test concentration:
- `/home/astra/codex/wine-for-android/tests/test_main.cpp` is a very large single-file harness
- local reasoning is harder as coverage expands
- future failures may become harder to isolate cleanly

2. CLI concentration:
- `/home/astra/codex/wine-for-android/src/main.cpp` owns a very broad command surface
- usage text and routing logic are already long
- command sprawl may make future operator changes brittle

3. Placeholder-heavy execution seams:
- many current runtime slices are explicit placeholders or stubs
- examples include `framework-stubbed`, `object-placeholder`, placeholder uid/gid/pid sources, and detection-only runtime paths
- this is honest, but it means docs and tests can drift from real execution if not watched carefully

4. Documentation drift risk:
- multiple status surfaces need to stay aligned:
  - `README.md`
  - `CHANGELOG.md`
  - `docs/phased-build-plan.md`
  - `docs/self-healing-runtime-skeleton.md`
  - `docs/steps.md`
  - `src/project_status.cpp`

5. Execution-first blockers remain deep:
- the current major blocker is still bridging `MainActivity.onCreate` into a real ART-owned runtime context
- many higher-level proofs depend on this blocker being represented accurately

6. Brownfield complexity:
- the repo already contains many phase slices and proof commands
- adding new work without mapping existing bridges would be risky

7. Optional graphics/runtime integrations:
- Wayland and EGL are optional at build time
- behavior may differ across hosts if these libraries are present or absent

The codebase map should help future planning avoid widening the surface without strengthening the current execution-first spine.
