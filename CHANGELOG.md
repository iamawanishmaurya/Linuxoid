# Changelog

## v0.1.46 - 2026-05-17

- Upgrade `native-art-runtime-smoke` from a generic ART availability probe into a real host-side class-resolution attempt seam: Linuxoid now selects a deterministic manifest-derived target class and prepares or runs `dalvikvm -cp <apk> <class>` when a safe local ART runtime exists.
- Add regression coverage that proves the runtime-smoke seam records a deterministic target class, attempts host-side class resolution only when safe, and keeps fallback behavior honest when ART is absent.
- Refresh the README, phased plan, self-healing runtime note, changelog, and status output so Linuxoid now reports the first real host-side ART class-resolution attempt honestly while still marking full Android app bootstrap and execution as pending.

## v0.1.45 - 2026-05-17

- Clarify in the README, phased plan, and self-healing runtime note what Linuxoid self-healing means today: deterministic diagnosis, bounded recovery planning, replayable trace artifacts, and refusal of false success.
- Document more explicitly what still blocks full native Android app execution: real host-side ART/class execution, richer Binder/framework behavior, bound Wayland+EGL rendering, full IME/text composition, and full Android resource-table semantics.

## v0.1.44 - 2026-05-17

- Add explicit regression coverage for self-healing runtime honesty when a native dependency is missing, including fixture-level health classification, command-level JSON stability, and merged diagnostic replay output.
- Prove through tests that Linuxoid does not claim false success when `native_loading` is blocked, and that the deterministic recovery action `retry_native_load_after_bundle_refresh` is surfaced consistently across health and replay outputs.

## v0.1.43 - 2026-05-17

- Add a Linuxoid-owned `runtime-smoke-trace.jsonl` artifact so the ART runtime smoke seam now emits structured JSONL events instead of only a plan, log, and result file.
- Add a `native-runtime-diagnostic-replay <bootstrap-manifest>` seam so Linuxoid can merge health, recovery, classloader, class-resolution, and runtime-smoke traces into one replayable diagnostic bundle without rerunning the UI path.
- Refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status output so Linuxoid now reports the merged replay-bundle gate honestly while still marking real ART/DEX execution, full Android Binder behavior, compositor-backed rendering, and IME/text composition as pending.

## v0.1.42 - 2026-05-17

- Add a Linuxoid-owned `native-runtime-recovery-plan` seam so the self-healing runtime can now materialize deterministic recovery-plan and action-trace artifacts instead of only selecting bounded recovery actions inside `runtime-health.json`.
- Extend runtime-health records so recovery actions now carry stable action IDs, subsystem-scoped artifact paths, and replay-trace links for missing artifact, failed native load, unavailable display, failed service lookup, and pending DEX/ART work.
- Refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status output so Linuxoid now reports the deterministic recovery-plan gate honestly while still marking real ART/DEX execution, full Android Binder behavior, compositor-backed rendering, and IME/text composition as pending.

## v0.1.41 - 2026-05-17

- Add a Linuxoid-owned `native-art-class-resolution-fixture` seam so staged APK dex entries can now resolve manifest-target descriptors offline, write deterministic resolution-map plus trace artifacts, and report unresolved classes honestly without pretending ART has already executed them.
- Add a Linuxoid-owned `native-art-runtime-smoke` seam so the staged ART/classloader plan can now produce deterministic invocation-plan, runtime-log, and result artifacts while probing local ART availability safely and honestly.
- Thread the new offline class-resolution and runtime-smoke artifacts into the self-healing runtime story so `dex_classloader_readiness` now carries evidence about classpath readiness, resolved versus missing manifest targets, runtime detection, and probe attempts instead of stopping at a classloader-plan placeholder.
- Refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status output so Linuxoid now reports the offline DEX resolution plus host-ART smoke gates honestly while still marking real host ART class execution, Android framework loading, full Binder behavior, compositor-backed rendering, and IME/text composition as pending.

## v0.1.40 - 2026-05-17

- Add a Linuxoid-owned `native-art-classloader-fixture` seam so staged APK dex entries can be inventoried, manifest application/activity targets can be normalized into deterministic descriptors, and stable `art/classloader-plan.json` plus trace artifacts can be written without depending on Waydroid, ADB, or an emulator.
- Thread the new classloader preparation artifact into the self-healing runtime story so `dex_classloader_readiness` now points at a concrete classpath plan instead of a bundle-level placeholder while still refusing false success when host ART is unavailable.
- Refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status output so Linuxoid now reports the ART/classloader preparation gate honestly while still marking real ART execution, full Android Binder behavior, compositor-backed rendering, and full IME/text composition as pending.

