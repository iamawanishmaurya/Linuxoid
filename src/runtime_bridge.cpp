#include "wfa/runtime_bridge.hpp"

#include "wfa/apk_loader.hpp"
#include "wfa/checkpoint.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace wfa {

RuntimeBackendKind ParseRuntimeBackendKind(const std::string& backend_name) {
  if (backend_name == "waydroid") {
    return RuntimeBackendKind::kWaydroid;
  }
  if (backend_name == "attached-adb") {
    return RuntimeBackendKind::kAttachedAdb;
  }
  if (backend_name == "native") {
    return RuntimeBackendKind::kNative;
  }
  throw std::invalid_argument("unsupported runtime backend: " + backend_name);
}

std::string RenderRuntimeBackendName(RuntimeBackendKind backend) {
  switch (backend) {
    case RuntimeBackendKind::kWaydroid:
      return "waydroid";
    case RuntimeBackendKind::kAttachedAdb:
      return "attached-adb";
    case RuntimeBackendKind::kNative:
      return "native";
  }
  throw std::invalid_argument("unsupported runtime backend enum");
}

namespace {

struct StatusQueryAttempt {
  bool ok = false;
  std::string error;
  AdbImeStatus status;
};

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

CommandResult RunCommandCaptureAllowFailure(const std::string& command) {
  std::array<char, 4096> buffer{};
  std::string output;
  const std::string captured_command = command + " 2>&1";

  FILE* pipe = popen(captured_command.c_str(), "r");
  if (pipe == nullptr) {
    throw std::runtime_error("failed to start command: " + command);
  }

  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }

  const int rc = pclose(pipe);
  return CommandResult{rc, output};
}

CommandResult RequireSuccessfulCommand(const CommandRunner& runner,
                                       const std::string& command) {
  const auto result = runner(command);
  if (result.exit_code != 0) {
    throw std::runtime_error("command failed: " + command + "\n" + result.output);
  }
  return result;
}

CommandRunner MakeShellRunner() {
  return [](const std::string& command) {
    return RunCommandCaptureAllowFailure(command);
  };
}

std::string BuildAdbPrefix(const std::string& serial) {
  return "adb -s " + QuoteForShell(serial);
}

bool IsValidPackageName(const std::string& package_name) {
  static const std::regex pattern(
      R"(^[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+$)");
  return std::regex_match(package_name, pattern);
}

std::vector<std::string> SplitLines(const std::string& output) {
  std::vector<std::string> lines;
  std::istringstream input(output);
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(line);
  }
  return lines;
}

std::string CanonicalizeAndroidComponent(const std::string& component) {
  const auto slash = component.find('/');
  if (slash == std::string::npos) {
    return component;
  }

  const std::string package_name = component.substr(0, slash);
  std::string class_name = component.substr(slash + 1);
  if (!class_name.empty() && class_name.front() == '.') {
    class_name = package_name + class_name;
  }
  return package_name + "/" + class_name;
}

std::string ShortenAndroidComponent(const std::string& component) {
  const std::string canonical = CanonicalizeAndroidComponent(component);
  const auto slash = canonical.find('/');
  if (slash == std::string::npos) {
    return canonical;
  }

  const std::string package_name = canonical.substr(0, slash);
  const std::string class_name = canonical.substr(slash + 1);
  if (class_name.rfind(package_name + ".", 0) == 0) {
    return package_name + "/." + class_name.substr(package_name.size() + 1);
  }

  return canonical;
}

bool AndroidComponentsEquivalent(const std::string& left,
                                 const std::string& right) {
  return CanonicalizeAndroidComponent(left) ==
         CanonicalizeAndroidComponent(right);
}

int CalculateProvisioningProgress(const AdbProvisioningReport& report) {
  int total = 9;
  int satisfied = 0;

  if (!report.settings_component.empty()) {
    ++total;
  }

  satisfied += report.apk_matches_requested_package ? 1 : 0;
  satisfied += report.install_ok ? 1 : 0;
  satisfied += report.enable_ok ? 1 : 0;
  satisfied += report.set_ok ? 1 : 0;
  satisfied += report.readback_ok ? 1 : 0;
  satisfied += report.final_status.package_installed ? 1 : 0;
  satisfied += report.final_status.ime_registered ? 1 : 0;
  satisfied += report.final_status.ime_enabled ? 1 : 0;
  satisfied += report.final_status.is_default_ime ? 1 : 0;

  if (!report.settings_component.empty() && report.final_status.settings_launch_ok) {
    ++satisfied;
  }

  if (total == 0) {
    return 0;
  }

  return static_cast<int>(
      std::lround((static_cast<double>(satisfied) / total) * 100.0));
}

