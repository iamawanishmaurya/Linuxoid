#include "wfa/apk_runtime_bridge.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace wfa {

namespace fs = std::filesystem;

namespace {

struct ContractValidationOutcome {
  enum class State {
    kValid,
    kMissing,
    kMalformed,
    kIncompatible,
    kStale,
    kIncomplete,
  };

  State state = State::kValid;
  std::vector<std::string> diagnostics;
  std::vector<std::string> healing_actions;
};

std::string EscapeJson(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (const char character : value) {
    switch (character) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      default:
        escaped.push_back(character);
        break;
    }
  }
  return escaped;
}

std::string RenderJsonArray(const std::vector<std::string>& values) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    output << "\"" << EscapeJson(values[index]) << "\"";
  }
  output << "]";
  return output.str();
}

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

std::string ReadTextFile(const fs::path& path) {
  std::ifstream input(path);
  if (!input) {
    return "";
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void AppendUnique(std::vector<std::string>* values, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(value);
  }
}

std::string_view TrimWhitespace(std::string_view value) {
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.remove_prefix(1);
  }
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.remove_suffix(1);
  }
  return value;
}

bool LooksLikeJsonObject(std::string_view json) {
  const std::string_view trimmed = TrimWhitespace(json);
  return trimmed.size() >= 2 && trimmed.front() == '{' &&
         trimmed.back() == '}';
}

std::uint64_t ComputeFnv1a64(std::string_view value) {
  constexpr std::uint64_t kOffset = 1469598103934665603ull;
  constexpr std::uint64_t kPrime = 1099511628211ull;
  std::uint64_t hash = kOffset;
  for (const unsigned char character : value) {
    hash ^= static_cast<std::uint64_t>(character);
    hash *= kPrime;
  }
  return hash;
}

std::uint64_t ComputeDeterministicUnixMs(const std::string& package_name,
                                         int user_id, int app_id,
                                         const std::string& launch_component) {
  const std::uint64_t hash = ComputeFnv1a64(
      package_name + ":" + std::to_string(user_id) + ":" +
      std::to_string(app_id) + ":" + launch_component + ":runtime");
  return 1730000000000ull + (hash % 1000000000ull);
}

std::optional<std::string> ExtractJsonStringField(const std::string& json,
                                                  const std::string& field) {
  std::smatch match;
  if (!std::regex_search(json, match,
                         std::regex("\"" + field + "\"\\s*:\\s*\"([^\"]*)\"")) ||
      match.size() < 2) {
    return std::nullopt;
  }
  return match[1].str();
}

std::optional<long long> ExtractJsonIntegerField(const std::string& json,
                                                 const std::string& field) {
  std::smatch match;
  if (!std::regex_search(json, match,
                         std::regex("\"" + field + "\"\\s*:\\s*([0-9]+)")) ||
      match.size() < 2) {
    return std::nullopt;
  }
  return std::stoll(match[1].str());
}

std::optional<bool> ExtractJsonBoolField(const std::string& json,
                                         const std::string& field) {
  std::smatch match;
  if (!std::regex_search(json, match,
                         std::regex("\"" + field + "\"\\s*:\\s*(true|false)")) ||
      match.size() < 2) {
    return std::nullopt;
  }
  return match[1].str() == "true";
}

bool JsonContainsArrayField(const std::string& json, const std::string& field) {
  return std::regex_search(json, std::regex("\"" + field + "\"\\s*:\\s*\\["));
}

