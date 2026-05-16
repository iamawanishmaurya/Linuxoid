# Problem: APK loader materialize helper uses shell helpers before declaration

## Exact error

```
/home/astra/codex/wine-for-android/src/apk_loader.cpp: In function ‘void wfa::{anonymous}::MaterializeDecodedPayload(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&)’:
/home/astra/codex/wine-for-android/src/apk_loader.cpp:67:31: error: ‘QuoteForShell’ was not declared in this scope
   67 |                               QuoteForShell(decode_root.string()) + " " +
      |                               ^~~~~~~~~~~~~
/home/astra/codex/wine-for-android/src/apk_loader.cpp:69:3: error: ‘RunCommandCapture’ was not declared in this scope
   69 |   RunCommandCapture(command);
      |   ^~~~~~~~~~~~~~~~~
```

## Reproduction steps

1. Edit `src/apk_loader.cpp` to add `MaterializeDecodedPayload`.
2. Run `cmake --build build`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Date: `2026-05-16`
- Toolchain: local `cmake` + C++20 build

## First hypothesis

The new helper was inserted above the existing `QuoteForShell` and `RunCommandCapture` helpers in the same anonymous namespace, so the compiler sees the calls before those functions are declared.
