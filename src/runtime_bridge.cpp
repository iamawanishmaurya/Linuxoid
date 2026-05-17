#include "wfa/runtime_bridge.hpp"

#include "wfa/art_activity_bootstrap_fixture.hpp"
#include "wfa/art_class_resolution_fixture.hpp"
#include "wfa/art_bootstrap_execution_fixture.hpp"
#include "wfa/art_runtime_smoke.hpp"
#include "wfa/apk_loader.hpp"
#include "wfa/checkpoint.hpp"
#include "wfa/native_spike.hpp"
#include "wfa/runtime_health.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
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

constexpr int kAdbDiscoveryTimeoutSeconds = 5;
constexpr const char* kNativeRuntimeSerial = "linuxoid-native";

struct StatusQueryAttempt {
  bool ok = false;
  std::string error;
  AdbImeStatus status;
};

struct LauncherResolutionAttempt {
  bool launcher_resolved = false;
  bool timed_out = false;
  std::string resolved_component;
  std::string output;
};

struct NativePreflightDiagnostics {
  bool runtime_probe_ready = false;
  bool bootstrap_planned = false;
  bool dependency_blocked = false;
  std::string art_runtime_probe_source;
  std::string bootstrap_manifest_path;
  std::string runtime_health_json_path;
  std::string runtime_diagnostic_replay_json_path;
  int failing_subsystem_count = 0;
  std::vector<std::string> failing_subsystems;
  std::string notes;
};

struct NativePackageLookup;
NativePackageLookup ResolveNativePackageLookup(const std::string& package_name);
LoadedApkReport BuildLoadedApkReportFromNativeLookup(
    const NativePackageLookup& lookup);
NativePreflightDiagnostics BuildNativePreflightDiagnostics(
    const std::string& package_name);

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

std::string WrapWithTimeout(const std::string& command, int seconds) {
  return "timeout " + std::to_string(seconds) + "s " + command;
}

bool EnvFlagEnabled(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  std::string normalized(value);
  const std::size_t first = normalized.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return false;
  }
  const std::size_t last = normalized.find_last_not_of(" \t\r\n");
  normalized = normalized.substr(first, last - first + 1);
  return normalized == "1" || normalized == "true" ||
         normalized == "TRUE" || normalized == "yes" ||
         normalized == "YES";
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to read file: " + path.string());
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string ResolveNativeCompatRoot() {
  if (const char* override_root = std::getenv("LINUXOID_NATIVE_COMPAT_ROOT");
      override_root != nullptr && override_root[0] != '\0') {
    return override_root;
  }
  return "/var/lib/wfa";
}

std::string ResolveNativeSpikeRoot() {
  if (const char* override_root = std::getenv("LINUXOID_NATIVE_SPIKE_ROOT");
      override_root != nullptr && override_root[0] != '\0') {
    return override_root;
  }
  return "/tmp/linuxoid-native-spike";
}

std::string ResolveCompatctlPathForNativeRuntime() {
  if (const char* override_path = std::getenv("LINUXOID_COMPATCTL_PATH");
      override_path != nullptr && override_path[0] != '\0') {
    return override_path;
  }
  return "compatctl";
}

std::string DetectHostAbi() {
#if defined(__x86_64__)
  return "x86_64";
#elif defined(__aarch64__)
  return "arm64-v8a";
#elif defined(__arm__)
  return "armeabi-v7a";
#else
  return "unknown";
#endif
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

std::string TrimWhitespace(std::string value) {
  while (!value.empty() &&
         (value.front() == ' ' || value.front() == '\t' ||
          value.front() == '\n' || value.front() == '\r')) {
    value.erase(value.begin());
  }
  while (!value.empty() &&
         (value.back() == ' ' || value.back() == '\t' ||
          value.back() == '\n' || value.back() == '\r')) {
    value.pop_back();
  }
  return value;
}

std::string JoinStrings(const std::vector<std::string>& values,
                        const std::string& delimiter) {
  std::ostringstream output;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      output << delimiter;
    }
    output << values[index];
  }
  return output.str();
}

std::string ExtractJsonStringOrEmpty(const std::string& json,
                                     const std::string& key) {
  const std::regex pattern("\"" + key + R"(\"\s*:\s*\"([^\"]*)\")");
  std::smatch match;
  if (std::regex_search(json, match, pattern) && match.size() == 2) {
    return match[1].str();
  }
  return "";
}

int ExtractJsonIntOrDefault(const std::string& json, const std::string& key,
                            int fallback) {
  const std::regex pattern("\"" + key + R"(\"\s*:\s*(-?\d+))");
  std::smatch match;
  if (std::regex_search(json, match, pattern) && match.size() == 2) {
    return std::stoi(match[1].str());
  }
  return fallback;
}

struct NativePackageLookup {
  std::string compat_root;
  std::string package_name;
  std::string install_id;
  std::string install_root;
  std::string manifest_path;
  std::string apk_path;
  std::string launcher_component;
  std::string version_code;
  std::string version_name;
  int min_sdk = 0;
  int target_sdk = 0;
  bool package_visible = false;
  bool launcher_resolved = false;
  std::string notes;
  std::string package_check_output;
  std::string launcher_query_output;
  std::string path_query_output;
  std::string dump_output;
};