StatusQueryAttempt TryQueryAdbImeStatusWithRunner(
    const std::string& serial, const std::string& package_name,
    const std::string& ime_id, const std::string& settings_component,
    const CommandRunner& runner) {
  StatusQueryAttempt attempt;
  attempt.status.serial = serial;
  attempt.status.package_name = package_name;
  attempt.status.ime_id = ime_id;
  attempt.status.settings_component = settings_component;

  const std::string prefix = BuildAdbPrefix(serial);

  const auto package_result = runner(prefix + " shell pm list packages");
  if (package_result.exit_code != 0) {
    attempt.error = "command failed: " + prefix + " shell pm list packages\n" +
                    package_result.output;
    return attempt;
  }
  attempt.status.package_installed =
      OutputContainsInstalledPackage(package_result.output, package_name);

  const auto ime_result = runner(prefix + " shell ime list -a");
  if (ime_result.exit_code != 0) {
    attempt.error =
        "command failed: " + prefix + " shell ime list -a\n" + ime_result.output;
    return attempt;
  }
  attempt.status.ime_registered = OutputContainsImeId(ime_result.output, ime_id);

  const auto enabled_result =
      runner(prefix + " shell settings get secure enabled_input_methods");
  if (enabled_result.exit_code != 0) {
    attempt.error = "command failed: " + prefix +
                    " shell settings get secure enabled_input_methods\n" +
                    enabled_result.output;
    return attempt;
  }
  attempt.status.enabled_input_methods = enabled_result.output;
  while (!attempt.status.enabled_input_methods.empty() &&
         (attempt.status.enabled_input_methods.back() == '\n' ||
          attempt.status.enabled_input_methods.back() == '\r')) {
    attempt.status.enabled_input_methods.pop_back();
  }
  attempt.status.ime_enabled = EnabledInputMethodsContainIme(
      attempt.status.enabled_input_methods, ime_id);

  const auto default_result =
      runner(prefix + " shell settings get secure default_input_method");
  if (default_result.exit_code != 0) {
    attempt.error = "command failed: " + prefix +
                    " shell settings get secure default_input_method\n" +
                    default_result.output;
    return attempt;
  }

  attempt.status.default_input_method = default_result.output;
  while (!attempt.status.default_input_method.empty() &&
         (attempt.status.default_input_method.back() == '\n' ||
          attempt.status.default_input_method.back() == '\r')) {
    attempt.status.default_input_method.pop_back();
  }
  attempt.status.is_default_ime =
      AndroidComponentsEquivalent(attempt.status.default_input_method, ime_id);

  if (!settings_component.empty()) {
    const auto launch_result =
        runner(prefix + " shell am start -W -n " + QuoteForShell(settings_component));
    attempt.status.settings_launch_ok =
        launch_result.exit_code == 0 &&
        LaunchOutputLooksSuccessful(launch_result.output);
  }

  attempt.ok = true;
  return attempt;
}

}  // namespace

bool OutputContainsInstalledPackage(const std::string& output,
                                    const std::string& package_name) {
  const std::string expected = "package:" + package_name;
  for (const auto& line : SplitLines(output)) {
    if (line == expected) {
      return true;
    }
  }
  return false;
}

bool OutputContainsImeId(const std::string& output, const std::string& ime_id) {
  for (const auto& line : SplitLines(output)) {
    std::string candidate = line;
    if (!candidate.empty() && candidate.back() == ':') {
      candidate.pop_back();
    }
    if (AndroidComponentsEquivalent(candidate, ime_id)) {
      return true;
    }
  }
  return false;
}