ContractValidationOutcome ValidateExistingContract(
    const fs::path& path, const std::string& expected_schema,
    const std::string& expected_package_name, int expected_user_id,
    int expected_app_id, const std::string& expected_sandbox_root,
    const std::string& expected_app_data_dir,
    const std::string& expected_apk_path,
    const std::string& expected_staged_dir,
    std::uint64_t expected_updated_at_unix_ms,
    const std::string& missing_diagnostic,
    const std::string& missing_action,
    const std::string& malformed_diagnostic,
    const std::string& malformed_action,
    const std::string& incompatible_diagnostic,
    const std::string& incompatible_action,
    const std::string& stale_diagnostic,
    const std::string& stale_action) {
  ContractValidationOutcome outcome;
  if (!fs::exists(path)) {
    outcome.state = ContractValidationOutcome::State::kMissing;
    outcome.diagnostics = {missing_diagnostic};
    outcome.healing_actions = {missing_action};
    return outcome;
  }

  const std::string json = ReadTextFile(path);
  if (json.empty() || !LooksLikeJsonObject(json)) {
    outcome.state = ContractValidationOutcome::State::kMalformed;
    outcome.diagnostics = {malformed_diagnostic};
    outcome.healing_actions = {malformed_action};
    return outcome;
  }

  const auto schema_version = ExtractJsonStringField(json, "schema_version");
  const auto package_name = ExtractJsonStringField(json, "package_name");
  const auto sandbox_root = ExtractJsonStringField(json, "sandbox_root");
  const auto app_data_dir = ExtractJsonStringField(json, "app_data_dir");
  const auto apk_path = ExtractJsonStringField(json, "apk_path");
  const auto staged_dir = ExtractJsonStringField(json, "staged_dir");
  const auto user_id = ExtractJsonIntegerField(json, "user_id");
  const auto app_id = ExtractJsonIntegerField(json, "app_id");
  const auto updated_at_unix_ms =
      ExtractJsonIntegerField(json, "updated_at_unix_ms");
  const auto contract_ready = ExtractJsonBoolField(json, "contract_ready");

  if (!schema_version || !package_name || !sandbox_root || !app_data_dir ||
      !apk_path || !staged_dir || !user_id || !app_id || !updated_at_unix_ms ||
      !contract_ready) {
    outcome.state = ContractValidationOutcome::State::kMalformed;
    outcome.diagnostics = {malformed_diagnostic};
    outcome.healing_actions = {malformed_action};
    return outcome;
  }

  if (*schema_version != expected_schema ||
      *package_name != expected_package_name ||
      *sandbox_root != expected_sandbox_root ||
      *app_data_dir != expected_app_data_dir ||
      *apk_path != expected_apk_path || *staged_dir != expected_staged_dir ||
      *user_id != expected_user_id || *app_id != expected_app_id) {
    outcome.state = ContractValidationOutcome::State::kIncompatible;
    outcome.diagnostics = {incompatible_diagnostic};
    outcome.healing_actions = {incompatible_action};
    return outcome;
  }

  if (static_cast<std::uint64_t>(*updated_at_unix_ms) !=
          expected_updated_at_unix_ms ||
      !*contract_ready) {
    outcome.state = ContractValidationOutcome::State::kStale;
    outcome.diagnostics = {stale_diagnostic};
    outcome.healing_actions = {stale_action};
    return outcome;
  }

  return outcome;
}

bool RuntimeContractLooksComplete(const std::string& json,
                                  const NativeApkRuntimeBridgeReport& report) {
  if (!JsonContainsArrayField(json, "boot_classpath_entries") ||
      !JsonContainsArrayField(json, "native_library_dirs") ||
      !JsonContainsArrayField(json, "dex_files") ||
      !JsonContainsArrayField(json, "states_visited") ||
      !JsonContainsArrayField(json, "healing_actions") ||
      !JsonContainsArrayField(json, "diagnostics") ||
      !JsonContainsArrayField(json, "errors")) {
    return false;
  }

  const auto runtime_handle = ExtractJsonStringField(json, "runtime_handle");
  const auto process_identity =
      ExtractJsonStringField(json, "process_identity");
  const auto activity_component =
      ExtractJsonStringField(json, "activity_component");
  const auto bootstrap_state =
      ExtractJsonStringField(json, "bootstrap_state");
  const auto blocking_reason = ExtractJsonStringField(json, "blocking_reason");
  const auto recommended_recovery_action =
      ExtractJsonStringField(json, "recommended_recovery_action");
  const auto report_json_path = ExtractJsonStringField(json, "report_json_path");
  const auto session_map_path = ExtractJsonStringField(json, "session_map_path");
  const auto event_log_path = ExtractJsonStringField(json, "event_log_path");

  if (!runtime_handle || !process_identity || !activity_component ||
      !bootstrap_state || !blocking_reason ||
      !recommended_recovery_action || !report_json_path || !session_map_path ||
      !event_log_path) {
    return false;
  }

  return *runtime_handle == report.runtime_handle &&
         *process_identity == report.process_identity &&
         *activity_component == report.activity_component &&
         *bootstrap_state == report.bootstrap_state &&
         *blocking_reason == report.blocking_reason &&
         *recommended_recovery_action == report.recommended_recovery_action &&
         *report_json_path == report.report_json_path &&
         *session_map_path == report.session_map_path &&
         *event_log_path == report.event_log_path;
}

