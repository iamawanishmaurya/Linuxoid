# Solution: define `ANativeWindowStub` once in the shared namespace

Related problem: [2026-05-16-native-window-stub-type-ambiguity.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-native-window-stub-type-ambiguity.md)

## What failed

`src/native_window_surface.cpp` introduced a private `ANativeWindowStub` inside its anonymous namespace even though `native_types.hpp` already forward-declared `wfa::ANativeWindowStub`.

## What worked

Moved the concrete `ANativeWindowStub` definition into the `wfa` namespace and removed the duplicate anonymous-namespace type.

## Why it worked

The forward declaration and the implementation now describe the same type, so the compiler no longer sees two conflicting `ANativeWindowStub` candidates.

## Commands run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```
