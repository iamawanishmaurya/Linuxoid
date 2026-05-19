# Stack

Project: `Linuxoid`

This repository is a C++20 codebase built with `CMake`.
The root build file is `/home/astra/codex/wine-for-android/CMakeLists.txt`.
The declared project version in CMake is `0.1.55`.
Compilation disables compiler-specific GNU extensions.

Primary language and runtime:
- `C++20`
- standard library containers, strings, filesystem, and streams
- POSIX/Linux facilities such as `dlopen`, file descriptors, and `/proc/self/exe`

Primary build targets:
- static library `wfa_core`
- static library `linuxoid_p1`
- executable `compatctl`
- executable `wfa_tests`
- shared fixture library `linuxoid_p1_fixture`

Source layout for core build targets:
- `/home/astra/codex/wine-for-android/include/wfa/`
- `/home/astra/codex/wine-for-android/src/`
- `/home/astra/codex/wine-for-android/tests/`

Important libraries and link inputs:
- `dl`
- optional `wayland-client`
- optional `EGL`

The codebase does not use a package manager like `conan` or `vcpkg`.
The repository currently leans on the host toolchain and host system libraries.

Current technical emphasis:
- direct APK staging and launch proofs
- DEX parsing and minimal bytecode interpretation
- real APK verification against `keyboard-0.1.28.apk`
- narrow execution-first progress through concrete invoke, no-code, and opcode boundaries on the real keyboard APK path
- the current live managed-bytecode blocker is `unsupported-dex-opcode:opcode-0x90` after clearing the earlier `Ljava/lang/Math;->min(II)I` no-code seam
- native Linux session artifacts and JSON reports
- deterministic offline fixtures rather than SDK-built apps

Notable subsystem families:
- `apk_*` bridge classes for direct APK session slices
- `art_*` fixtures for runtime/bootstrap probes
- native stubs for lifecycle, input, window, and execution
- watchdog and health reporting for the Self-Healing Android Device runtime

The stack is intentionally bridge-heavy and proof-oriented.
It is optimized for incremental execution checkpoints rather than a finished Android runtime.
