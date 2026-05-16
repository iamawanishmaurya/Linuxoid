#include "wfa/apk_host_integration.hpp"

#include "wfa/checkpoint.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <stdexcept>

namespace wfa {

namespace {

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

int CalculateVerificationProgress(const ApkHostVerificationReport& report) {
  int satisfied = 0;
  satisfied += report.apk_load_ok ? 1 : 0;
  satisfied += report.launcher_generation_ok ? 1 : 0;
  satisfied += report.generated_launcher_ok ? 1 : 0;
  return static_cast<int>(
      std::lround((static_cast<double>(satisfied) / 3.0) * 100.0));
}

bool GeneratedLauncherOutputLooksSuccessful(
    const DesktopLaunchArtifacts& artifacts, const CommandResult& result) {
  if (result.exit_code != 0) {
    return false;
  }

  if (artifacts.uses_provision_mode) {
    return result.output.find("Ready for typing: yes") != std::string::npos;
  }

  return result.output.find("Launch OK: yes") != std::string::npos;
}

}  // namespace

ApkHostVerificationReport VerifyLoadedApkHostLaunchAutoWithRunner(
    const LoadedApkReport& report, const ApkHostVerificationSpec& spec,
    const CommandRunner& launcher_runner) {
  if (spec.serial.empty() || spec.compatctl_path.empty() ||
      spec.desktop_root.empty()) {
    throw std::invalid_argument(
        "apk host verification spec is missing required fields");
  }

  ApkHostVerificationReport verification;
  verification.loaded_apk = report;
  verification.apk_load_ok = true;

  verification.artifacts = CreateDesktopLaunchArtifactsForLoadedApkAuto(
      report, spec.serial, spec.compatctl_path, spec.desktop_root,
      spec.launcher_root);
  verification.launcher_generation_ok =
      verification.artifacts.host_launch_ready;

  const auto launcher_result =
      launcher_runner(verification.artifacts.script_path);
  verification.generated_launcher_output = launcher_result.output;
  verification.generated_launcher_ok = GeneratedLauncherOutputLooksSuccessful(
      verification.artifacts, launcher_result);

  return verification;
}

ApkHostVerificationReport VerifyLoadedApkHostLaunchAuto(
    const LoadedApkReport& report, const ApkHostVerificationSpec& spec) {
  const auto shell_runner = MakeShellRunner();
  const auto launcher_runner = [&](const std::string& script_path) {
    return shell_runner(QuoteForShell(script_path));
  };
  return VerifyLoadedApkHostLaunchAutoWithRunner(report, spec, launcher_runner);
}

ApkHostVerificationReport VerifyApkHostLaunchAuto(
    const std::string& apk_path, const std::string& compat_root,
    const ApkHostVerificationSpec& spec) {
  auto verification = VerifyLoadedApkHostLaunchAuto(
      LoadApkToCompatRoot(apk_path, compat_root), spec);
  verification.loaded_apk.apk_path = apk_path;
  verification.apk_load_ok = true;
  return verification;
}

std::string RenderApkHostVerificationReport(
    const ApkHostVerificationReport& report) {
  std::ostringstream output;
  output << "Verification Loading: "
         << RenderLoadingBar(CalculateVerificationProgress(report), 10) << '\n';
  output << "Runtime Path: apk-backed-linux-launch\n";
  output << "APK Path: " << report.loaded_apk.apk_path << '\n';
  output << "Package: " << report.loaded_apk.manifest_profile.package_name
         << '\n';
  output << "Install ID: " << report.loaded_apk.install_id << '\n';
  output << "Launch Mode: "
         << (report.artifacts.uses_provision_mode ? "provision-ime"
                                                  : "launch-activity")
         << '\n';
  output << "Launcher Selection: "
         << (report.artifacts.launcher_component_inferred ? "inferred"
                                                          : "explicit")
         << '\n';
  output << "Launcher Component: " << report.artifacts.launcher_component
         << '\n';
  output << "IME Component: " << report.artifacts.ime_component << '\n';
  output << "APK Load OK: " << (report.apk_load_ok ? "yes" : "no") << '\n';
  output << "Launcher Generation OK: "
         << (report.launcher_generation_ok ? "yes" : "no") << '\n';
  output << "Generated Launcher OK: "
         << (report.generated_launcher_ok ? "yes" : "no") << '\n';
  output << "Install Root: " << report.loaded_apk.install_root << '\n';
  output << "Launcher Script: " << report.artifacts.script_path << '\n';
  output << "Desktop Entry: " << report.artifacts.desktop_file_path << '\n';
  output << "Generated Launcher Output:\n"
         << report.generated_launcher_output;
  return output.str();
}

}  // namespace wfa