std::vector<std::string> SplitPathList(const std::string& value) {
  std::vector<std::string> parts;
  std::string current;
  std::istringstream input(value);
  while (std::getline(input, current, ':')) {
    if (!current.empty()) {
      parts.push_back(current);
    }
  }
  return parts;
}

struct RuntimeDiscovery {
  std::string runtime_root;
  std::string discovery_source = "none";
  std::string boot_classpath;
  std::vector<std::string> boot_classpath_entries;
  std::vector<std::string> native_library_dirs;
  bool art_runtime_available = false;
};

RuntimeDiscovery DiscoverRuntime(const NativeApkRuntimeBridgeContext& context) {
  RuntimeDiscovery discovery;
  std::vector<fs::path> candidates;

  if (!context.runtime_root_override.empty()) {
    candidates.emplace_back(context.runtime_root_override);
  }
  if (!context.runtime_probe_override.empty()) {
    fs::path probe_path = fs::path(context.runtime_probe_override);
    if (probe_path.has_parent_path()) {
      candidates.push_back(probe_path.parent_path().parent_path());
    }
  }
  if (!context.disable_host_runtime_probe) {
    candidates.emplace_back("/opt/linuxoid/art-runtime");
    candidates.emplace_back("/usr/lib/android-runtime");
    candidates.emplace_back("/usr/local/lib/android-runtime");
  }

  for (const auto& candidate : candidates) {
    if (candidate.empty() || !fs::exists(candidate)) {
      continue;
    }
    discovery.runtime_root = candidate.string();
    if (!context.runtime_root_override.empty() &&
        candidate == fs::path(context.runtime_root_override)) {
      discovery.discovery_source = "env_override_root";
    } else if (!context.runtime_probe_override.empty()) {
      discovery.discovery_source = "probe_override_root";
    } else {
      discovery.discovery_source = "host_guess";
    }
    break;
  }

  if (discovery.runtime_root.empty()) {
    return discovery;
  }

  const fs::path runtime_root(discovery.runtime_root);
  const fs::path bootclasspath_path =
      runtime_root / "framework" / "bootclasspath.txt";
  if (fs::exists(bootclasspath_path)) {
    std::string bootclasspath_contents = ReadTextFile(bootclasspath_path);
    while (!bootclasspath_contents.empty() &&
           (bootclasspath_contents.back() == '\n' ||
            bootclasspath_contents.back() == '\r')) {
      bootclasspath_contents.pop_back();
    }
    discovery.boot_classpath = bootclasspath_contents;
    discovery.boot_classpath_entries = SplitPathList(bootclasspath_contents);
  } else {
    std::vector<std::string> fallback_entries;
    const fs::path core_oj = runtime_root / "framework" / "core-oj.jar";
    const fs::path core_libart = runtime_root / "framework" / "core-libart.jar";
    if (fs::exists(core_oj)) {
      fallback_entries.push_back("framework/core-oj.jar");
    }
    if (fs::exists(core_libart)) {
      fallback_entries.push_back("framework/core-libart.jar");
    }
    if (!fallback_entries.empty()) {
      discovery.boot_classpath_entries = fallback_entries;
      discovery.boot_classpath = fallback_entries.front();
      for (std::size_t index = 1; index < fallback_entries.size(); ++index) {
        discovery.boot_classpath += ":" + fallback_entries[index];
      }
    }
  }

  const fs::path lib64_dir = runtime_root / "lib64";
  const fs::path lib_dir = runtime_root / "lib";
  if (fs::exists(lib64_dir) && fs::is_directory(lib64_dir)) {
    discovery.native_library_dirs.push_back(lib64_dir.string());
  }
  if (fs::exists(lib_dir) && fs::is_directory(lib_dir)) {
    discovery.native_library_dirs.push_back(lib_dir.string());
  }

  const bool has_libart =
      fs::exists(lib64_dir / "libart.so") || fs::exists(lib_dir / "libart.so");
  const bool has_runtime_probe =
      fs::exists(runtime_root / "bin" / "dalvikvm64") ||
      fs::exists(runtime_root / "bin" / "dalvikvm") ||
      fs::exists(runtime_root / "bin" / "app_process64") ||
      fs::exists(runtime_root / "bin" / "app_process");
  discovery.art_runtime_available =
      (has_libart || has_runtime_probe) &&
      !discovery.boot_classpath_entries.empty() &&
      !discovery.native_library_dirs.empty();
  return discovery;
}