## v0.1.39 - 2026-05-17

- Add a self-healing runtime observability skeleton so Linuxoid now records structured health for APK staging, native loading, surface readiness, input readiness, Binder/service readiness, and DEX/classloader readiness through `compatctl native-runtime-health-fixture`.
- Add deterministic recovery-action selection plus replayable `runtime-health-trace.jsonl` and `runtime-health-replay.json` artifacts so native-path failures can be diagnosed without rerunning the full UI flow.
- Refresh the README Mermaid architecture, phased plan, status output, and solution trail so the repo now reports the self-healing Android Device skeleton honestly while still marking ART/DEX execution, full Android Binder semantics, compositor-backed rendering, and full IME/text composition as pending.

## v0.1.38 - 2026-05-17

- Add a minimal APK/ZIP resource-readiness bridge so Linuxoid can inspect manifest metadata, list normalized assets, reject path traversal, and emit stable JSON through `compatctl inspect-apk-resources` without depending on Waydroid, ADB, or an emulator for the native direct-run path.
- Add a socketpair-backed local transport seam to the Binder-shaped service-manager fixture so Linuxoid now records deterministic lookup/transaction request-response round trips in `binder/transport-messages.jsonl`.
- Refresh the README Mermaid architecture, phased plan, changelog, and status output so Linuxoid now reports the pre-ART resource bridge and local Binder transport seam honestly while still marking ART/DEX execution, full Android resource-table semantics, real Android Binder behavior, and compositor-backed rendering as pending.

## v0.1.37 - 2026-05-17

- Add a minimal `native-service-manager-fixture` command and Binder-shaped local service-manager contract so Linuxoid now writes deterministic service registration, lookup, and transaction artifacts for `package_manager` and `activity_manager` without depending on Waydroid, ADB, or an emulator for the native direct-run path.
- Thread the new Binder-shaped artifact set into the native lifecycle shim so session state now carries service-manager metadata, lookup logs, and transaction logs instead of relying only on a flat placeholder registry.
- Refresh the Mermaid architecture, phased plan, verify examples, and status output so Linuxoid now reports the local Binder-shaped manager honestly while still marking real Binder transport, DEX/ART, bound Wayland-EGL rendering, IME/text composition, and richer resources as pending.

## v0.1.36 - 2026-05-17

- Add a minimal `native-input-queue-fixture` contract so Linuxoid now proves focused native-surface ownership, deterministic pointer/key injection, and stable metadata plus JSONL event artifacts without depending on Waydroid, ADB, or an emulator for the native direct-run path.
- Keep the new input seam honest by reporting probe-only versus headless-fallback backing separately from full IME/text composition, and keep the existing P1/P2 surface, Wayland, EGL, and callback proofs green.
- Refresh the Mermaid architecture, phased plan, verify examples, and status output so Linuxoid now reports the focused input contract as verified while still marking full IME/text composition, bound Wayland-EGL rendering, DEX/ART, Binder, and Android resource-table work as pending.

## v0.1.35 - 2026-05-17

- Add a minimal `ANativeWindow` bridge contract over the existing headless, Wayland, and EGL fixture seams so Linuxoid now exposes width, height, format, stride, deterministic buffer-geometry updates, and stable bridge artifacts without pretending full Android rendering is complete.
- Add the new `native-window-bridge-fixture` command plus tests that verify deterministic geometry updates, stable metadata/event paths, and honest headless-fallback versus probe-only reporting depending on Wayland/EGL backing availability.
- Refresh the Mermaid architecture, phased plan, verify examples, and status output so Linuxoid now reports the `ANativeWindow` bridge contract honestly while still marking Wayland-EGL binding, real Android drawing, DEX/ART, Binder, input, and full resources as pending.

## v0.1.34 - 2026-05-17

