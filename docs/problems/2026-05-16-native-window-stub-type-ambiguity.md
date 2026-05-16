# Problem: duplicate `ANativeWindowStub` type causes ambiguity in native window fixture

## Exact error

```
/home/astra/codex/wine-for-android/src/native_window_surface.cpp: In function ‘wfa::ANativeWindow* wfa::CreateHeadlessNativeWindowSurface(const NativeWindowMetadata&, const std::string&)’:
/home/astra/codex/wine-for-android/src/native_window_surface.cpp:78:22: error: reference to ‘ANativeWindowStub’ is ambiguous
   78 |   auto* window = new ANativeWindowStub;
      |                      ^~~~~~~~~~~~~~~~~
  • there are 2 candidates
    • candidate 1: ‘struct wfa::{anonymous}::ANativeWindowStub’
    • candidate 2: ‘struct wfa::ANativeWindowStub’
```

## Reproduction steps

1. Add `src/native_window_surface.cpp` with a local `ANativeWindowStub` definition.
2. Rebuild with `cmake --build build`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Date: `2026-05-16`
- Toolchain: local `cmake` + C++20 build

## First hypothesis

The `.cpp` file defines a private `ANativeWindowStub` even though `native_types.hpp` already forward-declares `wfa::ANativeWindowStub`, so the compiler sees two distinct candidates with the same name.