bool EnabledInputMethodsContainIme(const std::string& output,
                                   const std::string& ime_id) {
  std::istringstream input(output);
  std::string segment;
  while (std::getline(input, segment, ':')) {
    if (!segment.empty() && segment.back() == '\r') {
      segment.pop_back();
    }
    if (!segment.empty() && segment.back() == '\n') {
      segment.pop_back();
    }
    if (AndroidComponentsEquivalent(segment, ime_id)) {
      return true;
    }
  }
  return false;
}

bool InstallOutputLooksSuccessful(const std::string& output) {
  return output.find("Success") != std::string::npos;
}

bool LaunchOutputLooksSuccessful(const std::string& output) {
  return output.find("Status: ok") != std::string::npos &&
         output.find("Complete") != std::string::npos;
}

bool LaunchOutputMentionsComponent(const std::string& output,
                                   const std::string& component) {
  if (output.find("cmp=" + component) != std::string::npos ||
      output.find(component) != std::string::npos) {
    return true;
  }

  const std::string canonical = CanonicalizeAndroidComponent(component);
  if (canonical != component &&
      (output.find("cmp=" + canonical) != std::string::npos ||
       output.find(canonical) != std::string::npos)) {
    return true;
  }

  const auto slash = canonical.find('/');
  if (slash == std::string::npos) {
    return false;
  }

  const std::string package_name = canonical.substr(0, slash);
  const std::string class_name = canonical.substr(slash + 1);
  if (class_name.rfind(package_name + ".", 0) == 0) {
    const std::string short_form =
        package_name + "/." + class_name.substr(package_name.size() + 1);
    return output.find("cmp=" + short_form) != std::string::npos ||
           output.find(short_form) != std::string::npos;
  }

  return false;
}

bool LaunchOutputConfirmsComponent(const std::string& output,
                                   const std::string& component) {
  return LaunchOutputLooksSuccessful(output) &&
         LaunchOutputMentionsComponent(output, component);
}

std::string RenderAdbActivityLaunchReport(
    const AdbActivityLaunchReport& report) {
  std::ostringstream output;
  output << "ADB Serial: " << report.serial << '\n';
  output << "Component: " << report.component << '\n';
  output << "Launch OK: " << (report.launch_ok ? "yes" : "no") << '\n';
  output << "Launch Output:\n" << report.output;
  return output.str();
}

std::string RenderInstalledAppLaunchReport(
    const InstalledAppLaunchReport& report) {
  std::ostringstream output;
  output << "Runtime Backend: " << report.backend_name << '\n';
  output << "Package: " << report.package_name << '\n';
  output << "ADB Serial: " << report.serial << '\n';
  output << "Component: " << report.component << '\n';
  output << "Launch OK: " << (report.launch_ok ? "yes" : "no") << '\n';
  output << "Launch Output:\n" << report.output;
  return output.str();
}

std::string RenderWaydroidAppLaunchReport(
    const WaydroidAppLaunchReport& report) {
  std::ostringstream output;
  output << "Runtime Backend: waydroid\n";
  output << "Package: " << report.package_name << '\n';
  output << "Launch OK: " << (report.launch_ok ? "yes" : "no") << '\n';
  output << "Launch Output:\n" << report.output;
  return output.str();
}

AdbActivityLaunchReport LaunchAdbActivityWithRunner(
    const std::string& serial, const std::string& component,
    const CommandRunner& runner) {
  const std::string prefix = BuildAdbPrefix(serial);
  const auto result =
      runner(prefix + " shell am start -W -n " + QuoteForShell(component));

  AdbActivityLaunchReport report;
  report.serial = serial;
  report.component = component;
  report.output = result.output;
  report.launch_ok =
      result.exit_code == 0 &&
      LaunchOutputConfirmsComponent(result.output, component);
  return report;
}

AdbActivityLaunchReport LaunchAdbActivity(const std::string& serial,
                                          const std::string& component) {
  return LaunchAdbActivityWithRunner(serial, component, MakeShellRunner());
}

