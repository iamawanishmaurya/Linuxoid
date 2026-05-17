# Problem: Missing activity bootstrap fixture seam

- Date: 2026-05-18
- Environment: `/home/astra/codex/wine-for-android` on Linux, CMake build directory `build`

## Exact error

```text
/usr/bin/ld: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o: in function `(anonymous namespace)::TestNativeArtActivityBootstrapFixtureWritesStableArtifacts()':
undefined reference to `wfa::RunNativeArtActivityBootstrapFixture(...)'
/usr/bin/ld: undefined reference to `wfa::RenderNativeArtActivityBootstrapFixtureJson(...)'
```

## Reproduction steps

1. Open the repo at `/home/astra/codex/wine-for-android`.
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`.
3. Observe the linker failure while building `wfa_tests`.

## First hypothesis

Linuxoid already has the classloader, class-resolution, and runtime-smoke seams, but it does not yet expose the next post-class-resolution activity-bootstrap surface. The new tests are correctly failing because the activity-bootstrap fixture API has been declared in tests but not implemented in the runtime or wired into the build.