std::string DetermineRuntimeBlockingReason(
    const NativeApkRuntimeBridgeContext& context,
    const RuntimeDiscovery& discovery) {
  if (!context.launch_ready) {
    if (context.native_post_dispatch_blocker != "none") {
      return context.native_post_dispatch_blocker;
    }
    return "launch_not_ready";
  }
  if (context.storage_health != "ready" || context.sandbox_health != "ready") {
    return "app_storage_not_ready";
  }
  if (context.permission_health != "ready" || context.app_ops_health != "ready") {
    return "permission_state_not_ready";
  }
  if (context.binder_health != "ready") {
    return "binder_not_ready";
  }
  if (context.activity_health != "ready" || !context.activity_launch_ready) {
    return context.activity_launch_blocking_reason == "none"
               ? "activity_launch_not_ready"
               : context.activity_launch_blocking_reason;
  }
  if (context.activity_manager_health != "ready" ||
      !context.activity_manager_ready) {
    return "activity_manager_not_ready";
  }
  if (context.process_health != "ready" || !context.process_ready) {
    return "process_manager_not_ready";
  }
  if (context.window_health != "ready" || !context.window_ready) {
    return "window_manager_not_ready";
  }
  if (context.surface_health != "ready" || !context.surface_created ||
      !context.surface_first_frame_presented) {
    return "surface_not_ready";
  }
  if (context.lifecycle_health != "ready" || context.looper_health != "ready" ||
      context.input_health != "ready") {
    return "lifecycle_not_ready";
  }
  if (context.dex_health != "ready" || !context.dex_bootstrap_ready ||
      !context.class_loader_ready || context.art_health != "ready") {
    return "dex_bootstrap_not_ready";
  }
  if (context.dex_files.empty()) {
    return "dex_payload_unavailable";
  }
  if (discovery.runtime_root.empty()) {
    return "art_runtime_unavailable";
  }
  if (discovery.boot_classpath_entries.empty()) {
    return "boot_classpath_unavailable";
  }
  if (discovery.native_library_dirs.empty()) {
    return "native_runtime_libraries_unavailable";
  }
  if (context.simulate_bootstrap_failure && !context.bootstrap_recovered) {
    return "runtime_bootstrap_failed";
  }
  return "none";
}

