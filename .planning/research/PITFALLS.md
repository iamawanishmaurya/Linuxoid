# Pitfalls Research: Linuxoid v1.1 Visible App Launch

## Pitfall 1: Treating dispatch as if visible launch is already solved

Moving past `activity_oncreate_bundle_dispatch_required` matters, but it is not the same as having a visible settings window.

Warning signs:
- `onCreate(Bundle)` dispatch succeeds, but resource or surface readiness is still missing
- milestone claims jump from dispatch success to "the app runs"

Prevention:
- keep post-dispatch blocker reporting exact
- treat visible surface readiness as a separate milestone phase

## Pitfall 2: Hidden resource or initialization gaps

The settings activity may fail after dispatch because compiled resources, assets, or app-state initialization are still incomplete.

Warning signs:
- managed dispatch moves forward, then startup drops into generic window failure
- launch reports stop naming assets, resources, or app-state inputs

Prevention:
- keep resource and app-state readiness in the same launch proof
- expose the first exact missing resource or initialization seam

## Pitfall 3: Visible surface truth drift

A best-effort Wayland or EGL target must stay honest. Linuxoid should not overclaim visibility when the host display is absent or blocked upstream.

Warning signs:
- `visible_target_state` looks successful while launch is still blocked
- host availability fields stop matching the actual launch status

Prevention:
- keep `wayland_surface_available`, `egl_surface_available`, and `visible_target_state` tied to the same launch session
- preserve upstream blocker authority when visibility cannot proceed

## Pitfall 4: IME scope creep

The keyboard app contains both a settings activity and an `InputMethodService`. Visible settings launch is still not the same as keyboard enablement.

Warning signs:
- planning drifts from visible launch into system-wide input-method behavior
- milestone scope grows without reducing the current blocker

Prevention:
- keep the milestone anchored to `SettingsActivity`
- defer IME behavior until after visible launch and interaction are stable

## Pitfall 5: Recovery noise

The watchdog can become misleading if it starts proposing downstream repairs while a more important upstream launch blocker is still active.

Warning signs:
- recovery actions mention surface or focus repairs before dispatch truly moves
- repeated runs produce changing primary blocker language for the same seam

Prevention:
- keep the earliest blocker authoritative
- require repeated launch truth to stay comparable across runs

## Pitfall 6: Sideways expansion

The repo already contains enough broad runtime contracts. The main risk is adding more architecture instead of finishing the visible launch path.

Warning signs:
- milestone effort goes into new abstraction layers without reducing the keyboard app blocker
- reports grow richer while the real app still does not visibly start

Prevention:
- measure every phase by whether `/home/astra/Downloads/keyboard-0.1.28.apk` moves forward
- reject work that does not help one real app visibly launch