- Add an optional real EGL smoke fixture so Linuxoid can initialize an EGL display, choose a config, create an OpenGL ES context plus pbuffer surface, and write a deterministic `egl-metadata.json` artifact under the requested root.
- Add the new `native-egl-smoke-fixture` command plus tests that verify the JSON/artifact contract, deterministic metadata paths, and honest fallback behavior when EGL support is unavailable at build or runtime.
- Refresh the Mermaid architecture, phased plan, status output, verify examples, and dependency notes so Linuxoid now reports the real EGL context plus pbuffer proof honestly while still marking Wayland-EGL binding, `ANativeWindow` backing, DEX/ART, Binder, input, and full resources as pending.

## v0.1.33 - 2026-05-17

- Add an optional real `wayland-client` surface fixture so Linuxoid can connect to `wl_display`, bind `wl_compositor`, create a real `wl_surface`, and write a deterministic `surface-metadata.json` artifact under the requested root.
- Add the new `native-wayland-surface-fixture` command plus tests that verify the JSON/artifact contract, deterministic metadata paths, and honest fallback behavior when Wayland support is unavailable at build or runtime.
- Refresh the Mermaid architecture, phased plan, status output, and dependency notes so Linuxoid now reports the real Wayland client surface proof honestly while still marking EGL binding, `ANativeWindow` backing, DEX/ART, Binder, input, and full resources as pending.

## v0.1.32 - 2026-05-17

- Add a minimal `ANativeActivityCallbacks` contract and a `native-window-callback-fixture` command so Linuxoid can dispatch deterministic `window_created`, `window_changed`, and `window_destroyed` events against the existing headless native-window surface.
- Write a stable `native-window-callbacks.jsonl` artifact and structured callback JSON so the future direct native graphics path now has a Linuxoid-owned lifecycle journal instead of a placeholder callback story.
- Refresh the tests, Mermaid architecture, phased plan, and status output so Linuxoid now reports verified callback plumbing honestly while still marking real Wayland/EGL surfaces, compositor-backed callbacks, DEX/ART, Binder, input, and full resources as pending.

## v0.1.31 - 2026-05-16

- Add the first `P2.1` host-side `ANativeWindow`-shaped surface seam with explicit width, height, format, and stride metadata plus lifecycle checks for a future direct native runner.
- Add `native-first-pixel-fixture`, a headless Wayland/EGL-style test double that writes a deterministic `first-pixel-marker.txt` artifact and structured JSON without pretending a real compositor window exists yet.
- Refresh the tests, Mermaid architecture, phased plan, and status output so Linuxoid now reports the verified first-pixel marker slice honestly while still marking real Wayland/EGL, callbacks, DEX/ART, Binder, input, and full resources as pending.

## v0.1.30 - 2026-05-16

- Extend the native spike planner so Linuxoid now stages host-ABI native libraries into the deterministic bundle `lib` root, copies extracted `assets/` and `res/` content into the bundle resource tree, and reports unsupported ABI payloads honestly instead of pretending they can run.
- Add the first minimal APK-backed asset bridge so the stub asset manager can resolve and read staged assets by path from the native resource tree.
- Refresh the bootstrap/session metadata, tests, Mermaid architecture, phased plan, and status output so GitHub now reflects the staged-library and asset-read seam while still marking DEX/ART, Binder, graphics, input, and full Android resource handling as pending.

## v0.1.29 - 2026-05-16

- Keep `native-execute-stub` as the public P1 bootstrap entrypoint and harden it so the manifest path now forks and `execve`s a controlled Linuxoid child runner with deterministic cwd, Linuxoid-only environment variables, and inherited-fd cleanup.
- Load native libraries in deterministic order, call `JNI_OnLoad` when present, and emit structured JSON that records `execution_engine_ready`, `libraries_loaded`, `jni_onload_results`, `exit_reason`, and stable artifact paths for replay and harness diffing.
- Refresh the fixture native test library, bootstrap/session tests, README Mermaid architecture, phased plan, and status text so the repo now reflects the stricter P1 contract while still marking DEX/ART, Binder, graphics, input, and full resources as pending.

## v0.1.28 - 2026-05-16

- Add `native-process-bootstrap` so Linuxoid now turns a bootstrap manifest into a real parent/child native runner handoff with `pid`, `process_state`, `exit_code`, runner logs, and structured session state.
- Make `native-lifecycle-shim` truthful before launch by keeping activity state at `NOT_CREATED` instead of prewriting `RESUMED`, and persist richer process/report fields for later native slices.
- Switch generated native entrypoints to `native-process-bootstrap`, add fixture-backed tests for the new bootstrap path, and refresh the README Mermaid architecture plus roadmap to match the new execution flow.