std::string DetermineRuntimeRecoveryAction(const std::string& blocking_reason) {
  if (blocking_reason.rfind("framework-boundary-stubbed:", 0) == 0 ||
      blocking_reason.rfind("framework-boundary-unimplemented:", 0) == 0 ||
      blocking_reason.rfind("managed-post-dispatch-probe-blocked:", 0) == 0) {
    return "extend_runtime_context_bridge";
  }
  if (blocking_reason == "activity_manager_not_ready" ||
      blocking_reason == "process_manager_not_ready") {
    return "rebuild_process_manager_state";
  }
  if (blocking_reason == "window_manager_not_ready" ||
      blocking_reason == "surface_not_ready") {
    return "rebuild_window_manager_state";
  }
  if (blocking_reason == "dex_bootstrap_not_ready" ||
      blocking_reason == "dex_payload_unavailable") {
    return "rebuild_dex_bootstrap";
  }
  if (blocking_reason == "activity_launch_not_ready") {
    return "rerun_intent_resolution";
  }
  if (blocking_reason == "app_storage_not_ready") {
    return "repair_app_storage";
  }
  if (blocking_reason == "permission_state_not_ready") {
    return "rebuild_permission_state";
  }
  if (blocking_reason == "binder_not_ready") {
    return "refresh_binder_services";
  }
  if (blocking_reason == "art_runtime_unavailable" ||
      blocking_reason == "boot_classpath_unavailable" ||
      blocking_reason == "native_runtime_libraries_unavailable" ||
      blocking_reason == "runtime_bootstrap_failed") {
    return "retry_runtime_bootstrap";
  }
  return "none";
}

std::vector<std::string> BuildRuntimeStatesVisited(
    const RuntimeDiscovery& discovery, const std::string& bootstrap_state) {
  std::vector<std::string> states;
  if (discovery.runtime_root.empty()) {
    states.push_back("unavailable");
    return states;
  }
  states.push_back("discovered");
  if (!discovery.boot_classpath_entries.empty() &&
      !discovery.native_library_dirs.empty()) {
    states.push_back("configured");
  }
  if (bootstrap_state == "ready" || bootstrap_state == "failed" ||
      bootstrap_state == "recovered") {
    states.push_back("bootstrapping");
  }
  if (bootstrap_state != "discovered" && bootstrap_state != "configured") {
    states.push_back(bootstrap_state);
  }
  return states;
}

