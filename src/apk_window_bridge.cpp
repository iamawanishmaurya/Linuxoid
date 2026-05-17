#include "wfa/apk_window_bridge.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
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
      std::to_string(app_id) + ":" + launch_component);
  return 1720000000000ull + (hash % 1000000000ull);
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

bool WindowManagerContractLooksComplete(
    const std::string& json, const NativeApkWindowManagerReport& report) {
  if (!JsonContainsArrayField(json, "states_visited") ||
      !JsonContainsArrayField(json, "healing_actions") ||
      !JsonContainsArrayField(json, "diagnostics") ||
      !JsonContainsArrayField(json, "errors")) {
    return false;
  }

  const auto window_id = ExtractJsonStringField(json, "window_id");
  const auto process_identity =
      ExtractJsonStringField(json, "process_identity");
  const auto launch_component =
      ExtractJsonStringField(json, "launch_component");
  const auto surface_session_id =
      ExtractJsonStringField(json, "surface_session_id");
  const auto backing_mode = ExtractJsonStringField(json, "backing_mode");
  const auto blocking_reason = ExtractJsonStringField(json, "blocking_reason");
  const auto recommended_recovery_action =
      ExtractJsonStringField(json, "recommended_recovery_action");
  const auto report_json_path = ExtractJsonStringField(json, "report_json_path");
  const auto session_map_path = ExtractJsonStringField(json, "session_map_path");
  const auto event_log_path = ExtractJsonStringField(json, "event_log_path");
  if (!window_id || !process_identity || !launch_component ||
      !surface_session_id || !backing_mode || !blocking_reason ||
      !recommended_recovery_action || !report_json_path || !session_map_path ||
      !event_log_path) {
    return false;
  }
  return *window_id == report.window_id &&
         *process_identity == report.process_identity &&
         *launch_component == report.launch_component &&
         *surface_session_id == report.surface_session_id &&
         *backing_mode == report.backing_mode &&
         *blocking_reason == report.blocking_reason &&
         *recommended_recovery_action ==
             report.recommended_recovery_action &&
         *report_json_path == report.report_json_path &&
         *session_map_path == report.session_map_path &&
         *event_log_path == report.event_log_path;
}

