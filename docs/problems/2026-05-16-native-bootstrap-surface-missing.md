# Native Bootstrap Surface Missing

## Exact Error

The first red-bar build for the native activity bootstrap slice failed at link time:

```text
[100%] Linking CXX executable wfa_tests
/usr/bin/ld: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o: in function `(anonymous namespace)::TestNativeActivityBootstrapWritesArtifacts()':
test_main.cpp:(.text+0x7fc7): undefined reference to `wfa::BuildNativeActivityBootstrap(wfa::NativeLaunchPlan const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)'
/usr/bin/ld: test_main.cpp:(.text+0x8724): undefined reference to `wfa::RenderNativeActivityBootstrapReport[abi:cxx11](wfa::NativeActivityBootstrap const&)'
collect2: error: ld returned 1 exit status
make[2]: *** [CMakeFiles/wfa_tests.dir/build.make:102: wfa_tests] Error 1
make[1]: *** [CMakeFiles/Makefile2:1081: CMakeFiles/wfa_tests.dir/all] Error 2
make: *** [Makefile:101: all] Error 2
```

## Reproduction Steps

1. Add the native activity bootstrap declarations and tests.
2. Run:
   - `cmake --build build`

## Environment

- Project: Linuxoid
- Repository: `/home/astra/codex/wine-for-android`
- Host OS: Linux
- Build tool: CMake-generated Makefiles

## First Hypothesis

The tests are correctly targeting a missing implementation. Linuxoid needs a concrete native bootstrap surface that writes bootstrap artifacts and renders a bootstrap report for native spike candidates.
