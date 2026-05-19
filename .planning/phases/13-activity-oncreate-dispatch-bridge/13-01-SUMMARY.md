# Phase 13 Plan 01 Summary

## Outcome

Phase 13 wave 1 moved the keyboard launch path beyond the generic `activity_oncreate_bundle_dispatch_required` seam when staged DEX metadata is available. Linuxoid now binds the managed runtime context, resolves the staged `SettingsActivity->onCreate(Landroid/os/Bundle;)V` entrypoint, and can execute the first managed post-dispatch bytecode boundary instead of stopping at a generic native handoff.

## What changed

- Kept the native launch and first-app-start path aligned around one Linuxoid-owned managed runtime-context handoff.
- Reused the existing minimal DEX interpreter so the real lifecycle path reaches a concrete `invoke-super` boundary.
- Tightened the focused JNI-registration regression fixture to include deterministic `classes.dex` and a local ART-runtime-root override, which makes the post-dispatch seam reproducible offline.

## Verification

- `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build`

## Resulting seam

- Smaller than: `activity_oncreate_bundle_dispatch_required`
- Reached by real keyboard path: yes
- Exact managed boundary now available for later waves: `framework-boundary-unimplemented:Landroidx/activity/ComponentActivity;->onCreate(Landroid/os/Bundle;)V`
