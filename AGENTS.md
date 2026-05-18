<!-- GSD:project-start source:PROJECT.md -->
## Project

**Linuxoid**

Linuxoid is a Wine-style Android-on-Linux runtime that aims to run Android APKs directly on a Linux desktop without an emulator, full Android VM, or Waydroid-style dependency. The current codebase already stages APKs, resolves activities, bootstraps runtime/session state, and executes small real DEX bytecode checkpoints; the next milestone is to turn that execution-first spine into a first genuinely usable Android app launch on Linux. The first verification target is `keyboard-0.1.28.apk` from Downloads, and the first host target is a Wayland Linux desktop.

**Core Value:** Run a real Android app directly on Linux through Linuxoid’s own compatibility/runtime path, with honest execution and honest blockers instead of emulator fallback.

### Constraints

- **Runtime model**: Native Linux compatibility layer first — the project goal is direct APK execution without emulator or full Android container dependency
- **Host target**: Wayland Linux desktop — first visible launch, focus, and input work should optimize for modern Wayland environments
- **Verification app**: `keyboard-0.1.28.apk` — milestone success must be anchored to a concrete real APK, not only synthetic fixtures
- **Execution honesty**: No fake ART/framework claims — unsupported boundaries must stay explicit and actionable
- **Development style**: Execution-first slices — prefer the smallest real runtime improvement that gets one app closer to running
<!-- GSD:project-end -->

<!-- GSD:stack-start source:codebase/STACK.md -->
## Technology Stack

- `C++20`
- standard library containers, strings, filesystem, and streams
- POSIX/Linux facilities such as `dlopen`, file descriptors, and `/proc/self/exe`
- static library `wfa_core`
- static library `linuxoid_p1`
- executable `compatctl`
- executable `wfa_tests`
- shared fixture library `linuxoid_p1_fixture`
- `/home/astra/codex/wine-for-android/include/wfa/`
- `/home/astra/codex/wine-for-android/src/`
- `/home/astra/codex/wine-for-android/tests/`
- `dl`
- optional `wayland-client`
- optional `EGL`
- direct APK staging and launch proofs
- DEX parsing and minimal bytecode interpretation
- native Linux session artifacts and JSON reports
- deterministic offline fixtures rather than SDK-built apps
- `apk_*` bridge classes for direct APK session slices
- `art_*` fixtures for runtime/bootstrap probes
- native stubs for lifecycle, input, window, and execution
- watchdog and health reporting for the Self-Healing Android Device runtime
<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->
## Conventions

- bridge interfaces use `apk_*_bridge` names for direct APK session slices
- fixture and smoke helpers use `*_fixture` or `*_smoke`
- report renderers often use `Render*Json` or similar explicit names
- report exact blockers instead of overstating support
- persist deterministic JSON artifacts under staged session roots
- keep tests offline and reproducible
- use fixture APK-like ZIP payloads instead of network or SDK builds
- prove the smallest real execution seam possible
- name unsupported boundaries precisely
- distinguish `framework-stubbed`, `object-placeholder`, and real execution
- keep the next blocker explicit in status and docs
- phase progression is recorded in `README.md`, `CHANGELOG.md`, `docs/phased-build-plan.md`, and `docs/steps.md`
- runtime honesty is reinforced in `docs/self-healing-runtime-skeleton.md`
- the phrase `Self-Healing Android Device` is part of the project/runtime language
- `compatctl` owns the operator surface
- proof modes use flags such as `--first-app-start-proof`
- inspection commands use names like `inspect-apk-*`
- standard library and explicit structs over metaprogramming-heavy style
- deterministic string values for states and blockers
- minimal abstraction around JSON string assembly where needed
- most regression coverage is concentrated in one large `tests/test_main.cpp`
- fixtures are deterministic and synthetic
- build verification is normally `cmake --build build && ctest --test-dir build --output-on-failure`
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->
## Architecture

- `wfa_core` contains lower-level foundation, runtime discovery, manifest, package layout, native spike, and status code
- `linuxoid_p1` contains direct APK session bridges and proof paths
- `/home/astra/codex/wine-for-android/src/main.cpp`
- `/home/astra/codex/wine-for-android/src/apk_native_launch.cpp`
- archive and manifest parsing
- package and activity resolution
- storage and sandbox state
- permissions and AppOps
- process and activity manager state
- window and surface state
- runtime bootstrap state
- Java proof and first-app-start execution checkpoints
- Self-Healing Android Device watchdog diagnostics
- `apk_activity_launch_bridge.hpp`
- `apk_asset_bridge.hpp`
- `apk_dex_bridge.hpp`
- `apk_permission_bridge.hpp`
- `apk_process_bridge.hpp`
- `apk_runtime_bridge.hpp`
- `apk_storage_bridge.hpp`
- `apk_window_bridge.hpp`
- `apk_self_healing_watchdog.hpp`
- staged APK metadata
- minimal DEX parsing and interpretation
- explicit placeholder or stubbed Android framework seams
- `needs-real-art-execution`
- `needs-real-activitythread-context`
- `unsupported-dex-opcode:...`
<!-- GSD:architecture-end -->

<!-- GSD:skills-start source:skills/ -->
## Project Skills

No project skills found. Add skills to any of: `.claude/skills/`, `.agents/skills/`, `.cursor/skills/`, `.github/skills/`, or `.codex/skills/` with a `SKILL.md` index file.
<!-- GSD:skills-end -->

<!-- GSD:workflow-start source:GSD defaults -->
## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:
- `/gsd-quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd-debug` for investigation and bug fixing
- `/gsd-execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->



<!-- GSD:profile-start -->
## Developer Profile

> Profile not yet configured. Run `/gsd-profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
