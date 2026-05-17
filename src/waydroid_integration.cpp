#include "wfa/waydroid_integration.hpp"

#include "wfa/checkpoint.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace wfa {

namespace {

namespace fs = std::filesystem;

std::string QuoteForShell(const std::string& value) {
  std::string quoted = "'";
  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(character);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

int CalculateInstalledVerificationProgress(
    const InstalledPackageVerificationReport& report) {
  int satisfied = 0;
  satisfied += report.preflight_ok ? 1 : 0;
  satisfied += report.direct_launch_ok ? 1 : 0;
  satisfied += report.launcher_generation_ok ? 1 : 0;
  satisfied += report.generated_launcher_ok ? 1 : 0;
  return static_cast<int>(
      std::lround((static_cast<double>(satisfied) / 4.0) * 100.0));
}

bool VerificationPassed(const InstalledPackageVerificationReport& report) {
  return report.preflight_ok && report.direct_launch_ok &&
         report.launcher_generation_ok &&
         report.generated_launcher_ok;
}

int CalculateInstalledMatrixProgress(const InstalledPackageMatrixReport& report) {
  if (report.entries.empty()) {
    return 0;
  }

  int passed = 0;
  for (const auto& entry : report.entries) {
    passed += entry.verification_ok ? 1 : 0;
  }

  return static_cast<int>(
      std::lround((static_cast<double>(passed) / report.entries.size()) *
                  100.0));
}

CommandRunner MakeShellRunner() {
  return [](const std::string& command) -> CommandResult {
    const std::string captured_command = command + " 2>&1";
    std::array<char, 4096> buffer{};
    std::string output;

    FILE* pipe = popen(captured_command.c_str(), "r");
    if (pipe == nullptr) {
      throw std::runtime_error("failed to start command: " + command);
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
           nullptr) {
      output += buffer.data();
    }

    const int rc = pclose(pipe);
    return CommandResult{rc, output};
  };
}

}  // namespace

InstalledPackageVerificationReport VerifyInstalledPackageWithRunners(
    const InstalledPackageVerificationSpec& spec,
    const CommandRunner& runtime_runner,
    const CommandRunner& launcher_runner) {
  if (spec.package_name.empty() || spec.compatctl_path.empty() ||
      spec.desktop_root.empty()) {
    throw std::invalid_argument(
        "installed package verification spec is missing required fields");
  }

  InstalledPackageVerificationReport report;
  report.backend_name = RenderRuntimeBackendName(spec.backend);
  report.app_name = spec.app_name.empty() ? spec.package_name : spec.app_name;
  report.serial = spec.serial;
  report.package_name = spec.package_name;
  report.component = spec.component;

  const auto preflight = PreflightRuntimeWithRunner(
      {.backend = spec.backend,
       .serial = spec.serial,
       .package_name = spec.package_name,
       .component = spec.component},
      runtime_runner);
  report.serial = preflight.serial.empty() ? spec.serial : preflight.serial;
  report.component = preflight.component.empty() ? spec.component : preflight.component;
  report.preflight_ok = preflight.ready_for_launch;
  report.target_discovered = preflight.target_discovered;
  report.package_visible = preflight.package_visible;
  report.component_ready = preflight.component_ready;
  report.preflight_output = RenderRuntimePreflightReport(preflight);
  report.preflight_notes = preflight.notes;

  if (preflight.ready_for_launch) {
    const auto direct_report = LaunchInstalledAppWithRunner(
        {.backend = spec.backend,
         .serial = report.serial,
         .package_name = spec.package_name,
         .component = report.component},
        runtime_runner);
    if (!direct_report.component.empty()) {
      report.component = direct_report.component;
    }
    report.direct_launch_ok = direct_report.launch_ok;
    report.direct_launch_output = direct_report.output;
  } else {
    report.direct_launch_ok = false;
    report.direct_launch_output = report.preflight_output;
  }

  try {
    report.artifacts = CreateInstalledPackageDesktopLaunchArtifacts(
        {.backend = spec.backend,
         .app_name = report.app_name,
         .serial = report.serial,
         .package_name = spec.package_name,
         .component = report.component,
         .compatctl_path = spec.compatctl_path,
         .desktop_root = spec.desktop_root,
         .launcher_root = spec.launcher_root});
    report.launcher_generation_ok = report.artifacts.host_launch_ready;
    if (report.launcher_generation_ok) {
      const auto launcher_result = launcher_runner(report.artifacts.script_path);
      report.generated_launcher_ok = launcher_result.exit_code == 0;
      report.generated_launcher_output = launcher_result.output;
    }
  } catch (const std::exception& error) {
    report.launcher_generation_ok = false;
    report.generated_launcher_ok = false;
    report.generated_launcher_output = std::string(error.what()) + "\n";
  }
  return report;
}

InstalledPackageVerificationReport VerifyInstalledPackage(
    const InstalledPackageVerificationSpec& spec) {
  const auto shell_runner = MakeShellRunner();
  const auto launcher_runner = [&](const std::string& script_path) {
    return shell_runner(QuoteForShell(script_path));
  };
  return VerifyInstalledPackageWithRunners(spec, shell_runner, launcher_runner);
}

InstalledPackageMatrixReport VerifyInstalledPackageMatrixWithRunners(
    const std::vector<InstalledPackageVerificationSpec>& specs,
    const std::string& artifact_root, const CommandRunner& runtime_runner,
    const CommandRunner& launcher_runner) {
  if (specs.empty() || artifact_root.empty()) {
    throw std::invalid_argument(
        "installed package matrix is missing required fields");
  }

  InstalledPackageMatrixReport report;
  report.backend_name = RenderRuntimeBackendName(specs.front().backend);
  report.artifact_root = artifact_root;
  report.serial = specs.front().serial;

  const fs::path root(artifact_root);
  const std::string desktop_root = (root / "applications").string();
  const std::string launcher_root = (root / "launchers").string();

  for (const auto& spec : specs) {
    InstalledPackageMatrixEntry entry;
    entry.backend_name = RenderRuntimeBackendName(spec.backend);
    entry.serial = spec.serial;
    entry.package_name = spec.package_name;
    entry.component = spec.component;
    try {
      entry.verification = VerifyInstalledPackageWithRunners(
          {.backend = spec.backend,
           .app_name = spec.app_name.empty() ? spec.package_name : spec.app_name,
           .serial = spec.serial,
           .package_name = spec.package_name,
           .component = spec.component,
           .compatctl_path = spec.compatctl_path,
           .desktop_root = desktop_root,
           .launcher_root = launcher_root},
          runtime_runner, launcher_runner);
      entry.verification_ok = VerificationPassed(entry.verification);
      if (report.serial.empty() && !entry.verification.serial.empty()) {
        report.serial = entry.verification.serial;
      }
    } catch (const std::exception& error) {
      entry.error = error.what();
      entry.verification.backend_name = entry.backend_name;
      entry.verification.app_name = spec.package_name;
      entry.verification.serial = spec.serial;
      entry.verification.package_name = spec.package_name;
      entry.verification.component = spec.component;
    }
    report.entries.push_back(entry);
  }

  return report;
}

InstalledPackageMatrixReport VerifyInstalledPackageMatrix(
    const std::vector<InstalledPackageVerificationSpec>& specs,
    const std::string& artifact_root) {
  const auto shell_runner = MakeShellRunner();
  const auto launcher_runner = [&](const std::string& script_path) {
    return shell_runner(QuoteForShell(script_path));
  };
  return VerifyInstalledPackageMatrixWithRunners(
      specs, artifact_root, shell_runner, launcher_runner);
}

std::string RenderInstalledPackageVerificationReport(
    const InstalledPackageVerificationReport& report) {
  std::ostringstream output;
  output << "Verification Loading: "
         << RenderLoadingBar(CalculateInstalledVerificationProgress(report), 10)
         << '\n';
  output << "Runtime Backend: " << report.backend_name << '\n';
  output << "App Name: " << report.app_name << '\n';
  output << "Package: " << report.package_name << '\n';
  output << "Runtime Target: " << report.serial << '\n';
  output << "Component: " << report.component << '\n';
  output << "Target Discovered: " << (report.target_discovered ? "yes" : "no")
         << '\n';
  output << "Preflight OK: " << (report.preflight_ok ? "yes" : "no") << '\n';
  output << "Package Visible: " << (report.package_visible ? "yes" : "no")
         << '\n';
  output << "Component Ready: " << (report.component_ready ? "yes" : "no")
         << '\n';
  output << "Preflight Notes: " << report.preflight_notes << '\n';
  output << "Direct Launch OK: " << (report.direct_launch_ok ? "yes" : "no")
         << '\n';
  output << "Launcher Generation OK: "
         << (report.launcher_generation_ok ? "yes" : "no") << '\n';
  output << "Generated Launcher OK: "
         << (report.generated_launcher_ok ? "yes" : "no") << '\n';
  output << "Launcher Script: " << report.artifacts.script_path << '\n';
  output << "Desktop Entry: " << report.artifacts.desktop_file_path << '\n';
  output << "Preflight Output:\n" << report.preflight_output;
  output << "Direct Launch Output:\n" << report.direct_launch_output;
  output << "Generated Launcher Output:\n" << report.generated_launcher_output;
  return output.str();
}

std::string RenderInstalledPackageMatrixReport(
    const InstalledPackageMatrixReport& report) {
  int passed = 0;
  for (const auto& entry : report.entries) {
    passed += entry.verification_ok ? 1 : 0;
  }

  std::ostringstream output;
  output << "Matrix Loading: "
         << RenderLoadingBar(CalculateInstalledMatrixProgress(report), 10)
         << '\n';
  output << "Runtime Backend: " << report.backend_name << '\n';
  output << "Artifact Root: " << report.artifact_root << '\n';
  output << "Runtime Target: " << report.serial << '\n';
  output << "Packages Passed: " << passed << "/" << report.entries.size()
         << '\n';
  for (const auto& entry : report.entries) {
    output << "  " << entry.package_name << " "
           << RenderLoadingBar(
                  CalculateInstalledVerificationProgress(entry.verification), 10)
           << " " << (entry.verification_ok ? "pass" : "fail")
           << " preflight="
           << (entry.verification.preflight_ok ? "yes" : "no")
           << " direct="
           << (entry.verification.direct_launch_ok ? "yes" : "no")
           << " launcher="
           << (entry.verification.launcher_generation_ok ? "yes" : "no")
           << " generated="
           << (entry.verification.generated_launcher_ok ? "yes" : "no")
           << '\n';
    if (!entry.component.empty()) {
      output << "    Component: " << entry.component << '\n';
    }
    if (!entry.error.empty()) {
      output << "    Error: " << entry.error << '\n';
    }
  }
  return output.str();
}

WaydroidPackageVerificationReport VerifyWaydroidPackageWithRunners(
    const WaydroidPackageVerificationSpec& spec,
    const CommandRunner& runtime_runner,
    const CommandRunner& launcher_runner) {
  if (spec.package_name.empty() || spec.compatctl_path.empty() ||
      spec.desktop_root.empty()) {
    throw std::invalid_argument(
        "waydroid package verification spec is missing required fields");
  }

  const auto generic = VerifyInstalledPackageWithRunners(
      {.backend = RuntimeBackendKind::kWaydroid,
       .app_name = spec.app_name,
       .package_name = spec.package_name,
       .compatctl_path = spec.compatctl_path,
       .desktop_root = spec.desktop_root,
       .launcher_root = spec.launcher_root},
      runtime_runner, launcher_runner);

  WaydroidPackageVerificationReport report;
  report.app_name = generic.app_name;
  report.serial = generic.serial;
  report.package_name = generic.package_name;
  report.component = generic.component;
  report.preflight_ok = generic.preflight_ok;
  report.target_discovered = generic.target_discovered;
  report.package_visible = generic.package_visible;
  report.component_ready = generic.component_ready;
  report.preflight_output = generic.preflight_output;
  report.preflight_notes = generic.preflight_notes;
  report.direct_launch_ok = generic.direct_launch_ok;
  report.launcher_generation_ok = generic.launcher_generation_ok;
  report.generated_launcher_ok = generic.generated_launcher_ok;
  report.direct_launch_output = generic.direct_launch_output;
  report.generated_launcher_output = generic.generated_launcher_output;
  report.artifacts = CreateWaydroidDesktopLaunchArtifacts(
      {.app_name = report.app_name,
       .package_name = report.package_name,
       .compatctl_path = spec.compatctl_path,
       .desktop_root = spec.desktop_root,
       .launcher_root = spec.launcher_root});
  return report;
}

WaydroidPackageVerificationReport VerifyWaydroidPackage(
    const WaydroidPackageVerificationSpec& spec) {
  const auto shell_runner = MakeShellRunner();
  const auto launcher_runner = [&](const std::string& script_path) {
    return shell_runner(QuoteForShell(script_path));
  };
  return VerifyWaydroidPackageWithRunners(spec, shell_runner, launcher_runner);
}

WaydroidMatrixReport VerifyWaydroidPackageMatrixWithRunners(
    const std::vector<std::string>& packages, const std::string& compatctl_path,
    const std::string& artifact_root, const CommandRunner& runtime_runner,
    const CommandRunner& launcher_runner) {
  if (packages.empty() || compatctl_path.empty() || artifact_root.empty()) {
    throw std::invalid_argument(
        "waydroid package matrix is missing required fields");
  }

  std::vector<InstalledPackageVerificationSpec> specs;
  specs.reserve(packages.size());
  for (const auto& package_name : packages) {
    specs.push_back({.backend = RuntimeBackendKind::kWaydroid,
                     .app_name = package_name,
                     .package_name = package_name,
                     .compatctl_path = compatctl_path});
  }

  const auto generic = VerifyInstalledPackageMatrixWithRunners(
      specs, artifact_root, runtime_runner, launcher_runner);

  WaydroidMatrixReport report;
  report.artifact_root = generic.artifact_root;
  for (const auto& generic_entry : generic.entries) {
    WaydroidMatrixEntry entry;
    entry.package_name = generic_entry.package_name;
    entry.verification_ok = generic_entry.verification_ok;
    entry.error = generic_entry.error;
    entry.verification.app_name = generic_entry.verification.app_name;
    entry.verification.package_name = generic_entry.verification.package_name;
    entry.verification.direct_launch_ok =
        generic_entry.verification.direct_launch_ok;
    entry.verification.launcher_generation_ok =
        generic_entry.verification.launcher_generation_ok;
    entry.verification.generated_launcher_ok =
        generic_entry.verification.generated_launcher_ok;
    entry.verification.direct_launch_output =
        generic_entry.verification.direct_launch_output;
    entry.verification.generated_launcher_output =
        generic_entry.verification.generated_launcher_output;
    entry.verification.artifacts = CreateWaydroidDesktopLaunchArtifacts(
        {.app_name = generic_entry.verification.app_name,
         .package_name = generic_entry.verification.package_name,
         .compatctl_path = compatctl_path,
         .desktop_root = generic_entry.verification.artifacts.desktop_root,
         .launcher_root = generic_entry.verification.artifacts.launcher_root});
    report.entries.push_back(entry);
  }

  return report;
}

WaydroidMatrixReport VerifyWaydroidPackageMatrix(
    const std::vector<std::string>& packages, const std::string& compatctl_path,
    const std::string& artifact_root) {
  const auto shell_runner = MakeShellRunner();
  const auto launcher_runner = [&](const std::string& script_path) {
    return shell_runner(QuoteForShell(script_path));
  };
  return VerifyWaydroidPackageMatrixWithRunners(
      packages, compatctl_path, artifact_root, shell_runner, launcher_runner);
}

std::string RenderWaydroidPackageVerificationReport(
    const WaydroidPackageVerificationReport& report) {
  return RenderInstalledPackageVerificationReport(
      {.backend_name = "waydroid",
       .app_name = report.app_name,
       .serial = report.serial,
       .package_name = report.package_name,
       .component = report.component,
       .preflight_ok = report.preflight_ok,
       .target_discovered = report.target_discovered,
       .package_visible = report.package_visible,
       .component_ready = report.component_ready,
       .direct_launch_ok = report.direct_launch_ok,
       .launcher_generation_ok = report.launcher_generation_ok,
       .generated_launcher_ok = report.generated_launcher_ok,
       .preflight_output = report.preflight_output,
       .preflight_notes = report.preflight_notes,
       .direct_launch_output = report.direct_launch_output,
       .generated_launcher_output = report.generated_launcher_output,
       .artifacts = {.backend_name = "waydroid",
                     .app_name = report.artifacts.app_name,
                     .package_name = report.artifacts.package_name,
                     .compatctl_path = report.artifacts.compatctl_path,
                     .desktop_root = report.artifacts.desktop_root,
                     .launcher_root = report.artifacts.launcher_root,
                     .script_path = report.artifacts.script_path,
                     .desktop_file_path = report.artifacts.desktop_file_path,
                     .command_line = report.artifacts.command_line,
                     .host_launch_ready = report.artifacts.host_launch_ready}});
}

std::string RenderWaydroidMatrixReport(const WaydroidMatrixReport& report) {
  InstalledPackageMatrixReport generic;
  generic.backend_name = "waydroid";
  generic.artifact_root = report.artifact_root;
  for (const auto& entry : report.entries) {
    generic.entries.push_back(
        {.backend_name = "waydroid",
         .package_name = entry.package_name,
         .verification_ok = entry.verification_ok,
         .error = entry.error,
         .verification =
             {.backend_name = "waydroid",
              .app_name = entry.verification.app_name,
              .serial = entry.verification.serial,
              .package_name = entry.verification.package_name,
              .component = entry.verification.component,
              .preflight_ok = entry.verification.preflight_ok,
              .target_discovered = entry.verification.target_discovered,
              .package_visible = entry.verification.package_visible,
              .component_ready = entry.verification.component_ready,
              .direct_launch_ok = entry.verification.direct_launch_ok,
              .launcher_generation_ok =
                  entry.verification.launcher_generation_ok,
              .generated_launcher_ok = entry.verification.generated_launcher_ok,
              .preflight_output = entry.verification.preflight_output,
              .preflight_notes = entry.verification.preflight_notes,
              .direct_launch_output = entry.verification.direct_launch_output,
              .generated_launcher_output =
                  entry.verification.generated_launcher_output,
              .artifacts =
                  {.backend_name = "waydroid",
                   .app_name = entry.verification.artifacts.app_name,
                   .package_name = entry.verification.artifacts.package_name,
                   .compatctl_path =
                       entry.verification.artifacts.compatctl_path,
                   .desktop_root = entry.verification.artifacts.desktop_root,
                   .launcher_root = entry.verification.artifacts.launcher_root,
                   .script_path = entry.verification.artifacts.script_path,
                   .desktop_file_path =
                       entry.verification.artifacts.desktop_file_path,
                   .command_line = entry.verification.artifacts.command_line,
                   .host_launch_ready =
                       entry.verification.artifacts.host_launch_ready}}});
  }
  return RenderInstalledPackageMatrixReport(generic);
}

}  // namespace wfa