## v0.1.27 - 2026-05-16

- Complete a 15-track Linuxoid research wave covering process bootstrap, DEX/ART, JNI, Binder/services, graphics/input, browser-control surfaces, and storage/sandboxing for the direct Android-on-Linux roadmap.
- Add `docs/15-track-research-wave-2026-05-16.md` as the integrated architecture note with per-track findings, repo touchpoints, immediate implementation moves, and a cross-track execution order.
- Refresh the README roadmap and reference links so GitHub now points at the completed research wave and reflects the current `execution 20/100` state honestly.

## v0.1.26 - 2026-05-16

- Add the first real `P1` native-runner slice with a dedicated `native_execute_stub.cpp`, minimal JNI/asset/looper/signal-handler surfaces, `dlopen`-based library discovery, `ANativeActivity_onCreate` resolution, and a five-second watchdog gate.
- Switch the generated native bootstrap entrypoint from `native-lifecycle-shim` to `native-execute-stub`, and add fixture-based tests that prove the runner reaches `ANativeActivity_onCreate` and exits `0`.
- Document the current local Calculator APK as a dex-only negative oracle, update the phased plan and README to reflect the fixture-backed `P1` proof, and record the new status split as `scaffold 95/100, execution 20/100`.

## v0.1.25 - 2026-05-16

- Add `docs/phased-build-plan.md` as the repo-facing source of truth for the direct-execution roadmap from `P0 Freeze & Triage` through `P6 Polish + Release`.
- Freeze the browser track behind `P5` in the README and browser architecture note so Linuxoid stops drifting into browser implementation before native execution, graphics, DEX, and Binder work land.
- Add `docs/p0-stub-audit.md` and update `compatctl status` plus `compatctl foundation` so the project now says `scaffold 95/100, execution 0/100` explicitly and maps the current native stub exits into an ordered `P1` queue.

## v0.1.24 - 2026-05-16

- Add a researched self-healing Android browser architecture direction for Linuxoid based on an Android `WebView` browser shell, bounded recovery policy, and MCP/harness-friendly artifacts.
- Update the README Mermaid architecture and browser roadmap so the repo now records the self-healing browser track as a first-class Linuxoid design constraint.
- Add `docs/browser-self-healing-architecture.md` with the recommended browser foundation, recovery taxonomy, machine-facing contract, and next implementation steps.

## v0.1.23 - 2026-05-16

- Add `native-lifecycle-shim` so Linuxoid can turn a native bootstrap manifest into deterministic session artifacts, activity-state tracking, and a first service registry for native candidates.
- Route the generated native bootstrap entrypoint through the lifecycle shim, so local Linux execution now proves a real lifecycle handoff instead of jumping straight from bootstrap artifacts to a stubbed stop.
- Refresh the README Mermaid architecture, MCP/harness-aligned roadmap, and status narrative so GitHub reflects the new lifecycle/service seam and the next process-bootstrap step.

## v0.1.22 - 2026-05-16

- Add MCP and harness compatibility as an explicit Linuxoid architecture rule in the README and Mermaid diagrams.
- Document that Linuxoid control paths, artifact layouts, and verification flows should remain machine-friendly for agent runtimes, MCP clients, and validation harnesses as native execution work continues.

## v0.1.21 - 2026-05-16

- Add `bootstrap-native-spike` so Linuxoid can turn a native spike candidate into a Linuxoid-owned bootstrap manifest, env script, entrypoint stub, and bootstrap report.
- Add `native-execute-stub` so the generated native bootstrap path can run locally on Linux and report its still-missing execution engine honestly instead of pretending app code already runs.
- Refresh the README Mermaid architecture, dependency notes, verification commands, and next-step roadmap so GitHub now shows the native bootstrap surface and the next path toward real direct Linux execution.

## v0.1.20 - 2026-05-16

- Add a README Mermaid maintenance rule so the current and target Linuxoid architecture graphs are updated in the same commit as meaningful architecture changes.
- Keep the GitHub documentation honest by making diagram upkeep an explicit project workflow requirement instead of an informal convention.

## v0.1.19 - 2026-05-16

