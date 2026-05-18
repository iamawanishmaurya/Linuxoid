# Architecture Research: Build Order for First Real App Execution

## Goal

Derive the minimum component structure needed to get `keyboard-0.1.28.apk` started on Linux through Linuxoid.

## Major Components

### 1. APK intake and staging

Inputs:
- APK path
- staging root
- package and activity selection

Responsibilities:
- inspect the APK
- decode manifest metadata
- stage DEX, assets, resources, and native libraries
- emit deterministic artifact paths

Current code anchors:
- `/home/astra/codex/wine-for-android/src/apk_native_launch.cpp`
- `/home/astra/codex/wine-for-android/src/apk_archive.cpp`
- `/home/astra/codex/wine-for-android/src/apk_loader.cpp`

### 2. Package/activity resolution

Responsibilities:
- build the package record
- resolve launcher activity or explicit component
- tie launch intent to process/session state

Current code anchors:
- `/home/astra/codex/wine-for-android/src/apk_activity_launch_bridge.cpp`

### 3. Managed runtime and class loading

Responsibilities:
- discover runtime inputs
- resolve app class from staged DEX metadata
- materialize the lifecycle receiver contract
- interpret or bridge into managed execution

Current code anchors:
- `/home/astra/codex/wine-for-android/src/apk_runtime_bridge.cpp`
- `/home/astra/codex/wine-for-android/src/apk_dex_bridge.cpp`

### 4. Native and JNI execution support

Responsibilities:
- choose the correct ABI
- stage and load native libraries
- expose JNI and native dependency diagnostics

Current code anchors:
- `/home/astra/codex/wine-for-android/src/native_execute_stub.cpp`
- `/home/astra/codex/wine-for-android/src/apk_native_launch.cpp`

### 5. Window, surface, and input

Responsibilities:
- bind activity/process/session to a visible or headless-safe window contract
- render on Wayland when available
- preserve deterministic behavior in CI

Current code anchors:
- `/home/astra/codex/wine-for-android/src/apk_window_bridge.cpp`
- `/home/astra/codex/wine-for-android/src/native_window_surface.cpp`

### 6. Sandbox, permissions, and recovery

Responsibilities:
- maintain app storage
- enforce basic permission/AppOps decisions
- surface recovery actions through the Self-Healing Android Device watchdog

Current code anchors:
- `/home/astra/codex/wine-for-android/src/apk_storage_bridge.cpp`
- `/home/astra/codex/wine-for-android/src/apk_permission_bridge.cpp`
- `/home/astra/codex/wine-for-android/src/apk_self_healing_watchdog.cpp`

## Recommended Build Order

1. Improve real APK intake for `keyboard-0.1.28.apk`
2. Keep launcher activity resolution stable
3. Push managed execution one step further into real app lifecycle behavior
4. Bring x86_64 JNI/native loading into the same launch path
5. Tighten visible window/input behavior for the settings activity
6. Only then move deeper into IME-specific service behavior

## Key Integration Boundary

The most important architectural seam is the boundary between:

- Linuxoid's current minimal DEX interpreter
- a future ART-owned `ActivityThread` or equivalent managed app context

That seam should stay explicit in both code and roadmap, because it is the main blocker between "interesting proof" and "one real app starts".