NativePackageLookup ResolveNativePackageLookup(const std::string& package_name) {
  namespace fs = std::filesystem;

  NativePackageLookup lookup;
  lookup.compat_root = ResolveNativeCompatRoot();
  lookup.package_name = package_name;

  const fs::path package_root =
      fs::path(lookup.compat_root) / "users/0/packages" / package_name;
  std::ostringstream package_check_output;
  package_check_output << "native compat root: " << lookup.compat_root << "\n";
  package_check_output << "package root: " << package_root.string() << "\n";

  if (!fs::exists(package_root) || !fs::is_directory(package_root)) {
    package_check_output << "package staged: no\n";
    lookup.package_check_output = package_check_output.str();
    lookup.notes = "package is not staged in the native compat root";
    return lookup;
  }

  std::vector<std::string> install_ids;
  for (const auto& entry : fs::directory_iterator(package_root)) {
    if (!entry.is_directory()) {
      continue;
    }
    install_ids.push_back(entry.path().filename().string());
  }
  std::sort(install_ids.begin(), install_ids.end());
  package_check_output << "package staged: yes\n";
  package_check_output << "install count: " << install_ids.size() << "\n";

  if (install_ids.empty()) {
    lookup.package_check_output = package_check_output.str();
    lookup.notes = "package directory exists but no install roots were found";
    return lookup;
  }

  lookup.package_visible = true;
  lookup.install_id = install_ids.back();
  lookup.install_root = (package_root / lookup.install_id).string();
  lookup.manifest_path =
      (fs::path(lookup.install_root) / "manifest.json").string();
  lookup.apk_path = (fs::path(lookup.install_root) / "base.apk").string();
  package_check_output << "selected install id: " << lookup.install_id << "\n";
  package_check_output << "selected install root: " << lookup.install_root
                       << "\n";

  if (fs::exists(lookup.manifest_path)) {
    const std::string manifest_json = ReadTextFile(lookup.manifest_path);
    lookup.version_code = ExtractJsonStringOrEmpty(manifest_json, "version_code");
    if (lookup.version_code.empty()) {
      const std::regex int_pattern(R"("version_code"\s*:\s*([0-9]+))");
      std::smatch match;
      if (std::regex_search(manifest_json, match, int_pattern) &&
          match.size() == 2) {
        lookup.version_code = match[1].str();
      }
    }
    lookup.version_name =
        ExtractJsonStringOrEmpty(manifest_json, "version_name");
    lookup.min_sdk = ExtractJsonIntOrDefault(manifest_json, "min_sdk", 0);
    lookup.target_sdk = ExtractJsonIntOrDefault(manifest_json, "target_sdk", 0);
    lookup.launcher_component =
        ExtractJsonStringOrEmpty(manifest_json, "launcher_component");
    lookup.launcher_resolved = !lookup.launcher_component.empty();
    lookup.dump_output = manifest_json;
  }

  lookup.path_query_output = lookup.apk_path + "\n";
  lookup.launcher_query_output =
      lookup.launcher_component.empty()
          ? "launcher component not present in staged manifest metadata\n"
          : lookup.launcher_component + "\n";
  lookup.package_check_output = package_check_output.str();
  lookup.notes = lookup.launcher_resolved
                     ? "native package metadata resolved from staged compat root"
                     : "package is staged locally but no launcher component was resolved";
  return lookup;
}

LoadedApkReport BuildLoadedApkReportFromNativeLookup(
    const NativePackageLookup& lookup) {
  namespace fs = std::filesystem;

  if (!lookup.package_visible) {
    throw std::invalid_argument(
        "native package lookup must be visible before building launch report");
  }

  const int version_code = lookup.version_code.empty()
                               ? 1
                               : std::max(1, std::stoi(lookup.version_code));
  const auto layout = BuildPackageLayout(
      {.package_name = lookup.package_name,
       .install_id = lookup.install_id,
       .version_code = version_code},
      lookup.compat_root);

  ManifestProfile profile{
      .package_name = lookup.package_name,
      .launcher_activity_name = lookup.launcher_component,
      .declared_components = lookup.launcher_component.empty()
                                 ? std::vector<std::string>{}
                                 : std::vector<std::string>{lookup.launcher_component},
      .declared_activity_components =
          lookup.launcher_component.empty()
              ? std::vector<std::string>{}
              : std::vector<std::string>{lookup.launcher_component},
      .has_launcher_activity = lookup.launcher_resolved,
  };

  const fs::path manifest_xml_path =
      fs::path(lookup.install_root) / "AndroidManifest.xml";
  if (fs::exists(manifest_xml_path)) {
    profile = ParseDecodedManifest(ReadTextFile(manifest_xml_path));
  }
  if (profile.package_name.empty()) {
    profile.package_name = lookup.package_name;
  }
  if (!lookup.launcher_component.empty() &&
      (!profile.has_launcher_activity ||
       profile.launcher_activity_name.empty())) {
    profile.launcher_activity_name = lookup.launcher_component;
    profile.has_launcher_activity = true;
    if (std::find(profile.declared_components.begin(),
                  profile.declared_components.end(),
                  lookup.launcher_component) ==
        profile.declared_components.end()) {
      profile.declared_components.push_back(lookup.launcher_component);
    }
    if (std::find(profile.declared_activity_components.begin(),
                  profile.declared_activity_components.end(),
                  lookup.launcher_component) ==
        profile.declared_activity_components.end()) {
      profile.declared_activity_components.push_back(lookup.launcher_component);
    }
  }
  const auto assessment = AssessRuntimeRequirements(profile);

  return LoadedApkReport{
      .apk_path = lookup.apk_path,
      .install_id = lookup.install_id,
      .metadata =
          {.apk_file_name = fs::path(lookup.apk_path).filename().string(),
           .min_sdk = lookup.min_sdk,
           .target_sdk = lookup.target_sdk,
           .version_code = version_code,
           .version_name = lookup.version_name},
      .manifest_profile = profile,
      .assessment = assessment,
      .layout = layout,
      .install_root = lookup.install_root,
  };
}