- Add the first `plan-native-spike` flow so Linuxoid can stage a local APK, assess whether it fits the first native app slice, and write a Linuxoid-owned bundle layout plus bootstrap spec for future no-runtime execution.
- Fix the native spike gate so launcher-resolved multi-activity apps are still eligible, which lets Calculator pass the first real native planner proof without weakening the service, IME, boot, or secondary-process blockers.
- Refresh the README Mermaid architecture, dependency notes, verification examples, and roadmap so the GitHub repo now reflects the new native-spike slice and the next work toward true direct Linux execution.

## v0.1.18 - 2026-05-16

- Add attached-target `inspect-package` metadata lookup so Linuxoid can read package visibility, resolved launcher, install path, and version information from a live `attached-adb` runtime.
- Add attached-ADB launcher auto-resolution to `preflight-runtime`, `launch-package`, `verify-package`, and `verify-package-matrix`, removing the need to hard-code simple launcher components for installed-app flows.
- Verify the new attached-target path live with `com.android.settings`, plus a `3/3` attached-target matrix for `com.android.settings`, `com.android.calculator2`, and `org.fdroid.fdroid`, then refresh the README roadmap toward the first native package-launch spike.

## v0.1.17 - 2026-05-16

- Add backend-neutral `verify-package` and `verify-package-matrix` commands so installed-package verification no longer depends on Waydroid-shaped product names.
- Add attached-ADB generic verification tests and fix the matrix fixture so mocked `am start -W` output proves the launched component the same way the real runtime bridge expects.
- Refresh the README architecture, verification examples, progress numbers, and next-step roadmap so the GitHub repo reflects the new generic verification surface and the next attached-target priorities.

## v0.1.16 - 2026-05-16

- Add `discover-runtime` and `preflight-runtime` so Linuxoid can enumerate attached Android targets and check launch readiness before a generic installed-package launch.
- Bound the new attached-ADB discovery/preflight path with timeouts so it fails fast instead of hanging in the live environment.
- Refresh the README architecture and next-step roadmap to reflect that runtime discovery/preflight is now implemented and the next frontier is generic verification plus launcher resolution on attached targets.

## v0.1.15 - 2026-05-16

- Refresh the GitHub README Mermaid architecture so it now shows both the current working backend-neutral transition architecture and the long-term native Linux runtime target.
- Add a clear external-dependencies section to the README covering the current build and runtime requirements, including `apktool`, `adb`, and a live Android runtime adapter.
- Add five concrete next-step recommendations in the README focused on the goal of running Android apps on Linux without external Android runtime dependencies.

## v0.1.14 - 2026-05-16

- Add a backend-neutral `launch-package` runtime seam for installed-package launches, with `waydroid`, `attached-adb`, and `native` backend routing.
- Shift installed-package host launch artifacts to the generic `launch-package` contract while keeping the current Waydroid-facing commands as compatibility aliases.
- Add generic installed-package contract tests and raise verified phase loading to `91/100` while checkpoint gates remain `70/100`.

## v0.1.13 - 2026-05-16

- Fix automatic launcher inference so Linuxoid skips disabled manifest launcher candidates when generating Linux launchers from local APKs.
- Verify the repaired APK-backed path live with `/home/astra/Downloads/F-Droid.apk`, reaching a successful Linux launch through the inferred main activity.
- Raise verified project loading to `90/100` while checkpoint gates remain `70/100`.

## v0.1.12 - 2026-05-16

- Add `verify-apk-host-launch-auto` so Linuxoid can verify the full local-APK Linux launcher flow against a live Android runtime.
- Verify the new APK-backed path live with `/home/astra/Downloads/keyboard-0.1.28.apk`, reaching `Ready for typing: yes` through the generated Linux launcher.
- Raise verified project loading to `89/100` while checkpoint gates remain `70/100`.

## v0.1.11 - 2026-05-16

- Add the current full working Mermaid architecture to the GitHub README so the live Linuxoid flow is visible at a glance.
- Document both verified execution paths in the README diagram: APK-backed IME provisioning and installed-package Waydroid launch verification.

## v0.1.10 - 2026-05-16

- Add `verify-waydroid-matrix` so Linuxoid can verify several installed Waydroid apps in one pass and report package-level pass/fail results.
- Extend the Waydroid integration layer with matrix reporting that keeps running even when one app fails, so compatibility gaps surface honestly instead of aborting the whole batch.
- Keep verified project loading at `88/100` while raising the repeatability of the direct-on-Linux verification workflow.