InstalledAppLaunchReport LaunchInstalledAppWithRunner(
    const InstalledAppLaunchSpec& spec, const CommandRunner& runner) {
  if (!IsValidPackageName(spec.package_name)) {
    throw std::invalid_argument("package_name must look like a Java package");
  }

  InstalledAppLaunchReport report;
  report.backend_name = RenderRuntimeBackendName(spec.backend);
  report.serial = spec.serial;
  report.package_name = spec.package_name;
  report.component = spec.component;

  switch (spec.backend) {
    case RuntimeBackendKind::kWaydroid: {
      const auto result = runner("waydroid app launch " + spec.package_name);
      report.launch_ok = result.exit_code == 0;
      report.output = result.output;
      return report;
    }

    case RuntimeBackendKind::kAttachedAdb: {
      if (spec.serial.empty()) {
        throw std::invalid_argument(
            "attached-adb backend requires a target serial");
      }
      if (spec.component.empty()) {
        throw std::invalid_argument(
            "attached-adb backend requires an explicit launcher component");
      }

      const auto activity_report =
          LaunchAdbActivityWithRunner(spec.serial, spec.component, runner);
      report.launch_ok = activity_report.launch_ok;
      report.output = activity_report.output;
      return report;
    }

    case RuntimeBackendKind::kNative:
      report.output = "native backend is not implemented yet\n";
      report.launch_ok = false;
      return report;
  }

  throw std::invalid_argument("unsupported runtime backend enum");
}

InstalledAppLaunchReport LaunchInstalledApp(const InstalledAppLaunchSpec& spec) {
  return LaunchInstalledAppWithRunner(spec, MakeShellRunner());
}

WaydroidAppLaunchReport LaunchWaydroidAppWithRunner(
    const std::string& package_name, const CommandRunner& runner) {
  const auto generic = LaunchInstalledAppWithRunner(
      {.backend = RuntimeBackendKind::kWaydroid, .package_name = package_name},
      runner);
  WaydroidAppLaunchReport report;
  report.package_name = generic.package_name;
  report.launch_ok = generic.launch_ok;
  report.output = generic.output;
  return report;
}

WaydroidAppLaunchReport LaunchWaydroidApp(const std::string& package_name) {
  return LaunchWaydroidAppWithRunner(package_name, MakeShellRunner());
}

std::string RenderAdbImeStatusReport(const AdbImeStatus& status) {
  std::ostringstream output;
  output << "ADB Serial: " << status.serial << '\n';
  output << "Package: " << status.package_name << '\n';
  output << "IME ID: " << status.ime_id << '\n';
  output << "Settings Component: " << status.settings_component << '\n';
  output << "Package installed: " << (status.package_installed ? "yes" : "no")
         << '\n';
  output << "IME registered: " << (status.ime_registered ? "yes" : "no")
         << '\n';
  output << "IME enabled: " << (status.ime_enabled ? "yes" : "no") << '\n';
  output << "Enabled IMEs: " << status.enabled_input_methods << '\n';
  output << "Default IME: " << status.default_input_method << '\n';
  output << "Default matches target: "
         << (status.is_default_ime ? "yes" : "no") << '\n';
  output << "Settings launch OK: ";
  if (status.settings_component.empty()) {
    output << "not checked\n";
  } else {
    output << (status.settings_launch_ok ? "yes" : "no") << '\n';
  }
  return output.str();
}

std::string RenderAdbProvisioningReport(const AdbProvisioningReport& report) {
  std::ostringstream output;
  output << "Provisioning Loading: "
         << RenderLoadingBar(CalculateProvisioningProgress(report), 10) << '\n';
  output << "ADB Serial: " << report.serial << '\n';
  output << "APK Path: " << report.apk_path << '\n';
  output << "Package: " << report.package_name << '\n';
  output << "APK Declared Package: " << report.apk_declared_package_name << '\n';
  output << "APK package match: "
         << (report.apk_matches_requested_package ? "yes" : "no") << '\n';
  output << "IME ID: " << report.ime_id << '\n';
  output << "Settings Component: " << report.settings_component << '\n';
  output << "Install OK: " << (report.install_ok ? "yes" : "no") << '\n';
  output << "Enable OK: " << (report.enable_ok ? "yes" : "no") << '\n';
  output << "Set Default OK: " << (report.set_ok ? "yes" : "no") << '\n';
  output << "Readback OK: " << (report.readback_ok ? "yes" : "no") << '\n';
  if (!report.readback_ok) {
    output << "Readback Error: " << report.readback_error << '\n';
  }
  output << "Package installed: "
         << (report.final_status.package_installed ? "yes" : "no") << '\n';
  output << "IME registered: "
         << (report.final_status.ime_registered ? "yes" : "no") << '\n';
  output << "IME enabled: "
         << (report.final_status.ime_enabled ? "yes" : "no") << '\n';
  output << "Enabled IMEs: " << report.final_status.enabled_input_methods << '\n';
  output << "Default IME: " << report.final_status.default_input_method << '\n';
  output << "Default matches target: "
         << (report.final_status.is_default_ime ? "yes" : "no") << '\n';
  output << "Settings launch OK: ";
  if (report.settings_component.empty()) {
    output << "not checked\n";
  } else {
    output << (report.final_status.settings_launch_ok ? "yes" : "no") << '\n';
  }
  output << "Ready for typing: " << (report.ready_for_typing ? "yes" : "no")
         << '\n';
  return output.str();
}