NativePreflightDiagnostics BuildNativePreflightDiagnostics(
    const std::string& package_name) {
  const auto staged_report = BuildLoadedApkReportFromNativeLookup(
      ResolveNativePackageLookup(package_name));
  const auto plan = BuildNativeLaunchPlan(staged_report, ResolveNativeSpikeRoot());
  const auto bootstrap = BuildNativeActivityBootstrap(
      plan, ResolveCompatctlPathForNativeRuntime());
  const auto resolution =
      RunNativeArtClassResolutionFixture(bootstrap.bootstrap_manifest_path);
  const auto runtime_smoke = BuildNativeArtRuntimeSmokeFixture(resolution);
  const auto activity_bootstrap =
      BuildNativeArtActivityBootstrapFixture(runtime_smoke);
  const auto health = RunRuntimeHealthFixture(
      bootstrap.bootstrap_manifest_path, "baseline");
  const auto diagnostic =
      ReplayRuntimeDiagnosticBundle(bootstrap.bootstrap_manifest_path);

  NativePreflightDiagnostics diagnostics;
  diagnostics.art_runtime_probe_source = runtime_smoke.art_runtime_probe_source;
  diagnostics.bootstrap_manifest_path = bootstrap.bootstrap_manifest_path;
  diagnostics.runtime_health_json_path = health.health_json_path;
  diagnostics.runtime_diagnostic_replay_json_path = diagnostic.result_json_path;
  diagnostics.dependency_blocked = health.dependency_blocked;
  diagnostics.failing_subsystem_count = health.failing_subsystem_count;
  diagnostics.failing_subsystems = health.failing_subsystems;
  diagnostics.bootstrap_planned = activity_bootstrap.runtime_bootstrap_planned;

  const bool override_allowed =
      EnvFlagEnabled("LINUXOID_NATIVE_ALLOW_RUNTIME_OVERRIDE");
  const bool fixture_only_probe =
      runtime_smoke.art_runtime_probe_source == "override" && !override_allowed;
  diagnostics.runtime_probe_ready =
      runtime_smoke.runtime_class_resolution_succeeded && !fixture_only_probe;

  if (runtime_smoke.art_runtime_probe_source == "missing") {
    diagnostics.notes =
        "host ART runtime is not detected for the staged native launch path";
  } else if (fixture_only_probe) {
    diagnostics.notes =
        "runtime probe resolved through the Linuxoid override seam; set "
        "LINUXOID_NATIVE_ALLOW_RUNTIME_OVERRIDE=1 to treat it as launch-ready";
  } else if (!runtime_smoke.runtime_class_resolution_succeeded) {
    diagnostics.notes =
        "runtime class resolution is not ready for the staged native launch path";
  } else if (!activity_bootstrap.runtime_bootstrap_planned) {
    diagnostics.notes =
        "activity bootstrap planning is not ready for the staged native launch path";
  } else {
    diagnostics.notes =
        "runtime target looks ready for the staged native bootstrap attempt";
  }

  return diagnostics;
}

