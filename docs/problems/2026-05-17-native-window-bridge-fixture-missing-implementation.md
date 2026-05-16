# Problem: native window bridge fixture API was missing implementation

## Exact error

```text
native window bridge fixture behavior was specified by the new test cases, but the bridge contract functions and CLI surface did not exist yet.
```

## Reproduction steps

1. Add tests that expect a native-window bridge fixture with deterministic geometry updates and stable metadata/event artifacts.
2. Attempt to build or reason about the feature before implementing the bridge functions.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Date: `2026-05-17`
- Toolchain: local `cmake` + C++20 build
- Runtime probes available: Wayland and EGL both present on this host

## First hypothesis

Linuxoid had the lower-level headless, Wayland, and EGL fixtures, but it still needed a dedicated `ANativeWindow` bridge contract layer that tied geometry updates and fallback reporting together in one place.
