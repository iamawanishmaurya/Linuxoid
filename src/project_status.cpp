#include "wfa/project_status.hpp"

#include <cmath>
#include <sstream>

namespace wfa {

std::vector<PhaseStatus> BuildDefaultPhases() {
  return {
      {"P1", "Research and architecture lock", 100,
       "Language, runtime direction, and scope are locked."},
      {"P2", "Build and tooling scaffold", 100,
       "CMake, CLI entrypoint, and local test target exist."},
      {"P3", "Package and storage contract", 100,
       "APK layout and host storage mapping are encoded in code."},
      {"P4", "Runtime and service contract", 100,
       "The project can load an APK into a compat root, install it on a live target, provision its IME service, inspect attached-target package metadata, auto-resolve simple launcher components, route installed-package launch through a backend-neutral contract, preflight attached Android targets before launch, materialize a Linuxoid-owned native bundle plan plus bootstrap spec for simple foreground APKs, emit a Linuxoid-owned native bootstrap manifest plus entrypoint stub for those candidates, hand that bootstrap off to a Linuxoid-owned lifecycle and service shim with deterministic session artifacts, expose a local Binder-shaped service-manager contract with a socketpair-backed local transport seam, inspect APK manifest plus asset/resource readiness through a native Linux bridge that is staged for future ART/DEX loading, classify runtime health plus deterministic recovery actions for the Self-Healing Android Device skeleton, formalize those recovery actions with stable rank, retry-budget, and scope metadata, write a deterministic ART/classloader preparation plan from staged APK dex entries plus manifest targets, materialize a manifest-derived application-plus-activity bootstrap planning seam on top of class-resolution plus runtime-smoke evidence, upgrade that seam into a deterministic supervised bootstrap-execution contract with stable execution-context, runner-script, runner-state, and phase-log artifacts, and now keep the live health plus replay path fast enough to rerun on staged bundles by reusing one opened APK archive plus the already-staged bundle manifest."},
      {"P5", "Graphics and host integration", 84,
       "Linux launcher artifacts now target a backend-neutral installed-package launch seam, and both Waydroid-backed and attached-ADB host launch verification can reuse the same generated launcher flow."},
      {"P6", "APK execution and validation", 93,
       "A live Linux host launcher now proves the local keyboard and F-Droid APK flows plus Calculator, Settings, and installed-package F-Droid launch flows on Waydroid, attached-ADB now supports metadata-backed auto-resolution and a live `3/3` installed-package verification matrix without Waydroid-shaped product names, Calculator passes a real local `plan-native-spike` verification with a written native bootstrap spec and no blockers, and the generated Linuxoid-native lifecycle shim now runs locally with a ready session handoff while still reporting its pending execution-engine state honestly."},
  };
}

std::vector<Checkpoint> BuildDefaultCheckpoints() {
  return {
      {"C1", "Environment Reproducibility", 15,
       CompletionState::kInValidation,
       "Build instructions and the local scaffold exist; clean-room proof is pending."},
      {"C2", "Golden App Launch", 20, CompletionState::kComplete,
       "The keyboard APK now installs, enables, sets default IME, and launches settings successfully on the live target."},
      {"C3", "Representative Compatibility Set", 25,
       CompletionState::kInValidation,
       "The keyboard, Calculator, Settings, and F-Droid launch paths are now verified on live runtimes, and Calculator also passes the first native spike planner, but the wider native app matrix is still incomplete."},
      {"C4", "Host Integration", 20, CompletionState::kInValidation,
       "Linux wrappers and `.desktop` entries now launch APK-backed and multiple installed Waydroid apps from the host, and Linuxoid now owns a local native bootstrap stub for Calculator-like bundles, but compositor, clipboard, and richer input handoff still need proof."},
      {"C5", "Repeatability and Regression Guard", 20,
       CompletionState::kComplete,
       "The build, tests, installed-package verification, and live native Calculator spike planner all passed again on the latest slice."},
  };
}

int CalculateAveragePhaseProgress(std::span<const PhaseStatus> phases) {
  if (phases.empty()) {
    return 0;
  }

  int total = 0;
  for (const auto& phase : phases) {
    total += phase.progress;
  }

  return static_cast<int>(
      std::lround(static_cast<double>(total) / phases.size()));
}

std::string RenderProjectStatusReport() {
  const auto phases = BuildDefaultPhases();
  const auto checkpoints = BuildDefaultCheckpoints();
  const int phase_progress = CalculateAveragePhaseProgress(phases);
  const int checkpoint_progress =
      CalculateWeightedCheckpointProgress(checkpoints);
  const int completed_checkpoints = CountCompletedCheckpoints(checkpoints);

  std::ostringstream output;
  output << "Project: Linuxoid\n";
  output << "Language: C++\n";
  output << "Runtime Strategy: backend-neutral attached Android targets on the path to a native Linux compatibility layer\n";
  output << "Scaffold Readiness: " << RenderLoadingBar(phase_progress) << "\n";
  output << "Native Execution Readiness: " << RenderLoadingBar(98) << "\n";
  output << "Execution Focus: P0 Freeze & Triage -> P1 NDK Execution Core -> P2 Window + Graphics\n";
  output << "Legacy Scaffold Phases:\n";

  for (const auto& phase : phases) {
    output << "  " << phase.id << " " << phase.name << " "
           << RenderLoadingBar(phase.progress, 10) << '\n';
  }

  output << "Checkpoint Gates: " << checkpoint_progress << "/100"
         << ", " << completed_checkpoints << "/5 passed\n";
  for (const auto& checkpoint : checkpoints) {
    output << "  " << checkpoint.id << " " << checkpoint.name << " "
           << RenderLoadingBar(static_cast<int>(checkpoint.state), 10) << '\n';
  }

  return output.str();
}

std::string RenderPackageLayoutReport(const PackageLayout& layout) {
  std::ostringstream output;
  output << "Host Package Root: " << layout.host_package_root << '\n';
  output << "Host Manifest: " << layout.host_manifest_path << '\n';
  output << "Host Data Root: " << layout.host_data_root << '\n';
  output << "Host External Data Root: " << layout.host_external_data_root << '\n';
  output << "Host OBB Root: " << layout.host_obb_root << '\n';
  output << "Guest Base APK: " << layout.guest_base_apk << '\n';
  output << "Guest Private Data Root: " << layout.guest_private_data_root << '\n';
  output << "Guest External Data Root: " << layout.guest_external_data_root << '\n';
  output << "Guest OBB Root: " << layout.guest_obb_root << '\n';
  return output.str();
}

std::string DescribeMvpFoundation() {
  std::ostringstream output;
  output << "Project: Linuxoid\n";
  output << "Chosen Language: C++\n";
  output << "Alternative: Rust for later helper services\n";
  output << "Runtime Direction: backend-neutral attached Android targets on the path to a native Linux compatibility layer\n";
  output << "Current State: scaffold 96/100, execution 98/100\n";
  output << "Execution Focus: P0 Freeze & Triage -> P1 NDK Execution Core -> P2 Window + Graphics\n";
  output << "Current Slice: checkpoint engine, package layout planner, manifest/runtime assessor, runtime bridges, backend-neutral installed-package launch plus target discovery/preflight, attached-target package inspection and launcher resolution, Linux desktop launch artifacts with repeatable generic per-app, matrix, and APK-backed verification, the first native spike planner that writes a Linuxoid-owned bundle layout and bootstrap spec for simple APKs, stages host-ABI native libraries plus extracted assets/resources into deterministic bundle roots, a Linuxoid-owned native bootstrap surface that emits a manifest, env script, and local entrypoint stub, the first lifecycle/service shim that turns that bootstrap into deterministic session artifacts and service bindings, a hardened native runner that now `execve`s a controlled child process, sets deterministic cwd plus Linuxoid-only environment variables, closes inherited file descriptors, loads native libraries in deterministic order, calls `JNI_OnLoad`, and emits structured JSON for harnesses, plus headless `ANativeWindow`-shaped first-pixel and callback fixtures, the first real optional Wayland client surface proof, a real optional EGL context plus pbuffer smoke gate, a minimal `ANativeWindow` bridge contract with deterministic geometry updates and stable metadata artifacts, a focused input queue fixture with deterministic pointer/key injection plus focus-ownership artifacts, a local Binder-shaped service manager with deterministic registration, lookup, and transaction artifacts plus a socketpair-backed local transport seam, a new APK resource-readiness bridge that can inspect manifest plus asset metadata from a plain APK/ZIP path before ART/DEX loading exists, a self-healing runtime health skeleton that records subsystem health, chooses bounded recovery actions, emits replayable trace artifacts, writes first-class deterministic recovery-plan files, formalizes those actions with stable rank plus retry-budget plus recovery-scope metadata, now exposes summary fields like dependency-blocked state plus failing-subsystem counts plus recovery-action counts directly in the health JSON, writes a deterministic diagnostic trace index with per-source fingerprints and event boundaries plus a one-shot replayable diagnostic fixture command, now treats application/activity bootstrap planning as a first-class health and replay seam, upgrades execution into its own deterministic supervised bootstrap-execution contract with stable execution-context, runner-script, runner-state, and phase-log artifacts, and keeps live health plus replay fast enough to rerun on staged bundles by reusing one opened APK archive plus the already-staged bundle manifest, alongside a deterministic ART/classloader preparation fixture that inventories dex entries, normalizes manifest target classes, writes classpath artifacts, resolves manifest-target descriptors from real staged DEX contents, attempts a real host-side `dalvikvm -cp <apk> <class>` class-resolution probe when that path is safely available while still reporting runtime absence honestly on hosts without ART, can now route that runtime-smoke plus bootstrap-execution seam through an explicit Linuxoid-owned ART probe override for deterministic fixture runs, and upgrades the bootstrap seam into an application-plus-launcher attempt contract that only trusts a real manifest application class when one is declared.\n";
  output << "Execution Baseline: the FUTO keyboard APK still returns Ready for typing: yes through runtime-backed verification, attached-ADB still verifies a live Settings, Calculator, and F-Droid matrix, Calculator passes `plan-native-spike` with a real native bundle plan and no blockers, the generated Linuxoid-native lifecycle shim now runs locally with `Lifecycle Handoff Ready: yes`, the dex-only Calculator bundle now fails honestly with structured `no_native_libraries_found` output, the planner now reports unsupported ABI libraries without staging them as runnable, the asset bridge can now list normalized asset paths from both staged roots and plain APK/ZIP fixtures, reject traversal, and feed a structured `inspect-apk-resources` readiness report, the hardened native runner still passes the five-second gate against a fixture shared library that exports both `JNI_OnLoad` and `ANativeActivity_onCreate`, the `native-art-classloader-fixture` command now proves Linuxoid can inventory APK dex entries, normalize application plus activity target classes, write deterministic classloader-plan artifacts, and report missing host ART honestly, the `native-art-class-resolution-fixture` command now proves Linuxoid can resolve manifest-target descriptors from real staged DEX contents and write deterministic resolution-map plus trace artifacts without pretending ART already executed them, the `native-art-runtime-smoke` command now proves Linuxoid can select a deterministic manifest-derived class target, carry offline class resolution into deterministic invocation-plan, runtime-log, and JSONL trace artifacts, and attempt a real host-side `dalvikvm -cp <apk> <class>` class-resolution probe when that path is safely available while still reporting runtime absence honestly on this host, while Linuxoid-owned override probes can now exercise that same runtime-smoke path deterministically in fixtures, the upgraded `native-art-activity-bootstrap-fixture` command now proves Linuxoid can normalize a real manifest application class when present, carry launcher resolution plus Binder readiness plus runtime-smoke evidence into deterministic application-plus-activity bootstrap planning artifacts, and mark `activity_bootstrap_readiness` as ready once that plan is materialized, the `native-art-bootstrap-execution-fixture` command now proves Linuxoid can carry that seam one step further into a supervised bootstrap-execution path with deterministic plan, context, runner-script, runner-state, phase-log, trace, and result artifacts, and that same execution seam can now run end to end through a Linuxoid-owned ART probe override for deterministic fixture coverage even on hosts without ART, and runtime health now recognizes that deeper success path by letting `dex_classloader_readiness` plus `bootstrap_execution_readiness` converge to ready when class resolution and supervised bootstrap execution both succeed, and the `native-runtime-health-fixture` plus `native-runtime-diagnostic-replay` commands now prove Linuxoid can classify APK staging, native loading, surface, input, Binder, DEX/classloader, activity-bootstrap planning, and bootstrap-execution readiness separately, expose direct summary fields like `dependency_blocked`, `failing_subsystem_count`, `recovery_actions_selected`, and `failing_subsystems`, emit deterministic recovery plans plus replayable JSONL traces, and rerun live staged-bundle diagnosis quickly enough to be practical by reusing one opened APK archive plus the already-staged bundle manifest, while still leaving full Parcel semantics, real ART/DEX execution, real Android Binder behavior, and full IME/text composition pending.\n";
  output << "Why: this keeps the scaffold useful while turning the first direct-execution slice into something measurable instead of hypothetical.\n";
  return output.str();
}

}  // namespace wfa