std::string RenderRuntimeBridgeJson(const NativeApkRuntimeBridgeReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"schema_version\": \"" << EscapeJson(report.schema_version)
         << "\",\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"contract_ready\": " << (report.contract_ready ? "true" : "false")
         << ",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id) << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"report_json_path\": \"" << EscapeJson(report.report_json_path)
         << "\",\n"
         << "  \"session_map_path\": \"" << EscapeJson(report.session_map_path)
         << "\",\n"
         << "  \"event_log_path\": \"" << EscapeJson(report.event_log_path)
         << "\",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"user_id\": " << report.user_id << ",\n"
         << "  \"app_id\": " << report.app_id << ",\n"
         << "  \"uid_placeholder\": " << report.uid_placeholder << ",\n"
         << "  \"gid_placeholder\": " << report.gid_placeholder << ",\n"
         << "  \"sandbox_root\": \"" << EscapeJson(report.sandbox_root)
         << "\",\n"
         << "  \"app_data_dir\": \"" << EscapeJson(report.app_data_dir)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir)
         << "\",\n"
         << "  \"updated_at_unix_ms\": " << report.updated_at_unix_ms << ",\n"
         << "  \"runtime_handle\": \"" << EscapeJson(report.runtime_handle)
         << "\",\n"
         << "  \"process_session_id\": \"" << EscapeJson(report.process_session_id)
         << "\",\n"
         << "  \"process_identity\": \"" << EscapeJson(report.process_identity)
         << "\",\n"
         << "  \"process_name\": \"" << EscapeJson(report.process_name)
         << "\",\n"
         << "  \"pid_value\": " << report.pid_value << ",\n"
         << "  \"pid_source\": \"" << EscapeJson(report.pid_source)
         << "\",\n"
         << "  \"window_session_id\": \"" << EscapeJson(report.window_session_id)
         << "\",\n"
         << "  \"window_id\": \"" << EscapeJson(report.window_id) << "\",\n"
         << "  \"activity_component\": \""
         << EscapeJson(report.activity_component) << "\",\n"
         << "  \"launch_component\": \""
         << EscapeJson(report.launch_component) << "\",\n"
         << "  \"runtime_root\": \"" << EscapeJson(report.runtime_root)
         << "\",\n"
         << "  \"discovery_source\": \"" << EscapeJson(report.discovery_source)
         << "\",\n"
         << "  \"boot_classpath\": \"" << EscapeJson(report.boot_classpath)
         << "\",\n"
         << "  \"boot_classpath_entries\": "
         << RenderJsonArray(report.boot_classpath_entries) << ",\n"
         << "  \"native_library_dirs\": "
         << RenderJsonArray(report.native_library_dirs) << ",\n"
         << "  \"dex_files\": " << RenderJsonArray(report.dex_files) << ",\n"
         << "  \"art_runtime_required\": "
         << (report.art_runtime_required ? "true" : "false") << ",\n"
         << "  \"art_runtime_available\": "
         << (report.art_runtime_available ? "true" : "false") << ",\n"
         << "  \"class_loader_ready\": "
         << (report.class_loader_ready ? "true" : "false") << ",\n"
         << "  \"bytecode_execution_ready\": "
         << (report.bytecode_execution_ready ? "true" : "false") << ",\n"
         << "  \"java_execution_supported\": "
         << (report.java_execution_supported ? "true" : "false") << ",\n"
         << "  \"headless_safe\": "
         << (report.headless_safe ? "true" : "false") << ",\n"
         << "  \"wayland_surface_available\": "
         << (report.wayland_surface_available ? "true" : "false") << ",\n"
         << "  \"egl_surface_available\": "
         << (report.egl_surface_available ? "true" : "false") << ",\n"
         << "  \"bootstrap_state\": \"" << EscapeJson(report.bootstrap_state)
         << "\",\n"
         << "  \"native_post_dispatch_state\": \""
         << EscapeJson(report.native_post_dispatch_state) << "\",\n"
         << "  \"native_post_dispatch_blocker\": \""
         << EscapeJson(report.native_post_dispatch_blocker) << "\",\n"
         << "  \"native_post_dispatch_recovery_action\": \""
         << EscapeJson(report.native_post_dispatch_recovery_action)
         << "\",\n"
         << "  \"native_post_dispatch_backend\": \""
         << EscapeJson(report.native_post_dispatch_backend) << "\",\n"
         << "  \"blocking_reason\": \"" << EscapeJson(report.blocking_reason)
         << "\",\n"
         << "  \"recommended_recovery_action\": \""
         << EscapeJson(report.recommended_recovery_action) << "\",\n"
         << "  \"states_visited\": " << RenderJsonArray(report.states_visited)
         << ",\n"
         << "  \"healing_actions\": "
         << RenderJsonArray(report.healing_actions) << ",\n"
         << "  \"diagnostics\": " << RenderJsonArray(report.diagnostics)
         << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

void WriteRuntimeSessionMap(const NativeApkRuntimeBridgeReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"runtime_handle\": \"" << EscapeJson(report.runtime_handle)
         << "\",\n"
         << "  \"process_session_id\": \"" << EscapeJson(report.process_session_id)
         << "\",\n"
         << "  \"window_session_id\": \"" << EscapeJson(report.window_session_id)
         << "\",\n"
         << "  \"activity_component\": \""
         << EscapeJson(report.activity_component) << "\",\n"
         << "  \"bootstrap_state\": \"" << EscapeJson(report.bootstrap_state)
         << "\"\n"
         << "}\n";
  WriteTextFile(report.session_map_path, output.str());
}

void WriteRuntimeEventLog(const NativeApkRuntimeBridgeReport& report) {
  std::ostringstream output;
  for (const auto& state : report.states_visited) {
    output << "{"
           << "\"state\": \"" << EscapeJson(state) << "\", "
           << "\"runtime_handle\": \"" << EscapeJson(report.runtime_handle)
           << "\", "
           << "\"blocking_reason\": \"" << EscapeJson(report.blocking_reason)
           << "\"}\n";
  }
  WriteTextFile(report.event_log_path, output.str());
}

}  // namespace

NativeApkRuntimeBridgeSession::NativeApkRuntimeBridgeSession(
    NativeApkRuntimeBridgeContext context)
    : context_(std::move(context)) {}

const NativeApkRuntimeBridgeContext& NativeApkRuntimeBridgeSession::context()
    const {
  return context_;
}

