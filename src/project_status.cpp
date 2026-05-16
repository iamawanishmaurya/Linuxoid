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
      {"P4", "Runtime and service contract", 95,
       "The project can load an APK into a compat root, install it on a live target, provision its IME service, inspect attached-target package metadata, auto-resolve simple launcher components, route installed-package launch through a backend-neutral contract, preflight attached Android targets before launch, materialize a Linuxoid-owned native bundle plan plus bootstrap spec for simple foreground APKs, emit a Linuxoid-owned native bootstrap manifest plus entrypoint stub for those candidates, and hand that bootstrap off to a Linuxoid-owned lifecycle and service shim with deterministic session artifacts."},
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
  output << "Phase Loading: " << RenderLoadingBar(phase_progress) << "\n";

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
  output << "Current Slice: checkpoint engine, package layout planner, manifest/runtime assessor, runtime bridges, backend-neutral installed-package launch plus target discovery/preflight, attached-target package inspection and launcher resolution, Linux desktop launch artifacts with repeatable generic per-app, matrix, and APK-backed verification, the first native spike planner that writes a Linuxoid-owned bundle layout and bootstrap spec for simple APKs, a Linuxoid-owned native bootstrap surface that emits a manifest, env script, and local entrypoint stub, and the first lifecycle/service shim that turns that bootstrap into deterministic session artifacts and service bindings\n";
  output << "Golden App Proof: the FUTO keyboard APK now returns Ready for typing: yes through both a Linuxoid-generated launcher and the local-APK verifier on live Waydroid, attached-ADB now auto-resolves installed-app launchers and verifies a live Settings, Calculator, and F-Droid matrix without explicit component input, Calculator now passes `plan-native-spike` with a real native bundle plan and no blockers, and the generated Linuxoid-native lifecycle shim now runs locally with `Lifecycle Handoff Ready: yes` and `Execution Engine Ready: no` without depending on Waydroid at invocation time.\n";
  output << "Why: this keeps the first executable slice aligned with the future core.\n";
  return output.str();
}

}  // namespace wfa
