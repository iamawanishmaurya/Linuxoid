# Changelog

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