NativeApkRuntimeBridgeReport NativeApkRuntimeBridgeSession::BuildReport() const {
  fs::create_directories(context_.artifact_root);

  NativeApkRuntimeBridgeReport report;
  report.session_id = context_.session_id;
  report.artifact_root = context_.artifact_root;
  report.report_json_path =
      (fs::path(context_.artifact_root) / "runtime-state.json").string();
  report.session_map_path =
      (fs::path(context_.artifact_root) / "runtime-session-map.json").string();
  report.event_log_path =
      (fs::path(context_.artifact_root) / "runtime-events.jsonl").string();
  report.package_name = context_.package_name;
  report.user_id = context_.user_id;
  report.app_id = context_.app_id;
  report.uid_placeholder = context_.uid_placeholder;
  report.gid_placeholder = context_.gid_placeholder;
  report.sandbox_root = context_.sandbox_root;
  report.app_data_dir = context_.app_data_dir;
  report.apk_path = context_.apk_path;
  report.staged_dir = context_.staged_dir;
  report.updated_at_unix_ms = ComputeDeterministicUnixMs(
      context_.package_name, context_.user_id, context_.app_id,
      context_.resolved_component.empty() ? context_.launcher_component
                                          : context_.resolved_component);
  report.runtime_handle =
      context_.package_name + ":" + context_.install_id + ":art-runtime";
  report.process_session_id = context_.process_session_id;
  report.process_identity = context_.process_identity;
  report.process_name = context_.process_name;
  report.pid_value = context_.pid_value;
  report.pid_source = context_.pid_source;
  report.window_session_id = context_.window_session_id;
  report.window_id = context_.window_id;
  report.activity_component = context_.resolved_component.empty()
                                  ? context_.launcher_component
                                  : context_.resolved_component;
  report.launch_component = report.activity_component;
  report.headless_safe = true;
  report.wayland_surface_available = context_.wayland_surface_available;
  report.egl_surface_available = context_.egl_surface_available;
  report.native_post_dispatch_state = context_.native_post_dispatch_state;
  report.native_post_dispatch_blocker = context_.native_post_dispatch_blocker;
  report.native_post_dispatch_recovery_action =
      context_.native_post_dispatch_recovery_action;
  report.native_post_dispatch_backend = context_.native_post_dispatch_backend;
  report.class_loader_ready = context_.class_loader_ready;
  report.java_execution_supported = false;
  report.bytecode_execution_ready = false;
  report.dex_files = context_.dex_files;

  const RuntimeDiscovery discovery = DiscoverRuntime(context_);
  report.runtime_root = discovery.runtime_root;
  report.discovery_source = discovery.discovery_source;
  report.boot_classpath = discovery.boot_classpath;
  report.boot_classpath_entries = discovery.boot_classpath_entries;
  report.native_library_dirs = discovery.native_library_dirs;
  report.art_runtime_available = discovery.art_runtime_available;

  ContractValidationOutcome validation = ValidateExistingContract(
      report.report_json_path, report.schema_version, report.package_name,
      report.user_id, report.app_id, report.sandbox_root, report.app_data_dir,
      report.apk_path, report.staged_dir, report.updated_at_unix_ms,
      "runtime_bridge_state_missing", "rebuild_missing_runtime_bridge_state",
      "runtime_bridge_state_malformed",
      "rebuild_malformed_runtime_bridge_state",
      "runtime_bridge_state_incompatible",
      "rebuild_incompatible_runtime_bridge_state",
      "runtime_bridge_state_stale", "rebuild_stale_runtime_bridge_state");
  const std::string existing_json = ReadTextFile(report.report_json_path);
  bool persisted_state_invalid = false;
  if (validation.state == ContractValidationOutcome::State::kValid &&
      (!RuntimeContractLooksComplete(existing_json, report) ||
       !fs::exists(report.session_map_path) || !fs::exists(report.event_log_path))) {
    persisted_state_invalid = true;
    validation.state = ContractValidationOutcome::State::kIncomplete;
    AppendUnique(&report.diagnostics, "runtime_bridge_state_incomplete");
    AppendUnique(&report.healing_actions,
                 "rebuild_incomplete_runtime_bridge_state");
    AppendUnique(
        &report.diagnostics,
        "Self-Healing Android Device runtime bridge contract incomplete; rebuilding deterministic state");
  } else if (validation.state != ContractValidationOutcome::State::kValid) {
    persisted_state_invalid =
        validation.state != ContractValidationOutcome::State::kMissing ||
        context_.persisted_artifact_root_preexisting;
    for (const auto& diagnostic : validation.diagnostics) {
      AppendUnique(&report.diagnostics, diagnostic);
    }
    for (const auto& action : validation.healing_actions) {
      AppendUnique(&report.healing_actions, action);
    }
    if (persisted_state_invalid) {
      AppendUnique(
          &report.diagnostics,
          "Self-Healing Android Device runtime bridge contract invalid; rebuilding deterministic state");
    }
  }

  const std::string blocking_reason =
      DetermineRuntimeBlockingReason(context_, discovery);
  report.blocking_reason = blocking_reason;
  report.recommended_recovery_action =
      DetermineRuntimeRecoveryAction(blocking_reason);

  if (discovery.runtime_root.empty()) {
    report.bootstrap_state = "unavailable";
  } else if (discovery.boot_classpath_entries.empty() ||
             discovery.native_library_dirs.empty()) {
    report.bootstrap_state = "degraded";
  } else if (context_.simulate_bootstrap_failure &&
             !context_.bootstrap_recovered) {
    report.bootstrap_state = "failed";
  } else if (discovery.art_runtime_available) {
    report.bootstrap_state =
        context_.bootstrap_recovered ? "recovered" : "ready";
    report.bytecode_execution_ready = false;
  } else {
    report.bootstrap_state = "configured";
  }

  report.states_visited =
      BuildRuntimeStatesVisited(discovery, report.bootstrap_state);
  if (context_.bootstrap_recovered) {
    AppendUnique(&report.diagnostics, "Self-Healing Android Device runtime bootstrap recovered after retry");
    AppendUnique(&report.healing_actions, "retry_runtime_bootstrap");
  }
  if (report.bootstrap_state == "unavailable") {
    AppendUnique(&report.diagnostics,
                 "Self-Healing Android Device runtime bridge ready in probe-only mode without a discovered ART runtime root");
  }

  if (persisted_state_invalid && !context_.allow_persisted_contract_repair) {
    report.ready = false;
    report.contract_ready = false;
    report.bootstrap_state = "failed";
    report.blocking_reason = "persisted_runtime_bridge_state_invalid";
    report.recommended_recovery_action = "rebuild_runtime_bridge_state";
    AppendUnique(&report.errors, report.blocking_reason);
    return report;
  }

  WriteRuntimeSessionMap(report);
  WriteRuntimeEventLog(report);
  WriteTextFile(report.report_json_path, RenderRuntimeBridgeJson(report));

  const bool upstream_dependency_blocked =
      blocking_reason == "launch_not_ready" ||
      blocking_reason == "app_storage_not_ready" ||
      blocking_reason == "permission_state_not_ready" ||
      blocking_reason == "binder_not_ready" ||
      blocking_reason == "activity_launch_not_ready" ||
      blocking_reason == "activity_manager_not_ready" ||
      blocking_reason == "process_manager_not_ready" ||
      blocking_reason == "window_manager_not_ready" ||
      blocking_reason == "surface_not_ready" ||
      blocking_reason == "lifecycle_not_ready" ||
      blocking_reason == "dex_bootstrap_not_ready" ||
      blocking_reason == "dex_payload_unavailable";

  report.contract_ready = true;
  report.ready = !upstream_dependency_blocked &&
                 report.bootstrap_state != "failed";

  if (!report.ready) {
    if (blocking_reason == "runtime_bootstrap_failed") {
      AppendUnique(&report.errors, "runtime_bootstrap_failed");
    } else if (upstream_dependency_blocked) {
      AppendUnique(&report.errors, "runtime_bridge_dependency_blocked:" +
                                      blocking_reason);
    }
  }

  return report;
}

}  // namespace wfa