AdbImeStatus QueryAdbImeStatusWithRunner(const std::string& serial,
                                         const std::string& package_name,
                                         const std::string& ime_id,
                                         const std::string& settings_component,
                                         const CommandRunner& runner) {
  const auto attempt = TryQueryAdbImeStatusWithRunner(
      serial, package_name, ime_id, settings_component, runner);
  if (!attempt.ok) {
    throw std::runtime_error(attempt.error);
  }
  return attempt.status;
}

AdbImeStatus QueryAdbImeStatus(const std::string& serial,
                               const std::string& package_name,
                               const std::string& ime_id,
                               const std::string& settings_component) {
  return QueryAdbImeStatusWithRunner(serial, package_name, ime_id,
                                     settings_component, MakeShellRunner());
}

AdbProvisioningReport ProvisionAdbImeWithRunner(
    const std::string& serial, const std::string& apk_path,
    const std::string& package_name, const std::string& ime_id,
    const std::string& settings_component, const CommandRunner& runner,
    const std::string& apk_declared_package_name) {
  const std::string prefix = BuildAdbPrefix(serial);
  const std::string shell_ime_id = ShortenAndroidComponent(ime_id);

  AdbProvisioningReport report;
  report.serial = serial;
  report.apk_path = apk_path;
  report.package_name = package_name;
  report.ime_id = ime_id;
  report.settings_component = settings_component;
  report.apk_declared_package_name =
      apk_declared_package_name.empty() ? package_name : apk_declared_package_name;
  report.apk_matches_requested_package =
      report.apk_declared_package_name == package_name;
  if (!report.apk_matches_requested_package) {
    report.readback_ok = false;
    report.readback_error =
        "package mismatch between requested package and APK declared package";
    return report;
  }

  const auto install_result =
      runner(prefix + " install -r " + QuoteForShell(apk_path));
  report.install_output = install_result.output;
  report.install_ok =
      install_result.exit_code == 0 &&
      InstallOutputLooksSuccessful(install_result.output);

  const auto enable_result =
      runner(prefix + " shell ime enable " + QuoteForShell(shell_ime_id));
  report.enable_output = enable_result.output;
  report.enable_ok = enable_result.exit_code == 0;

  const auto set_result =
      runner(prefix + " shell ime set " + QuoteForShell(shell_ime_id));
  report.set_output = set_result.output;
  report.set_ok = set_result.exit_code == 0;

  const auto readback = TryQueryAdbImeStatusWithRunner(
      serial, package_name, ime_id, settings_component, runner);
  report.readback_ok = readback.ok;
  report.readback_error = readback.error;
  report.final_status = readback.status;
  report.ready_for_typing =
      report.apk_matches_requested_package && report.install_ok &&
      report.enable_ok && report.set_ok && report.readback_ok &&
      report.final_status.package_installed && report.final_status.ime_registered &&
      report.final_status.ime_enabled && report.final_status.is_default_ime &&
      (settings_component.empty() || report.final_status.settings_launch_ok);

  return report;
}

AdbProvisioningReport ProvisionAdbIme(const std::string& serial,
                                      const std::string& apk_path,
                                      const std::string& package_name,
                                      const std::string& ime_id,
                                      const std::string& settings_component) {
  const auto apk_declared_package_name = InspectApkPackageName(apk_path);
  return ProvisionAdbImeWithRunner(serial, apk_path, package_name, ime_id,
                                   settings_component, MakeShellRunner(),
                                   apk_declared_package_name);
}

}  // namespace wfa
