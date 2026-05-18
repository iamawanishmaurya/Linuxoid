# Architecture

Linuxoid is organized around many narrow runtime contracts rather than one large monolith.
The dominant pattern is a bridge per capability slice, with `compatctl` orchestrating them.

Top-level architectural split:
- `wfa_core` contains lower-level foundation, runtime discovery, manifest, package layout, native spike, and status code
- `linuxoid_p1` contains direct APK session bridges and proof paths

Main orchestration path:
- `/home/astra/codex/wine-for-android/src/main.cpp`
- `/home/astra/codex/wine-for-android/src/apk_native_launch.cpp`

The direct APK session path assembles these contracts:
- archive and manifest parsing
- package and activity resolution
- storage and sandbox state
- permissions and AppOps
- process and activity manager state
- window and surface state
- runtime bootstrap state
- Java proof and first-app-start execution checkpoints
- Self-Healing Android Device watchdog diagnostics

Header naming reveals the architecture clearly:
- `apk_activity_launch_bridge.hpp`
- `apk_asset_bridge.hpp`
- `apk_dex_bridge.hpp`
- `apk_permission_bridge.hpp`
- `apk_process_bridge.hpp`
- `apk_runtime_bridge.hpp`
- `apk_storage_bridge.hpp`
- `apk_window_bridge.hpp`
- `apk_self_healing_watchdog.hpp`

Execution-first logic currently lives at the boundary between:
- staged APK metadata
- minimal DEX parsing and interpretation
- explicit placeholder or stubbed Android framework seams

The code favors deterministic JSON artifact output at each bridge.
That means most subsystems have a persisted report path as part of their contract.

The architecture is intentionally honest about incomplete execution.
Instead of hiding blockers, it records exact boundary reasons such as:
- `needs-real-art-execution`
- `needs-real-activitythread-context`
- `unsupported-dex-opcode:...`

This is a brownfield codebase with significant accumulated runtime slices already in place.
Future work should usually attach to existing bridge/report patterns instead of introducing a parallel architecture.