RuntimeTarget ParseAdbDeviceLine(const std::string& line) {
  RuntimeTarget target;
  std::istringstream input(line);
  input >> target.serial >> target.state;
  target.online = target.state == "device";
  return target;
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

std::string ParseResolvedActivityComponent(const std::string& output,
                                           const std::string& package_name) {
  std::string resolved_component;
  for (const auto& line : SplitLines(output)) {
    const std::string trimmed = TrimWhitespace(line);
    if (trimmed.find('/') == std::string::npos) {
      continue;
    }
    if (trimmed.rfind(package_name, 0) != 0) {
      continue;
    }
    resolved_component = trimmed;
  }

  if (resolved_component.empty()) {
    return "";
  }

  return ShortenAndroidComponent(resolved_component);
}

std::string ParseInstalledPackagePath(const std::string& output) {
  for (const auto& line : SplitLines(output)) {
    const std::string trimmed = TrimWhitespace(line);
    if (trimmed.rfind("package:", 0) == 0) {
      return trimmed.substr(std::string("package:").size());
    }
  }
  return "";
}

std::string ParsePackageDumpField(const std::string& output,
                                  const std::string& field_name) {
  const std::string prefix = field_name + "=";
  for (const auto& line : SplitLines(output)) {
    const std::string trimmed = TrimWhitespace(line);
    if (trimmed.rfind(prefix, 0) != 0) {
      continue;
    }

    std::string value = trimmed.substr(prefix.size());
    const auto separator = value.find_first_of(" \t");
    if (separator != std::string::npos) {
      value = value.substr(0, separator);
    }
    return value;
  }
  return "";
}

LauncherResolutionAttempt ResolveAttachedAdbLauncherWithRunner(
    const std::string& serial, const std::string& package_name,
    const CommandRunner& runner) {
  LauncherResolutionAttempt attempt;
  const auto result = runner(
      WrapWithTimeout(BuildAdbPrefix(serial) +
                          " shell cmd package resolve-activity --brief " +
                          QuoteForShell(package_name),
                      kAdbDiscoveryTimeoutSeconds));
  attempt.output = result.output;
  attempt.timed_out = result.exit_code == 124;
  if (result.exit_code != 0) {
    return attempt;
  }

  attempt.resolved_component =
      ParseResolvedActivityComponent(result.output, package_name);
  attempt.launcher_resolved = !attempt.resolved_component.empty();
  return attempt;
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

std::string ReadAdbPropWithRunner(const std::string& serial,
                                  const std::string& prop_name,
                                  const CommandRunner& runner) {
  const auto result =
      runner(WrapWithTimeout(BuildAdbPrefix(serial) + " shell getprop " +
                                 QuoteForShell(prop_name),
                             kAdbDiscoveryTimeoutSeconds));
  if (result.exit_code != 0) {
    return "";
  }
  return TrimWhitespace(result.output);
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
  output << "Runtime Target: " << report.serial << '\n';
  output << "Component: " << report.component << '\n';
  if (!report.launch_classification.empty()) {
    output << "Launch Classification: " << report.launch_classification
           << '\n';
  }
  if (!report.art_runtime_probe_source.empty()) {
    output << "ART Runtime Probe Source: " << report.art_runtime_probe_source
           << '\n';
  }
  if (!report.bootstrap_manifest_path.empty()) {
    output << "Bootstrap Manifest Path: " << report.bootstrap_manifest_path
           << '\n';
  }
  if (!report.bootstrap_execution_result_path.empty()) {
    output << "Bootstrap Execution Result Path: "
           << report.bootstrap_execution_result_path << '\n';
  }
  if (!report.bootstrap_execution_trace_jsonl_path.empty()) {
    output << "Bootstrap Execution Trace Path: "
           << report.bootstrap_execution_trace_jsonl_path << '\n';
  }
  if (!report.bootstrap_execution_runner_state_json_path.empty()) {
    output << "Bootstrap Runner State Path: "
           << report.bootstrap_execution_runner_state_json_path << '\n';
  }
  if (!report.runtime_health_json_path.empty()) {
    output << "Runtime Health JSON Path: " << report.runtime_health_json_path
           << '\n';
    output << "Runtime Health Ready: "
           << (report.runtime_health_ready ? "yes" : "no") << '\n';
    output << "Runtime Dependency Blocked: "
           << (report.runtime_dependency_blocked ? "yes" : "no") << '\n';
    output << "Runtime Failing Subsystem Count: "
           << report.runtime_failing_subsystem_count << '\n';
    output << "Runtime Failing Subsystems: "
           << (report.runtime_failing_subsystems.empty()
                   ? "none"
                   : JoinStrings(report.runtime_failing_subsystems, ", "))
           << '\n';
  }
  if (!report.runtime_recovery_plan_path.empty()) {
    output << "Runtime Recovery Plan Path: "
           << report.runtime_recovery_plan_path << '\n';
  }
  if (!report.runtime_diagnostic_replay_json_path.empty()) {
    output << "Runtime Diagnostic Replay Path: "
           << report.runtime_diagnostic_replay_json_path << '\n';
    output << "Runtime Diagnostic Replay Ready: "
           << (report.runtime_diagnostic_replay_ready ? "yes" : "no") << '\n';
    output << "Runtime Trace Bundle Complete: "
           << (report.runtime_trace_bundle_complete ? "yes" : "no") << '\n';
  }
  if (!report.runtime_diagnostic_trace_index_path.empty()) {
    output << "Runtime Diagnostic Trace Index Path: "
           << report.runtime_diagnostic_trace_index_path << '\n';
  }
  output << "Launch OK: " << (report.launch_ok ? "yes" : "no") << '\n';
  output << "Launch Output:\n" << report.output;
  return output.str();
}

std::string RenderInstalledPackageMetadataReport(
    const InstalledPackageMetadataReport& report) {
  std::ostringstream output;
  output << "Runtime Backend: " << report.backend_name << '\n';
  output << "Runtime Target: " << report.serial << '\n';
  output << "Package: " << report.package_name << '\n';
  output << "Package Visible: " << (report.package_visible ? "yes" : "no")
         << '\n';
  output << "Launcher Resolved: " << (report.launcher_resolved ? "yes" : "no")
         << '\n';
  output << "Resolved Component: " << report.resolved_component << '\n';
  output << "Install Path: " << report.install_path << '\n';
  output << "Version Code: " << report.version_code << '\n';
  output << "Version Name: " << report.version_name << '\n';
  output << "Notes: " << report.notes << '\n';
  output << "Package Check Output:\n" << report.package_check_output;
  output << "Launcher Query Output:\n" << report.launcher_query_output;
  output << "Path Query Output:\n" << report.path_query_output;
  output << "Dump Output Summary: "
         << (report.dump_output.empty() ? "not available" : "parsed into summary fields above")
         << '\n';
  return output.str();
}

std::string RenderRuntimeDiscoveryReport(
    const RuntimeDiscoveryReport& report) {
  std::ostringstream output;
  output << "Runtime Backend: " << report.backend_name << '\n';
  output << "Backend Available: "
         << (report.backend_available ? "yes" : "no") << '\n';
  output << "Targets Found: " << report.targets.size() << '\n';
  for (const auto& target : report.targets) {
    output << "  - Serial: " << target.serial << '\n';
    output << "    State: " << target.state << '\n';
    output << "    Online: " << (target.online ? "yes" : "no") << '\n';
    output << "    Model: " << target.model << '\n';
    output << "    Android: " << target.android_release << '\n';
    output << "    ABI: " << target.abi << '\n';
  }
  output << "Backend Check Output:\n" << report.backend_check_output;
  return output.str();
}

std::string RenderRuntimePreflightReport(
    const RuntimePreflightReport& report) {
  std::ostringstream output;
  output << "Runtime Backend: " << report.backend_name << '\n';
  output << "Runtime Target: " << report.serial << '\n';
  output << "Package: " << report.package_name << '\n';
  output << "Component: " << report.component << '\n';
  output << "Backend Available: "
         << (report.backend_available ? "yes" : "no") << '\n';
  output << "Target Discovered: "
         << (report.target_discovered ? "yes" : "no") << '\n';
  output << "Target Selected: "
         << (report.target_selected ? "yes" : "no") << '\n';
  output << "Target Online: " << (report.target_online ? "yes" : "no")
         << '\n';
  output << "Package Visible: " << (report.package_visible ? "yes" : "no")
         << '\n';
  output << "Component Ready: " << (report.component_ready ? "yes" : "no")
         << '\n';
  if (report.backend_name == "native" || !report.art_runtime_probe_source.empty() ||
      !report.bootstrap_manifest_path.empty() ||
      !report.runtime_health_json_path.empty()) {
    if (!report.art_runtime_probe_source.empty()) {
      output << "ART Runtime Probe Source: " << report.art_runtime_probe_source
             << '\n';
    }
    output << "Runtime Probe Ready: "
           << (report.runtime_probe_ready ? "yes" : "no") << '\n';
    output << "Bootstrap Planned: "
           << (report.bootstrap_planned ? "yes" : "no") << '\n';
    output << "Dependency Blocked: "
           << (report.dependency_blocked ? "yes" : "no") << '\n';
    output << "Failing Subsystem Count: " << report.failing_subsystem_count
           << '\n';
    output << "Failing Subsystems: "
           << (report.failing_subsystems.empty()
                   ? "none"
                   : JoinStrings(report.failing_subsystems, ", "))
           << '\n';
  }
  output << "Ready For Launch: " << (report.ready_for_launch ? "yes" : "no")
         << '\n';
  if (report.backend_name == "native" || !report.bootstrap_manifest_path.empty() ||
      !report.runtime_health_json_path.empty() ||
      !report.runtime_diagnostic_replay_json_path.empty()) {
    if (!report.bootstrap_manifest_path.empty()) {
      output << "Bootstrap Manifest Path: " << report.bootstrap_manifest_path
             << '\n';
    }
    if (!report.runtime_health_json_path.empty()) {
      output << "Runtime Health JSON Path: " << report.runtime_health_json_path
             << '\n';
    }
    if (!report.runtime_diagnostic_replay_json_path.empty()) {
      output << "Runtime Diagnostic Replay Path: "
             << report.runtime_diagnostic_replay_json_path << '\n';
    }
  }
  output << "Model: " << report.model << '\n';
  output << "Android: " << report.android_release << '\n';
  output << "ABI: " << report.abi << '\n';
  output << "Notes: " << report.notes << '\n';
  output << "Backend Check Output:\n" << report.backend_check_output;
  output << "Discovery Output:\n" << report.discovery_output;
  output << "Package Check Output:\n" << report.package_check_output;
  return output.str();
}

RuntimeDiscoveryReport DiscoverRuntimeTargetsWithRunner(
    RuntimeBackendKind backend, const CommandRunner& runner) {
  RuntimeDiscoveryReport report;
  report.backend_name = RenderRuntimeBackendName(backend);

  switch (backend) {
    case RuntimeBackendKind::kAttachedAdb: {
      const auto result =
          runner(WrapWithTimeout("adb devices", kAdbDiscoveryTimeoutSeconds));
      report.backend_check_output = result.output;
      report.backend_available = result.exit_code == 0;
      if (!report.backend_available) {
        if (result.exit_code == 124) {
          report.backend_check_output +=
              "\nTimed out while querying attached ADB targets.\n";
        }
        return report;
      }

      for (const auto& line : SplitLines(result.output)) {
        const std::string trimmed = TrimWhitespace(line);
        if (trimmed.empty() || trimmed == "List of devices attached") {
          continue;
        }

        RuntimeTarget target = ParseAdbDeviceLine(trimmed);
        if (target.serial.empty()) {
          continue;
        }
        target.backend_name = report.backend_name;
        if (target.online) {
          target.model =
              ReadAdbPropWithRunner(target.serial, "ro.product.model", runner);
          target.android_release = ReadAdbPropWithRunner(
              target.serial, "ro.build.version.release", runner);
          target.abi =
              ReadAdbPropWithRunner(target.serial, "ro.product.cpu.abi", runner);
        }
        report.targets.push_back(target);
      }
      return report;
    }

    case RuntimeBackendKind::kWaydroid: {
      const auto result = runner("waydroid status");
      report.backend_check_output = result.output;
      report.backend_available = result.exit_code == 0;
      if (report.backend_available) {
        RuntimeTarget target;
        target.backend_name = report.backend_name;
        target.serial = "waydroid";
        target.state = result.output.find("RUNNING") != std::string::npos
                           ? "running"
                           : "stopped";
        target.online = target.state == "running";
        report.targets.push_back(target);
      }
      return report;
    }

    case RuntimeBackendKind::kNative: {
      report.backend_available = true;
      RuntimeTarget target;
      target.backend_name = report.backend_name;
      target.serial = kNativeRuntimeSerial;
      target.state = "local";
      target.online = true;
      target.model = "Linuxoid Host";
      target.android_release = "self-healing-runtime";
      target.abi = DetectHostAbi();
      report.targets.push_back(target);

      std::ostringstream output;
      output << "native compat root: " << ResolveNativeCompatRoot() << "\n";
      output << "native spike root: " << ResolveNativeSpikeRoot() << "\n";
      output << "native runtime target: " << kNativeRuntimeSerial << "\n";
      output << "host abi: " << target.abi << "\n";
      report.backend_check_output = output.str();
      return report;
    }
  }

  throw std::invalid_argument("unsupported runtime backend enum");
}

RuntimeDiscoveryReport DiscoverRuntimeTargets(RuntimeBackendKind backend) {
  return DiscoverRuntimeTargetsWithRunner(backend, MakeShellRunner());
}

RuntimePreflightReport PreflightRuntimeWithRunner(
    const RuntimePreflightSpec& spec, const CommandRunner& runner) {
  RuntimePreflightReport report;
  report.backend_name = RenderRuntimeBackendName(spec.backend);
  report.serial = spec.serial;
  report.package_name = spec.package_name;
  report.component = spec.component;

  const auto discovery = DiscoverRuntimeTargetsWithRunner(spec.backend, runner);
  report.backend_available = discovery.backend_available;
  report.backend_check_output = discovery.backend_check_output;
  report.discovery_output = RenderRuntimeDiscoveryReport(discovery);

  if (!report.backend_available) {
    report.notes = "backend command is not available";
    return report;
  }

  if (discovery.targets.empty()) {
    report.notes = "no runtime targets were discovered";
    return report;
  }

  const RuntimeTarget* selected_target = nullptr;
  if (!spec.serial.empty()) {
    for (const auto& target : discovery.targets) {
      if (target.serial == spec.serial) {
        selected_target = &target;
        break;
      }
    }
    if (selected_target == nullptr) {
      report.notes = "requested serial was not discovered";
      return report;
    }
  } else {
    int online_targets = 0;
    for (const auto& target : discovery.targets) {
      if (target.online) {
        ++online_targets;
        selected_target = &target;
      }
    }
    if (online_targets == 0) {
      report.notes = "no online runtime targets were discovered";
      return report;
    }
    if (online_targets > 1) {
      report.notes = "multiple online targets were discovered; provide a serial";
      return report;
    }
    report.serial = selected_target->serial;
    report.notes = "auto-selected the only online target";
  }

  report.target_discovered = true;
  report.target_selected = true;
  report.target_online = selected_target->online;
  report.model = selected_target->model;
  report.android_release = selected_target->android_release;
  report.abi = selected_target->abi;

  if (!selected_target->online) {
    report.notes = "target is discovered but not online";
    return report;
  }

  if (!spec.package_name.empty() &&
      spec.backend == RuntimeBackendKind::kAttachedAdb) {
    const auto package_result = runner(
        WrapWithTimeout(BuildAdbPrefix(report.serial) + " shell pm list packages " +
                            QuoteForShell(spec.package_name),
                        kAdbDiscoveryTimeoutSeconds));
    report.package_check_output = package_result.output;
    report.package_visible =
        package_result.exit_code == 0 &&
        OutputContainsInstalledPackage(package_result.output, spec.package_name);
    if (package_result.exit_code == 124) {
      report.notes = "package visibility check timed out";
    } else if (!report.package_visible && report.notes.empty()) {
      report.notes = "package is not visible on the selected target";
    }

    if (report.package_visible) {
      if (!spec.component.empty()) {
        report.component_ready = true;
      } else {
        const auto launcher_attempt = ResolveAttachedAdbLauncherWithRunner(
            report.serial, spec.package_name, runner);
        report.component = launcher_attempt.resolved_component;
        report.component_ready = launcher_attempt.launcher_resolved;
        if (!report.component_ready && report.notes.empty()) {
          report.notes = launcher_attempt.timed_out
                             ? "launcher resolution timed out"
                             : "package is visible but no launcher activity was resolved";
        }
      }
    } else {
      report.component_ready = false;
    }
  } else if (spec.backend == RuntimeBackendKind::kNative &&
             !spec.package_name.empty()) {
    const auto metadata = QueryInstalledPackageMetadataWithRunner(
        {.backend = RuntimeBackendKind::kNative,
         .serial = report.serial,
         .package_name = spec.package_name},
        runner);
    report.package_check_output = metadata.package_check_output;
    report.package_visible = metadata.package_visible;
    if (!report.package_visible) {
      report.notes = metadata.notes;
      report.component_ready = false;
    } else if (!spec.component.empty()) {
      report.component = spec.component;
      report.component_ready =
          CanonicalizeAndroidComponent(spec.component).rfind(
              spec.package_name + "/", 0) == 0;
      if (!report.component_ready) {
        report.notes =
            "explicit native component does not belong to the requested package";
      }
    } else {
      report.component = metadata.resolved_component;
      report.component_ready = metadata.launcher_resolved;
      if (!report.component_ready) {
        report.notes = metadata.notes;
      }
    }

    if (report.package_visible && report.component_ready) {
      try {
        const auto diagnostics = BuildNativePreflightDiagnostics(
            spec.package_name);
        report.art_runtime_probe_source = diagnostics.art_runtime_probe_source;
        report.bootstrap_manifest_path = diagnostics.bootstrap_manifest_path;
        report.runtime_health_json_path = diagnostics.runtime_health_json_path;
        report.runtime_diagnostic_replay_json_path =
            diagnostics.runtime_diagnostic_replay_json_path;
        report.runtime_probe_ready = diagnostics.runtime_probe_ready;
        report.bootstrap_planned = diagnostics.bootstrap_planned;
        report.dependency_blocked = diagnostics.dependency_blocked;
        report.failing_subsystem_count =
            diagnostics.failing_subsystem_count;
        report.failing_subsystems = diagnostics.failing_subsystems;
        report.notes = diagnostics.notes;
      } catch (const std::exception& error) {
        report.notes = error.what();
        report.runtime_probe_ready = false;
        report.bootstrap_planned = false;
      }
    }
  } else {
    report.component_ready =
        spec.backend != RuntimeBackendKind::kAttachedAdb ||
        spec.package_name.empty() || !spec.component.empty();
    report.package_visible = spec.package_name.empty() || report.target_online;
  }

  if (report.notes.empty()) {
    report.notes = "runtime target looks ready for the requested launch path";
  }

  if (spec.backend == RuntimeBackendKind::kNative && !spec.package_name.empty()) {
    report.ready_for_launch =
        report.backend_available && report.target_selected &&
        report.target_online && report.package_visible &&
        report.component_ready && report.runtime_probe_ready &&
        report.bootstrap_planned;
  } else {
    report.ready_for_launch =
        report.backend_available && report.target_selected &&
        report.target_online && report.package_visible &&
        report.component_ready;
  }
  return report;
}

RuntimePreflightReport PreflightRuntime(const RuntimePreflightSpec& spec) {
  return PreflightRuntimeWithRunner(spec, MakeShellRunner());
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

InstalledPackageMetadataReport QueryInstalledPackageMetadataWithRunner(
    const InstalledPackageMetadataSpec& spec, const CommandRunner& runner) {
  if (!IsValidPackageName(spec.package_name)) {
    throw std::invalid_argument("package_name must look like a Java package");
  }

  InstalledPackageMetadataReport report;
  report.backend_name = RenderRuntimeBackendName(spec.backend);
  report.serial = spec.serial;
  report.package_name = spec.package_name;

  switch (spec.backend) {
    case RuntimeBackendKind::kAttachedAdb: {
      if (spec.serial.empty()) {
        throw std::invalid_argument(
            "attached-adb metadata lookup requires a target serial");
      }

      const std::string prefix = BuildAdbPrefix(spec.serial);
      const auto package_result = runner(
          WrapWithTimeout(prefix + " shell pm list packages " +
                              QuoteForShell(spec.package_name),
                          kAdbDiscoveryTimeoutSeconds));
      report.package_check_output = package_result.output;
      report.package_visible =
          package_result.exit_code == 0 &&
          OutputContainsInstalledPackage(package_result.output, spec.package_name);
      if (!report.package_visible) {
        if (package_result.exit_code == 124) {
          report.notes = "package visibility check timed out";
        } else {
          report.notes = "package is not visible on the selected target";
        }
        return report;
      }

      const auto launcher_attempt = ResolveAttachedAdbLauncherWithRunner(
          spec.serial, spec.package_name, runner);
      report.launcher_query_output = launcher_attempt.output;
      report.resolved_component = launcher_attempt.resolved_component;
      report.launcher_resolved = launcher_attempt.launcher_resolved;
      if (launcher_attempt.timed_out) {
        report.notes = "launcher resolution timed out";
      }

      const auto path_result = runner(
          WrapWithTimeout(prefix + " shell pm path " +
                              QuoteForShell(spec.package_name),
                          kAdbDiscoveryTimeoutSeconds));
      report.path_query_output = path_result.output;
      if (path_result.exit_code == 0) {
        report.install_path = ParseInstalledPackagePath(path_result.output);
      }

      const auto dump_result = runner(
          WrapWithTimeout(prefix + " shell dumpsys package " +
                              QuoteForShell(spec.package_name),
                          kAdbDiscoveryTimeoutSeconds));
      report.dump_output = dump_result.output;
      if (dump_result.exit_code == 0) {
        report.version_code =
            ParsePackageDumpField(dump_result.output, "versionCode");
        report.version_name =
            ParsePackageDumpField(dump_result.output, "versionName");
      }

      if (report.notes.empty()) {
        report.notes = report.launcher_resolved
                           ? "package metadata resolved from attached target"
                           : "package is visible but no launcher activity was resolved";
      }
      return report;
    }

    case RuntimeBackendKind::kWaydroid:
      report.notes =
          "installed package metadata lookup is not implemented for waydroid";
      return report;

    case RuntimeBackendKind::kNative: {
      if (!spec.serial.empty() && spec.serial != kNativeRuntimeSerial) {
        report.notes = "requested serial was not discovered";
        return report;
      }
      report.serial = kNativeRuntimeSerial;
      const auto lookup = ResolveNativePackageLookup(spec.package_name);
      report.package_visible = lookup.package_visible;
      report.launcher_resolved = lookup.launcher_resolved;
      report.resolved_component = lookup.launcher_component;
      report.install_path = lookup.apk_path;
      report.version_code = lookup.version_code;
      report.version_name = lookup.version_name;
      report.package_check_output = lookup.package_check_output;
      report.launcher_query_output = lookup.launcher_query_output;
      report.path_query_output = lookup.path_query_output;
      report.dump_output = lookup.dump_output;
      report.notes = lookup.notes;
      return report;
    }
  }

  throw std::invalid_argument("unsupported runtime backend enum");
}

InstalledPackageMetadataReport QueryInstalledPackageMetadata(
    const InstalledPackageMetadataSpec& spec) {
  return QueryInstalledPackageMetadataWithRunner(spec, MakeShellRunner());
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
      report.launch_classification = report.launch_ok
                                         ? "direct_backend_launch"
                                         : "direct_backend_launch_failed";
      report.output = result.output;
      return report;
    }

    case RuntimeBackendKind::kAttachedAdb: {
      if (spec.serial.empty()) {
        throw std::invalid_argument(
            "attached-adb backend requires a target serial");
      }

      if (spec.component.empty()) {
        const auto launcher_attempt = ResolveAttachedAdbLauncherWithRunner(
            spec.serial, spec.package_name, runner);
        report.component = launcher_attempt.resolved_component;
        if (!launcher_attempt.launcher_resolved) {
          report.output = launcher_attempt.timed_out
                              ? "launcher resolution timed out\n"
                              : "package is visible but no launcher activity was resolved\n";
          report.launch_ok = false;
          return report;
        }
      }

      const auto activity_report =
          LaunchAdbActivityWithRunner(
              spec.serial,
              report.component.empty() ? spec.component : report.component,
              runner);
      report.component = activity_report.component;
      report.launch_ok = activity_report.launch_ok;
      report.launch_classification = report.launch_ok
                                         ? "direct_backend_launch"
                                         : "direct_backend_launch_failed";
      report.output = activity_report.output;
      return report;
    }

    case RuntimeBackendKind::kNative: {
      const auto metadata = QueryInstalledPackageMetadataWithRunner(
          {.backend = RuntimeBackendKind::kNative,
           .serial = spec.serial,
           .package_name = spec.package_name},
          runner);
      report.serial = kNativeRuntimeSerial;
      if (!metadata.package_visible) {
        report.output = metadata.notes + "\n";
        report.launch_ok = false;
        return report;
      }

      report.component = spec.component.empty() ? metadata.resolved_component
                                                : spec.component;
      if (report.component.empty()) {
        report.output =
            "package is staged locally but no launcher component was resolved\n";
        report.launch_ok = false;
        return report;
      }

      try {
        const auto staged_report = BuildLoadedApkReportFromNativeLookup(
            ResolveNativePackageLookup(spec.package_name));
        const auto plan = BuildNativeLaunchPlan(
            staged_report, ResolveNativeSpikeRoot());
        const auto bootstrap = BuildNativeActivityBootstrap(
            plan, ResolveCompatctlPathForNativeRuntime());
        report.bootstrap_manifest_path = bootstrap.bootstrap_manifest_path;
        const auto execution = RunNativeArtBootstrapExecutionFixture(
            bootstrap.bootstrap_manifest_path);
        report.bootstrap_execution_result_path = execution.result_json_path;
        report.bootstrap_execution_trace_jsonl_path = execution.trace_jsonl_path;
        report.bootstrap_execution_runner_state_json_path =
            execution.runner_state_json_path;
        report.art_runtime_probe_source = execution.art_runtime_probe_source;
        const auto health = RunRuntimeHealthFixture(
            bootstrap.bootstrap_manifest_path, "baseline");
        report.runtime_health_json_path = health.health_json_path;
        report.runtime_recovery_plan_path = health.recovery_plan_path;
        report.runtime_health_ready = health.self_healing_ready;
        report.runtime_dependency_blocked = health.dependency_blocked;
        report.runtime_failing_subsystem_count =
            health.failing_subsystem_count;
        report.runtime_failing_subsystems = health.failing_subsystems;
        const auto diagnostic = ReplayRuntimeDiagnosticBundle(
            bootstrap.bootstrap_manifest_path);
        report.runtime_diagnostic_replay_json_path =
            diagnostic.result_json_path;
        report.runtime_diagnostic_trace_index_path =
            diagnostic.trace_index_json_path;
        report.runtime_diagnostic_replay_ready = diagnostic.replay_ready;
        report.runtime_trace_bundle_complete =
            diagnostic.trace_bundle_complete;
        const bool override_allowed =
            EnvFlagEnabled("LINUXOID_NATIVE_ALLOW_RUNTIME_OVERRIDE");
        const bool override_only_success =
            execution.execution_succeeded &&
            execution.art_runtime_probe_source == "override" &&
            !override_allowed;
        report.launch_ok = execution.execution_succeeded && !override_only_success;
        if (override_only_success) {
          report.launch_classification = "fixture_override_rejected";
          report.output =
              RenderNativeArtBootstrapExecutionFixtureJson(execution) +
              "\nlaunch classification: override-backed bootstrap success is "
              "fixture-only; set LINUXOID_NATIVE_ALLOW_RUNTIME_OVERRIDE=1 "
              "to treat it as a successful native launch\n";
        } else {
          report.launch_classification = report.launch_ok
                                             ? "native_bootstrap_execution_succeeded"
                                             : "native_bootstrap_execution_failed";
          report.output =
              RenderNativeArtBootstrapExecutionFixtureJson(execution);
        }
      } catch (const std::exception& error) {
        report.launch_ok = false;
        report.launch_classification = "native_bootstrap_execution_error";
        report.output = std::string(error.what()) + "\n";
      }
      return report;
    }
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