std::string DetermineWindowBlockingReason(
    const NativeApkWindowManagerContext& context) {
  if (!context.launch_ready) {
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
  if (context.activity_health != "ready") {
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
  if (context.surface_health != "ready" || !context.surface_first_frame_presented ||
      !context.surface_created) {
    return "surface_not_ready";
  }
  if (context.lifecycle_health != "ready") {
    return "lifecycle_not_ready";
  }
  if (context.looper_health != "ready") {
    return "looper_not_ready";
  }
  if (context.input_health != "ready") {
    return "input_not_ready";
  }
  if (context.dex_health != "ready") {
    return "dex_not_ready";
  }
  if (context.art_health != "ready") {
    return "art_bootstrap_not_ready";
  }
  if (context.resolved_component.empty() && context.launcher_component.empty()) {
    return "activity_component_not_resolved";
  }
  if (context.process_identity.empty()) {
    return "process_identity_not_ready";
  }
  if (context.surface_session_id.empty() || context.surface_session_root.empty()) {
    return "surface_session_not_materialized";
  }
  if (context.surface_width <= 0 || context.surface_height <= 0 ||
      context.surface_format <= 0) {
    return "surface_geometry_unavailable";
  }
  return "none";
}

std::string DetermineWindowRecoveryAction(
    const NativeApkWindowManagerContext& context,
    const std::string& blocking_reason) {
  if (blocking_reason == "none") {
    return "none";
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
  if (blocking_reason == "surface_not_ready" ||
      blocking_reason == "surface_session_not_materialized" ||
      blocking_reason == "surface_geometry_unavailable") {
    return "restart_surface";
  }
  if (blocking_reason == "lifecycle_not_ready" ||
      blocking_reason == "looper_not_ready") {
    return "restart_lifecycle";
  }
  if (blocking_reason == "input_not_ready") {
    return "reset_input_queue";
  }
  if (blocking_reason == "dex_not_ready" ||
      blocking_reason == "art_bootstrap_not_ready") {
    return "rebuild_dex_bootstrap";
  }
  if (blocking_reason == "activity_launch_not_ready" ||
      blocking_reason == "activity_component_not_resolved") {
    if (context.activity_launch_recovery_action != "none") {
      return context.activity_launch_recovery_action;
    }
    return "rerun_intent_resolution";
  }
  if (blocking_reason == "activity_manager_not_ready" ||
      blocking_reason == "process_manager_not_ready" ||
      blocking_reason == "process_identity_not_ready") {
    return "rebuild_process_manager_state";
  }
  return "rebuild_window_manager_state";
}

std::string BuildWindowStateJson(const NativeApkWindowManagerReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"schema_version\": \"" << EscapeJson(report.schema_version)
         << "\",\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"contract_ready\": "
         << (report.contract_ready ? "true" : "false") << ",\n"
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
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir) << "\",\n"
         << "  \"updated_at_unix_ms\": " << report.updated_at_unix_ms
         << ",\n"
         << "  \"window_id\": \"" << EscapeJson(report.window_id) << "\",\n"
         << "  \"process_identity\": \"" << EscapeJson(report.process_identity)
         << "\",\n"
         << "  \"process_name\": \"" << EscapeJson(report.process_name)
         << "\",\n"
         << "  \"pid_value\": " << report.pid_value << ",\n"
         << "  \"pid_source\": \"" << EscapeJson(report.pid_source)
         << "\",\n"
         << "  \"launch_component\": \"" << EscapeJson(report.launch_component)
         << "\",\n"
         << "  \"activity_component\": \""
         << EscapeJson(report.activity_component) << "\",\n"
         << "  \"lifecycle_state\": \"" << EscapeJson(report.lifecycle_state)
         << "\",\n"
         << "  \"surface_state\": \"" << EscapeJson(report.surface_state)
         << "\",\n"
         << "  \"backend\": \"" << EscapeJson(report.backend) << "\",\n"
         << "  \"backing_mode\": \"" << EscapeJson(report.backing_mode)
         << "\",\n"
         << "  \"headless_safe\": "
         << (report.headless_safe ? "true" : "false") << ",\n"
         << "  \"wayland_surface_available\": "
         << (report.wayland_surface_available ? "true" : "false") << ",\n"
         << "  \"egl_surface_available\": "
         << (report.egl_surface_available ? "true" : "false") << ",\n"
         << "  \"width\": " << report.width << ",\n"
         << "  \"height\": " << report.height << ",\n"
         << "  \"format\": " << report.format << ",\n"
         << "  \"surface_session_id\": \""
         << EscapeJson(report.surface_session_id) << "\",\n"
         << "  \"surface_session_root\": \""
         << EscapeJson(report.surface_session_root) << "\",\n"
         << "  \"surface_metadata_path\": \""
         << EscapeJson(report.surface_metadata_path) << "\",\n"
         << "  \"surface_event_log_path\": \""
         << EscapeJson(report.surface_event_log_path) << "\",\n"
         << "  \"marker_path\": \"" << EscapeJson(report.marker_path)
         << "\",\n"
         << "  \"surface_created\": "
         << (report.surface_created ? "true" : "false") << ",\n"
         << "  \"attached\": " << (report.attached ? "true" : "false")
         << ",\n"
         << "  \"visible\": " << (report.visible ? "true" : "false")
         << ",\n"
         << "  \"hidden\": " << (report.hidden ? "true" : "false")
         << ",\n"
         << "  \"resized\": " << (report.resized ? "true" : "false")
         << ",\n"
         << "  \"destroyed\": " << (report.destroyed ? "true" : "false")
         << ",\n"
         << "  \"failed\": " << (report.failed ? "true" : "false") << ",\n"
         << "  \"recovered\": " << (report.recovered ? "true" : "false")
         << ",\n"
         << "  \"window_state\": \"" << EscapeJson(report.window_state)
         << "\",\n"
         << "  \"blocking_reason\": \"" << EscapeJson(report.blocking_reason)
         << "\",\n"
         << "  \"recommended_recovery_action\": \""
         << EscapeJson(report.recommended_recovery_action) << "\",\n"
         << "  \"states_visited\": "
         << RenderJsonArray(report.states_visited) << ",\n"
         << "  \"healing_actions\": "
         << RenderJsonArray(report.healing_actions) << ",\n"
         << "  \"diagnostics\": " << RenderJsonArray(report.diagnostics)
         << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

std::string BuildWindowSessionMapJson(const NativeApkWindowManagerReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"window_id\": \"" << EscapeJson(report.window_id) << "\",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"user_id\": " << report.user_id << ",\n"
         << "  \"app_id\": " << report.app_id << ",\n"
         << "  \"process_identity\": \"" << EscapeJson(report.process_identity)
         << "\",\n"
         << "  \"activity_component\": \""
         << EscapeJson(report.activity_component) << "\",\n"
         << "  \"surface_session_id\": \""
         << EscapeJson(report.surface_session_id) << "\",\n"
         << "  \"surface_session_root\": \""
         << EscapeJson(report.surface_session_root) << "\",\n"
         << "  \"surface_metadata_path\": \""
         << EscapeJson(report.surface_metadata_path) << "\",\n"
         << "  \"surface_event_log_path\": \""
         << EscapeJson(report.surface_event_log_path) << "\",\n"
         << "  \"report_json_path\": \"" << EscapeJson(report.report_json_path)
         << "\",\n"
         << "  \"event_log_path\": \"" << EscapeJson(report.event_log_path)
         << "\",\n"
         << "  \"backend\": \"" << EscapeJson(report.backend) << "\",\n"
         << "  \"backing_mode\": \"" << EscapeJson(report.backing_mode)
         << "\",\n"
         << "  \"headless_safe\": "
         << (report.headless_safe ? "true" : "false") << "\n"
         << "}\n";
  return output.str();
}

std::string BuildWindowEventLog(const NativeApkWindowManagerReport& report) {
  std::ostringstream output;
  for (std::size_t index = 0; index < report.states_visited.size(); ++index) {
    output << "{"
           << "\"sequence_id\": " << (index + 1) << ", "
           << "\"state\": \"" << EscapeJson(report.states_visited[index])
           << "\", "
           << "\"window_id\": \"" << EscapeJson(report.window_id) << "\", "
           << "\"width\": " << report.width << ", "
           << "\"height\": " << report.height << ", "
           << "\"format\": " << report.format << "}\n";
  }
  return output.str();
}

}  // namespace

NativeApkWindowManagerSession::NativeApkWindowManagerSession(
    NativeApkWindowManagerContext context)
    : context_(std::move(context)) {}

const NativeApkWindowManagerContext&
NativeApkWindowManagerSession::context() const {
  return context_;
}

NativeApkWindowManagerReport NativeApkWindowManagerSession::BuildReport() const {
  NativeApkWindowManagerReport report;
  report.session_id = context_.session_id;
  report.artifact_root = context_.artifact_root;
  report.report_json_path =
      (fs::path(context_.artifact_root) / "window-state.json").string();
  report.session_map_path =
      (fs::path(context_.artifact_root) / "window-session-map.json").string();
  report.event_log_path =
      (fs::path(context_.artifact_root) / "window-events.jsonl").string();
  report.package_name = context_.package_name;
  report.user_id = context_.user_id;
  report.app_id = context_.app_id;
  report.uid_placeholder = context_.uid_placeholder;
  report.gid_placeholder = context_.gid_placeholder;
  report.sandbox_root = context_.sandbox_root;
  report.app_data_dir = context_.app_data_dir;
  report.apk_path = context_.apk_path;
  report.staged_dir = context_.staged_dir;
  report.process_identity = context_.process_identity;
  report.process_name = context_.process_name.empty() ? context_.package_name
                                                      : context_.process_name;
  report.pid_value = context_.pid_value;
  report.pid_source = context_.pid_source;
  report.launch_component = context_.resolved_component.empty()
                                ? context_.launcher_component
                                : context_.resolved_component;
  report.activity_component = report.launch_component;
  report.lifecycle_state = context_.lifecycle_state;
  report.surface_state = context_.surface_state;
  report.backend = context_.surface_backend.empty() ? "headless"
                                                    : context_.surface_backend;
  report.backing_mode = context_.surface_backing_mode.empty()
                            ? "headless_fallback"
                            : context_.surface_backing_mode;
  report.wayland_surface_available = context_.wayland_surface_available;
  report.egl_surface_available = context_.egl_surface_available;
  report.width = context_.surface_width;
  report.height = context_.surface_height;
  report.format = context_.surface_format;
  report.surface_session_id = context_.surface_session_id;
  report.surface_session_root = context_.surface_session_root;
  report.surface_metadata_path = context_.surface_metadata_path;
  report.surface_event_log_path = context_.surface_event_log_path;
  report.marker_path = context_.surface_marker_path;
  report.updated_at_unix_ms = ComputeDeterministicUnixMs(
      report.package_name, report.user_id, report.app_id,
      report.launch_component);
  report.window_id = report.process_identity + ":" + report.launch_component +
                     ":window";

  const std::string blocking_reason = DetermineWindowBlockingReason(context_);
  report.blocking_reason = blocking_reason;
  report.recommended_recovery_action =
      DetermineWindowRecoveryAction(context_, blocking_reason);
  report.contract_ready = true;
  report.ready = blocking_reason == "none";

  if (report.ready) {
    report.window_state = context_.surface_recovered
                              ? "window_manager_contract_recovered"
                              : "window_manager_contract_ready";
    report.surface_created = true;
    report.attached = true;
    report.visible = true;
    report.hidden = true;
    report.resized = true;
    report.destroyed = true;
    report.failed = false;
    report.recovered = context_.surface_recovered;
    report.states_visited = {"created", "attached", "visible",
                             "resized", "hidden", "destroyed"};
    if (report.recovered) {
      report.states_visited.push_back("recovered");
      AppendUnique(&report.diagnostics,
                   "Self-Healing Android Device window manager contract recovered after surface repair");
    }
    if (report.backing_mode == "headless_fallback") {
      AppendUnique(&report.diagnostics,
                   "Self-Healing Android Device window manager is running in headless-safe mode");
    }
  } else {
    report.window_state = "window_manager_dependency_blocked";
    report.failed = true;
    report.states_visited = {"failed"};
    AppendUnique(&report.diagnostics, "window_manager_dependency_blocked");
    AppendUnique(&report.diagnostics, report.blocking_reason);
  }

  fs::create_directories(context_.artifact_root);
  auto validation = ValidateExistingContract(
      report.report_json_path, report.schema_version, report.package_name,
      report.user_id, report.app_id, report.sandbox_root, report.app_data_dir,
      report.apk_path, report.staged_dir, report.updated_at_unix_ms,
      "window_manager_state_missing", "rebuild_missing_window_manager_state",
      "window_manager_state_malformed",
      "rebuild_malformed_window_manager_state",
      "window_manager_state_incompatible",
      "rebuild_incompatible_window_manager_state",
      "window_manager_state_stale",
      "refresh_stale_window_manager_state");
  const std::string existing_json = ReadTextFile(report.report_json_path);

  bool persisted_state_invalid = false;
  if (validation.state == ContractValidationOutcome::State::kValid &&
      (!WindowManagerContractLooksComplete(existing_json, report) ||
       !fs::exists(report.session_map_path) || !fs::exists(report.event_log_path))) {
    persisted_state_invalid = true;
    validation.state = ContractValidationOutcome::State::kIncomplete;
    AppendUnique(&report.diagnostics, "window_manager_state_incomplete");
    AppendUnique(&report.healing_actions,
                 "rebuild_incomplete_window_manager_state");
    AppendUnique(
        &report.diagnostics,
        "Self-Healing Android Device window manager contract incomplete; rebuilding deterministic state");
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
          "Self-Healing Android Device window manager contract invalid; rebuilding deterministic state");
    }
  }

  if (persisted_state_invalid && !context_.allow_persisted_contract_repair) {
    report.ready = false;
    report.contract_ready = false;
    report.window_state = "window_manager_persisted_state_blocked";
    report.blocking_reason = "persisted_window_manager_state_invalid";
    report.recommended_recovery_action = "rebuild_window_manager_state";
    AppendUnique(&report.errors, report.blocking_reason);
    return report;
  }

  WriteTextFile(report.report_json_path, BuildWindowStateJson(report));
  WriteTextFile(report.session_map_path, BuildWindowSessionMapJson(report));
  WriteTextFile(report.event_log_path, BuildWindowEventLog(report));
  return report;
}

}  // namespace wfa