## v0.1.9 - 2026-05-16

- Add `verify-waydroid-package` so Linuxoid can prove a direct Linux launch path for an installed Waydroid app through three checks: runtime launch, launcher generation, and generated launcher execution.
- Verify the new direct-on-Linux path on live Waydroid with `com.android.calculator2`, `com.android.settings`, and `org.fdroid.fdroid`.
- Raise verified project loading to `88/100` while keeping checkpoint gates at `70/100`.

## v0.1.8 - 2026-05-16

- Add `launch-waydroid-package` so Linuxoid can launch installed non-IME apps on Waydroid without requiring APK reinstall or hardcoded component selection.
- Add `desktopify-waydroid-package` so Linuxoid can generate Linux launchers and `.desktop` entries for installed Waydroid apps.
- Verify the new installed-package path on live Waydroid with `com.android.calculator2`, including a Linuxoid-generated host launcher execution from Linux.
- Raise verified project loading to `87/100` and checkpoint gates to `70/100`.

## v0.1.7 - 2026-05-16

- Normalize fully qualified IME identifiers to the short `package/.Class` form before Linuxoid sends `ime enable` and `ime set`, fixing the Waydroid mutation failure for the keyboard app.
- Verify the Linuxoid-generated host launcher against a live Waydroid runtime and reach `Ready for typing: yes` for the FUTO keyboard APK from Linux.
- Raise verified project loading to `84/100` while runtime checkpoint gates remain `58/100`.

## v0.1.6 - 2026-05-16

- Rename the project-facing identity to `Linuxoid` and point the local Git remote at `https://github.com/iamawanishmaurya/Linuxoid`.
- Add `desktopify-apk-auto`, which infers the launcher activity from the APK manifest instead of requiring a caller-supplied component.
- Split desktop-entry and launcher-script roots so Linuxoid can target host-discoverable application directories without forcing the wrapper script to live beside the `.desktop` file.
- Raise verified project loading to `78/100` while keeping runtime checkpoint gates at `58/100`.

## v0.1.5 - 2026-05-16

- Add a generic `launch-activity` bridge for explicit Android component launches from Linux.
- Add a `desktopify-apk` path that generates a Linux wrapper script and `.desktop` entry for a caller-selected Android component.
- Harden component matching so the runtime treats short and fully qualified Android component forms as equivalent.
- Raise verified project loading to `76/100` and runtime checkpoint gates to `58/100`, with host integration now in active validation.

## v0.1.4 - 2026-05-16

- Add a real `provision-ime` path that installs a keyboard APK on a live Android target, enables the IME, sets it as default, and re-verifies the resulting runtime state.
- Extend live runtime verification to prove the target IME is explicitly present in `enabled_input_methods` in addition to being registered and selected as default.
- Guard the provisioning verdict against wrong-APK/package mismatches and preserve partial evidence when post-action readback fails.
- Raise verified project loading to `71/100` and runtime checkpoint gates to `48/100`, with the golden-app launch and repeatability checkpoints now complete.

## v0.1.3 - 2026-05-16

- Add a real `load-apk` path that stages APKs into a compat root using decoded manifest and `apktool.yml` metadata.
- Add an `adb-ime-status` runtime bridge that verifies install state, IME registration, default IME selection, and settings launch on a live Android target.
- Harden the loader/runtime slice with install-key sanitization, exact ADB matching, optional launch verification, and unique cleaned temp decode directories.
- Raise verified project loading to `65/100` and runtime checkpoint gates to `28/100`.

## v0.1.2 - 2026-05-16

- Add a Phase 4 decoded-manifest assessor for runtime and service requirements.
- Add CLI support for `compatctl assess-manifest`.
- Verify the keyboard APK manifest path and raise Phase 4 loading to `45/100`, bringing overall phase loading to `58/100`.

## v0.1.1 - 2026-05-16

- Add the first C++ MVP scaffold with `compatctl`, a checkpoint engine, and package-layout planning.
- Add local build and test verification for the new scaffold.
- Add reproducibility notes in the project README.

## v0.1.0 - 2026-05-16

- Bootstrap the repository, documentation workflow, and MVP planning baseline.
- Establish English-only project documentation and commit policy from the first version.
