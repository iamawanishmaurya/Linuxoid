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
      {"P4", "Runtime and service contract", 84,
       "The project can load an APK into a compat root, install it on a live target, provision its IME service, and route installed-package launch through a backend-neutral contract."},
      {"P5", "Graphics and host integration", 78,
       "Linux launcher artifacts now target a backend-neutral installed-package launch seam while preserving the verified APK-backed path and the current Waydroid adapter."},
      {"P6", "APK execution and validation", 86,
       "A live Linux host launcher now proves the local keyboard and F-Droid APK flows plus Calculator, Settings, and installed-package F-Droid launch flows on Waydroid, including Ready-for-typing verification for the IME case, while the core installed-package launch path is no longer Waydroid-only."},
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
       "The keyboard, Calculator, Settings, and F-Droid launch paths are now verified on live Waydroid, but the wider app matrix is still incomplete."},
      {"C4", "Host Integration", 20, CompletionState::kInValidation,
       "Linux wrappers and `.desktop` entries now launch APK-backed and multiple installed Waydroid apps from the host, but compositor, clipboard, and richer input handoff still need proof."},
      {"C5", "Repeatability and Regression Guard", 20,
       CompletionState::kComplete,
       "The build, tests, and live keyboard provisioning flow all passed again without code changes."},
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
  output << "Current Slice: checkpoint engine, package layout planner, manifest/runtime assessor, runtime bridges, a backend-neutral installed-package launch seam, and Linux desktop launch artifacts with repeatable per-app, matrix, and APK-backed Waydroid verification\n";
  output << "Golden App Proof: the FUTO keyboard APK now returns Ready for typing: yes through both a Linuxoid-generated launcher and the local-APK verifier on live Waydroid, while F-Droid now verifies through both the local-APK and installed-package Linux launch paths alongside Calculator and Settings.\n";
  output << "Why: this keeps the first executable slice aligned with the future core.\n";
  return output.str();
}

}  // namespace wfa
