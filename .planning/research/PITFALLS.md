# Pitfalls Research: First Real App Execution

## Pitfall 1: Binary manifest and compiled resource gaps

The target APK is a real production-style package, not a plain-XML fixture.
Linuxoid may still fail before app logic if binary manifest or compiled resource handling is incomplete.

Warning signs:
- package or launcher activity cannot be resolved from the real APK
- permissions and resources disappear in reports that work for fixtures

Prevention:
- treat real APK metadata decode as an early milestone requirement
- keep any binary-manifest/resource blocker explicit in reports

## Pitfall 2: Mistaking launcher activity success for full IME success

`keyboard-0.1.28.apk` includes both a launcher settings activity and an `InputMethodService`.
Getting the settings activity to open is a valid milestone, but it is not the same as enabling the keyboard system-wide.

Warning signs:
- the app window opens, but there is no path toward actual IME behavior
- milestone claims drift from "app starts" into "keyboard fully works"

Prevention:
- define the first milestone around launcher activity start and interaction
- treat IME registration/enablement as a later milestone

## Pitfall 3: JNI and native library underestimation

The APK ships substantial native libraries, including x86_64 variants.
Real execution may fail only after managed code reaches JNI-backed paths.

Warning signs:
- settings activity launch reaches deeper code paths and then crashes or blocks
- ABI or `.so` loading diagnostics are present but ignored

Prevention:
- keep native library staging and loading in the critical path
- track JNI boundaries as first-class blockers in reports

## Pitfall 4: Framework-boundary optimism

Minimal DEX interpretation is useful, but a real app quickly hits framework-owned assumptions around context, lifecycle dispatch, resources, and services.

Warning signs:
- too many `framework-stubbed` boundaries accumulate
- reports say "returned" while the real app still cannot reach visible useful behavior

Prevention:
- keep `needs-real-activitythread-context` or similar blockers explicit
- prefer one real framework seam over many additional local proofs

## Pitfall 5: Window/input success without meaningful interaction

A visible Wayland window alone is not enough for the keyboard target.
Usability depends on focus, input, and settings interaction.

Warning signs:
- app launches visibly but cannot be navigated or used
- window proof passes while real app interaction stalls

Prevention:
- define success as "launch and interact", not merely "launch"
- keep input behavior in the same verification loop as runtime execution

## Pitfall 6: Sideways expansion

The repo already contains many broad runtime slices.
The easiest mistake is expanding compatibility or architecture again instead of finishing the first real app path.

Warning signs:
- new work adds reports, matrices, or abstractions without reducing the real app blocker
- milestone effort goes into generality before `keyboard-0.1.28.apk` starts

Prevention:
- measure progress by whether the verification APK moves forward
- reject work that does not help one real app run
