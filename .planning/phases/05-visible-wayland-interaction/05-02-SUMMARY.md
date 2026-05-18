# 05-02 Summary

## Result

Linuxoid now tells one coherent focus and interaction story for the visible-launch seam instead of only a surface story.

## What changed

- Added `visible_target_state`, `focus_state`, `focus_owned`, `focus_owner`, `interaction_state`, and `interaction_target_component` to the persisted `window_manager` contract.
- Kept focus ownership tied to the same deterministic `window_id` as the resolved activity/process session.
- Reused the deterministic native input queue pattern for pointer/key event counts on ready fixture paths.

## Verified truth

- Ready fixture path reports either `headless-only` or `probe-only-live-target-available` truth depending on host probe availability.
- Blocked native path reports `blocked-by-native` for visible target, focus, and interaction instead of pretending surface readiness or focus ownership exists.
