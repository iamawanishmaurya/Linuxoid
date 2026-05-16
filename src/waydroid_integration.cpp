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

int CalculateVerificationProgress(
    const WaydroidPackageVerificationReport& report) {
  int satisfied = 0;
  satisfied += report.direct_launch_ok ? 1 : 0;
  satisfied += report.launcher_generation_ok ? 1 : 0;
  satisfied += report.generated_launcher_ok ? 1 : 0;
  return static_cast<int>(
      std::lround((static_cast<double>(satisfied) / 3.0) * 100.0));
}

bool VerificationPassed(const WaydroidPackageVerificationReport& report) {
  return report.direct_launch_ok && report.launcher_generation_ok &&
         report.generated_launcher_ok;
}

int CalculateMatrixProgress(const WaydroidMatrixReport& report) {
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

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
      output += buffer.data();
    }

    const int rc = pclose(pipe);
    return CommandResult{rc, output};
  };
}

}  // namespace

WaydroidPackageVerificationReport VerifyWaydroidPackageWithRunners(
    const WaydroidPackageVerificationSpec& spec,
    const CommandRunner& runtime_runner,
    const CommandRunner& launcher_runner) {
  if (spec.package_name.empty() || spec.compatctl_path.empty() ||
      spec.desktop_root.empty()) {
    throw std::invalid_argument(
        "waydroid package verification spec is missing required fields");
  }

  WaydroidPackageVerificationReport report;
  report.app_name = spec.app_name.empty() ? spec.package_name : spec.app_name;
  report.package_name = spec.package_name;

  const auto direct_report =
      LaunchWaydroidAppWithRunner(spec.package_name, runtime_runner);
  report.direct_launch_ok = direct_report.launch_ok;
  report.direct_launch_output = direct_report.output;

  report.artifacts = CreateWaydroidDesktopLaunchArtifacts(
      {.app_name = report.app_name,
       .package_name = spec.package_name,
       .compatctl_path = spec.compatctl_path,
       .desktop_root = spec.desktop_root,
       .launcher_root = spec.launcher_root});
  report.launcher_generation_ok = report.artifacts.host_launch_ready;

  const auto launcher_result = launcher_runner(report.artifacts.script_path);
  report.generated_launcher_ok = launcher_result.exit_code == 0;
  report.generated_launcher_output = launcher_result.output;

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

  WaydroidMatrixReport report;
  report.artifact_root = artifact_root;

  const fs::path root(artifact_root);
  const std::string desktop_root = (root / "applications").string();
  const std::string launcher_root = (root / "launchers").string();

  for (const auto& package_name : packages) {
    WaydroidMatrixEntry entry;
    entry.package_name = package_name;
    try {
      entry.verification = VerifyWaydroidPackageWithRunners(
          {.app_name = package_name,
           .package_name = package_name,
           .compatctl_path = compatctl_path,
           .desktop_root = desktop_root,
           .launcher_root = launcher_root},
          runtime_runner, launcher_runner);
      entry.verification_ok = VerificationPassed(entry.verification);
    } catch (const std::exception& error) {
      entry.error = error.what();
      entry.verification.app_name = package_name;
      entry.verification.package_name = package_name;
    }
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
  std::ostringstream output;
  output << "Verification Loading: "
         << RenderLoadingBar(CalculateVerificationProgress(report), 10) << '\n';
  output << "Runtime Backend: waydroid\n";
  output << "App Name: " << report.app_name << '\n';
  output << "Package: " << report.package_name << '\n';
  output << "Direct Launch OK: " << (report.direct_launch_ok ? "yes" : "no")
         << '\n';
  output << "Launcher Generation OK: "
         << (report.launcher_generation_ok ? "yes" : "no") << '\n';
  output << "Generated Launcher OK: "
         << (report.generated_launcher_ok ? "yes" : "no") << '\n';
  output << "Launcher Script: " << report.artifacts.script_path << '\n';
  output << "Desktop Entry: " << report.artifacts.desktop_file_path << '\n';
  output << "Direct Launch Output:\n" << report.direct_launch_output;
  output << "Generated Launcher Output:\n" << report.generated_launcher_output;
  return output.str();
}

std::string RenderWaydroidMatrixReport(const WaydroidMatrixReport& report) {
  int passed = 0;
  for (const auto& entry : report.entries) {
    passed += entry.verification_ok ? 1 : 0;
  }

  std::ostringstream output;
  output << "Matrix Loading: "
         << RenderLoadingBar(CalculateMatrixProgress(report), 10) << '\n';
  output << "Runtime Backend: waydroid\n";
  output << "Artifact Root: " << report.artifact_root << '\n';
  output << "Packages Passed: " << passed << "/" << report.entries.size()
         << '\n';
  for (const auto& entry : report.entries) {
    output << "  " << entry.package_name << " "
           << RenderLoadingBar(CalculateVerificationProgress(entry.verification),
                               10)
           << " " << (entry.verification_ok ? "pass" : "fail")
           << " direct="
           << (entry.verification.direct_launch_ok ? "yes" : "no")
           << " launcher="
           << (entry.verification.launcher_generation_ok ? "yes" : "no")
           << " generated="
           << (entry.verification.generated_launcher_ok ? "yes" : "no")
           << '\n';
    if (!entry.error.empty()) {
      output << "    Error: " << entry.error << '\n';
    }
  }

  return output.str();
}

}  // namespace wfa
