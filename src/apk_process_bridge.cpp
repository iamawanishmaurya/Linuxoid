#include "wfa/apk_process_bridge.hpp"

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
  return 1710000000000ull + (hash % 1000000000ull);
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

bool ActivityManagerContractLooksComplete(
    const std::string& json, const NativeApkActivityManagerReport& report) {
  if (!JsonContainsArrayField(json, "intent_categories") ||
      !JsonContainsArrayField(json, "healing_actions") ||
      !JsonContainsArrayField(json, "diagnostics") ||
      !JsonContainsArrayField(json, "errors")) {
    return false;
  }

  const auto resolved_component =
      ExtractJsonStringField(json, "resolved_component");
  const auto process_name = ExtractJsonStringField(json, "process_name");
  const auto start_reason = ExtractJsonStringField(json, "start_reason");
  const auto blocking_reason = ExtractJsonStringField(json, "blocking_reason");
  const auto recommended_recovery_action =
      ExtractJsonStringField(json, "recommended_recovery_action");
  if (!resolved_component || !process_name || !start_reason ||
      !blocking_reason || !recommended_recovery_action) {
    return false;
  }
  return *resolved_component == report.resolved_component &&
         *process_name == report.process_name &&
         *start_reason == report.start_reason &&
         *blocking_reason == report.blocking_reason &&
         *recommended_recovery_action == report.recommended_recovery_action;
}

bool ProcessManagerContractLooksComplete(
    const std::string& json, const NativeApkProcessManagerReport& report) {
  if (!JsonContainsArrayField(json, "intent_categories") ||
      !JsonContainsArrayField(json, "dependency_details") ||
      !JsonContainsArrayField(json, "healing_actions") ||
      !JsonContainsArrayField(json, "diagnostics") ||
      !JsonContainsArrayField(json, "errors")) {
    return false;
  }

  const auto process_identity =
      ExtractJsonStringField(json, "process_identity");
  const auto process_name = ExtractJsonStringField(json, "process_name");
  const auto pid_source = ExtractJsonStringField(json, "pid_source");
  const auto launch_component =
      ExtractJsonStringField(json, "launch_component");
  const auto blocking_reason = ExtractJsonStringField(json, "blocking_reason");
  const auto recommended_recovery_action =
      ExtractJsonStringField(json, "recommended_recovery_action");
  if (!process_identity || !process_name || !pid_source || !launch_component ||
      !blocking_reason || !recommended_recovery_action) {
    return false;
  }
  return *process_identity == report.process_identity &&
         *process_name == report.process_name &&
         *pid_source == report.pid_source &&
         *launch_component == report.launch_component &&
         *blocking_reason == report.blocking_reason &&
         *recommended_recovery_action == report.recommended_recovery_action;
}

std::string DetermineStartReason(
    const NativeApkProcessManagerContext& context) {
  return context.requested_component.empty() ? "launcher_intent"
                                             : "explicit_component";
}

std::string DetermineProcessBlockingReason(
    const NativeApkProcessManagerContext& context) {
  if (!context.launch_ready) {
    return "launch_not_ready";
  }
  if (!context.package_manager_ready) {
    return "package_manager_not_ready";
  }
  if (!context.intent_resolution_ready) {
    return context.resolution_blocking_reason.empty()
               ? "intent_resolution_not_ready"
               : context.resolution_blocking_reason;
  }
  if (!context.activity_launch_ready) {
    return context.activity_launch_blocking_reason.empty()
               ? "activity_launch_not_ready"
               : context.activity_launch_blocking_reason;
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
  if (context.surface_health != "ready") {
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
  if (context.resolved_component.empty()) {
    return "launch_component_not_resolved";
  }
  return "none";
}

std::string DetermineProcessRecoveryAction(
    const NativeApkProcessManagerContext& context,
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
  if (blocking_reason == "surface_not_ready") {
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
  if (blocking_reason == "package_manager_not_ready" ||
      blocking_reason == "intent_resolution_not_ready" ||
      blocking_reason == "launch_component_not_resolved") {
    return "rerun_intent_resolution";
  }
  if (blocking_reason == "launch_not_ready") {
    return "safe_mode_launch";
  }
  if (context.activity_launch_recovery_action != "none") {
    return context.activity_launch_recovery_action;
  }
  if (context.resolution_recovery_action != "none") {
    return context.resolution_recovery_action;
  }
  return "rebuild_process_manager_state";
}

bool DependenciesReady(const NativeApkProcessManagerContext& context) {
  return DetermineProcessBlockingReason(context) == "none";
}

std::string BuildActivityManagerJson(
    const NativeApkActivityManagerReport& report) {
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
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"user_id\": " << report.user_id << ",\n"
         << "  \"app_id\": " << report.app_id << ",\n"
         << "  \"sandbox_root\": \"" << EscapeJson(report.sandbox_root)
         << "\",\n"
         << "  \"app_data_dir\": \"" << EscapeJson(report.app_data_dir)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir) << "\",\n"
         << "  \"updated_at_unix_ms\": " << report.updated_at_unix_ms
         << ",\n"
         << "  \"requested_package_name\": \""
         << EscapeJson(report.requested_package_name) << "\",\n"
         << "  \"requested_component\": \""
         << EscapeJson(report.requested_component) << "\",\n"
         << "  \"resolved_component\": \""
         << EscapeJson(report.resolved_component) << "\",\n"
         << "  \"launch_component\": \""
         << EscapeJson(report.launch_component) << "\",\n"
         << "  \"process_name\": \"" << EscapeJson(report.process_name)
         << "\",\n"
         << "  \"start_reason\": \"" << EscapeJson(report.start_reason)
         << "\",\n"
         << "  \"resolution_mode\": \"" << EscapeJson(report.resolution_mode)
         << "\",\n"
         << "  \"intent_action\": \"" << EscapeJson(report.intent_action)
         << "\",\n"
         << "  \"intent_categories\": "
         << RenderJsonArray(report.intent_categories) << ",\n"
         << "  \"resolution_status\": \""
         << EscapeJson(report.resolution_status) << "\",\n"
         << "  \"resolution_reason\": \""
         << EscapeJson(report.resolution_reason) << "\",\n"
         << "  \"launch_state\": \"" << EscapeJson(report.launch_state)
         << "\",\n"
         << "  \"lifecycle_state\": \"" << EscapeJson(report.lifecycle_state)
         << "\",\n"
         << "  \"blocking_reason\": \"" << EscapeJson(report.blocking_reason)
         << "\",\n"
         << "  \"recommended_recovery_action\": \""
         << EscapeJson(report.recommended_recovery_action) << "\",\n"
         << "  \"restart_policy\": \"" << EscapeJson(report.restart_policy)
         << "\",\n"
         << "  \"termination_policy\": \""
         << EscapeJson(report.termination_policy) << "\",\n"
         << "  \"storage_health\": \"" << EscapeJson(report.storage_health)
         << "\",\n"
         << "  \"sandbox_health\": \"" << EscapeJson(report.sandbox_health)
         << "\",\n"
         << "  \"permission_health\": \""
         << EscapeJson(report.permission_health) << "\",\n"
         << "  \"app_ops_health\": \"" << EscapeJson(report.app_ops_health)
         << "\",\n"
         << "  \"binder_health\": \"" << EscapeJson(report.binder_health)
         << "\",\n"
         << "  \"surface_health\": \"" << EscapeJson(report.surface_health)
         << "\",\n"
         << "  \"lifecycle_health\": \""
         << EscapeJson(report.lifecycle_health) << "\",\n"
         << "  \"looper_health\": \"" << EscapeJson(report.looper_health)
         << "\",\n"
         << "  \"input_health\": \"" << EscapeJson(report.input_health)
         << "\",\n"
         << "  \"dex_health\": \"" << EscapeJson(report.dex_health) << "\",\n"
         << "  \"art_health\": \"" << EscapeJson(report.art_health) << "\",\n"
         << "  \"healing_actions\": "
         << RenderJsonArray(report.healing_actions) << ",\n"
         << "  \"diagnostics\": " << RenderJsonArray(report.diagnostics)
         << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

std::string BuildProcessManagerJson(
    const NativeApkProcessManagerReport& report) {
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
         << "  \"process_identity\": \""
         << EscapeJson(report.process_identity) << "\",\n"
         << "  \"process_name\": \"" << EscapeJson(report.process_name)
         << "\",\n"
         << "  \"pid_value\": " << report.pid_value << ",\n"
         << "  \"pid_source\": \"" << EscapeJson(report.pid_source)
         << "\",\n"
         << "  \"launch_component\": \""
         << EscapeJson(report.launch_component) << "\",\n"
         << "  \"start_reason\": \"" << EscapeJson(report.start_reason)
         << "\",\n"
         << "  \"process_state\": \"" << EscapeJson(report.process_state)
         << "\",\n"
         << "  \"lifecycle_state\": \"" << EscapeJson(report.lifecycle_state)
         << "\",\n"
         << "  \"restart_policy\": \"" << EscapeJson(report.restart_policy)
         << "\",\n"
         << "  \"termination_policy\": \""
         << EscapeJson(report.termination_policy) << "\",\n"
         << "  \"intent_action\": \"" << EscapeJson(report.intent_action)
         << "\",\n"
         << "  \"intent_categories\": "
         << RenderJsonArray(report.intent_categories) << ",\n"
         << "  \"blocking_reason\": \"" << EscapeJson(report.blocking_reason)
         << "\",\n"
         << "  \"recommended_recovery_action\": \""
         << EscapeJson(report.recommended_recovery_action) << "\",\n"
         << "  \"dependency_details\": "
         << RenderJsonArray(report.dependency_details) << ",\n"
         << "  \"healing_actions\": "
         << RenderJsonArray(report.healing_actions) << ",\n"
         << "  \"diagnostics\": " << RenderJsonArray(report.diagnostics)
         << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

}  // namespace

NativeApkProcessManagerSession::NativeApkProcessManagerSession(
    NativeApkProcessManagerContext context)
    : context_(std::move(context)) {}

const NativeApkProcessManagerContext&
NativeApkProcessManagerSession::context() const {
  return context_;
}

NativeApkActivityManagerReport
NativeApkProcessManagerSession::BuildActivityManagerReport() const {
  NativeApkActivityManagerReport report;
  report.session_id = context_.session_id;
  report.artifact_root = context_.artifact_root;
  report.report_json_path =
      (fs::path(context_.artifact_root) / "activity-manager-state.json")
          .string();
  report.package_name = context_.package_name;
  report.user_id = context_.user_id;
  report.app_id = context_.app_id;
  report.sandbox_root = context_.sandbox_root;
  report.app_data_dir = context_.app_data_dir;
  report.apk_path = context_.apk_path;
  report.staged_dir = context_.staged_dir;
  report.requested_package_name = context_.requested_package_name;
  report.requested_component = context_.requested_component;
  report.resolved_component = context_.resolved_component.empty()
                                  ? context_.launcher_component
                                  : context_.resolved_component;
  report.launch_component = report.resolved_component;
  report.process_name = context_.package_name;
  report.start_reason = DetermineStartReason(context_);
  report.resolution_mode = context_.resolution_mode;
  report.intent_action = context_.intent_action;
  report.intent_categories = context_.intent_categories;
  report.resolution_status = context_.resolution_status;
  report.resolution_reason = context_.resolution_reason;
  report.lifecycle_state = context_.lifecycle_state;
  report.storage_health = context_.storage_health;
  report.sandbox_health = context_.sandbox_health;
  report.permission_health = context_.permission_health;
  report.app_ops_health = context_.app_ops_health;
  report.binder_health = context_.binder_health;
  report.surface_health = context_.surface_health;
  report.lifecycle_health = context_.lifecycle_health;
  report.looper_health = context_.looper_health;
  report.input_health = context_.input_health;
  report.dex_health = context_.dex_health;
  report.art_health = context_.art_health;
  report.updated_at_unix_ms = ComputeDeterministicUnixMs(
      report.package_name, report.user_id, report.app_id,
      report.launch_component);

  const std::string blocking_reason = DetermineProcessBlockingReason(context_);
  report.blocking_reason = blocking_reason;
  report.recommended_recovery_action =
      blocking_reason == "none"
          ? "none"
          : DetermineProcessRecoveryAction(context_, blocking_reason);
  report.ready = blocking_reason == "none";
  report.contract_ready = true;
  report.launch_state = report.ready ? "activity_manager_contract_ready"
                                     : "activity_manager_dependency_blocked";
  if (!report.ready) {
    AppendUnique(&report.diagnostics, "activity_manager_dependency_blocked");
    AppendUnique(&report.diagnostics, blocking_reason);
  }

  fs::create_directories(context_.artifact_root);
  const auto validation = ValidateExistingContract(
      report.report_json_path, report.schema_version, report.package_name,
      report.user_id, report.app_id, report.sandbox_root, report.app_data_dir,
      report.apk_path, report.staged_dir, report.updated_at_unix_ms,
      "activity_manager_state_missing",
      "rebuild_missing_activity_manager_state",
      "activity_manager_state_malformed",
      "rebuild_malformed_activity_manager_state",
      "activity_manager_state_incompatible",
      "rebuild_incompatible_activity_manager_state",
      "activity_manager_state_stale",
      "refresh_stale_activity_manager_state");
  const std::string existing_json = ReadTextFile(report.report_json_path);

  bool persisted_state_invalid = false;
  if (validation.state == ContractValidationOutcome::State::kValid &&
      !existing_json.empty() &&
      !ActivityManagerContractLooksComplete(existing_json, report)) {
    persisted_state_invalid = true;
    AppendUnique(&report.diagnostics, "activity_manager_state_incomplete");
    AppendUnique(&report.healing_actions,
                 "rebuild_incomplete_activity_manager_state");
    AppendUnique(
        &report.diagnostics,
        "Self-Healing Android Device activity manager contract incomplete; rebuilding deterministic state");
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
          "Self-Healing Android Device activity manager contract invalid; rebuilding deterministic state");
    }
  }

  if (persisted_state_invalid && !context_.allow_persisted_contract_repair) {
    report.ready = false;
    report.contract_ready = false;
    report.launch_state = "activity_manager_persisted_state_blocked";
    report.blocking_reason = "persisted_activity_manager_state_invalid";
    report.recommended_recovery_action = "rebuild_process_manager_state";
    AppendUnique(&report.errors, report.blocking_reason);
    return report;
  }

  WriteTextFile(report.report_json_path, BuildActivityManagerJson(report));
  return report;
}

NativeApkProcessManagerReport
NativeApkProcessManagerSession::BuildProcessManagerReport(
    const NativeApkActivityManagerReport& activity_manager) const {
  NativeApkProcessManagerReport report;
  report.session_id = context_.session_id;
  report.artifact_root = context_.artifact_root;
  report.report_json_path =
      (fs::path(context_.artifact_root) / "process-state.json").string();
  report.package_name = context_.package_name;
  report.user_id = context_.user_id;
  report.app_id = context_.app_id;
  report.uid_placeholder = context_.uid_placeholder;
  report.gid_placeholder = context_.gid_placeholder;
  report.sandbox_root = context_.sandbox_root;
  report.app_data_dir = context_.app_data_dir;
  report.apk_path = context_.apk_path;
  report.staged_dir = context_.staged_dir;
  report.process_name = context_.package_name;
  report.process_identity = "u" + std::to_string(context_.user_id) + "a" +
                            std::to_string(context_.app_id) + ":" +
                            report.process_name;
  report.launch_component = activity_manager.launch_component;
  report.start_reason = activity_manager.start_reason;
  report.lifecycle_state = context_.lifecycle_state;
  report.intent_action = context_.intent_action;
  report.intent_categories = context_.intent_categories;
  report.updated_at_unix_ms = ComputeDeterministicUnixMs(
      report.package_name, report.user_id, report.app_id,
      report.launch_component);

  report.dependency_details = {
      "launch_status=" + context_.launch_status,
      "activity_launch_status=" + context_.activity_launch_status,
      "resolution_status=" + context_.resolution_status,
      "storage_health=" + context_.storage_health,
      "sandbox_health=" + context_.sandbox_health,
      "permission_health=" + context_.permission_health,
      "app_ops_health=" + context_.app_ops_health,
      "binder_health=" + context_.binder_health,
      "surface_health=" + context_.surface_health,
      "lifecycle_health=" + context_.lifecycle_health,
      "looper_health=" + context_.looper_health,
      "input_health=" + context_.input_health,
      "dex_health=" + context_.dex_health,
      "art_health=" + context_.art_health,
      "activity_manager_ready=" +
          std::string(activity_manager.ready ? "true" : "false")};

  report.blocking_reason = activity_manager.ready
                               ? "none"
                               : activity_manager.blocking_reason;
  report.recommended_recovery_action =
      activity_manager.ready ? "none"
                             : activity_manager.recommended_recovery_action;
  report.ready = activity_manager.ready;
  report.contract_ready = true;
  report.process_state = report.ready ? "process_manager_contract_ready"
                                      : "process_manager_dependency_blocked";
  if (!report.ready) {
    AppendUnique(&report.diagnostics, "process_manager_dependency_blocked");
    AppendUnique(&report.diagnostics, report.blocking_reason);
  }

  fs::create_directories(context_.artifact_root);
  const auto validation = ValidateExistingContract(
      report.report_json_path, report.schema_version, report.package_name,
      report.user_id, report.app_id, report.sandbox_root, report.app_data_dir,
      report.apk_path, report.staged_dir, report.updated_at_unix_ms,
      "process_manager_state_missing", "rebuild_missing_process_manager_state",
      "process_manager_state_malformed",
      "rebuild_malformed_process_manager_state",
      "process_manager_state_incompatible",
      "rebuild_incompatible_process_manager_state",
      "process_manager_state_stale",
      "refresh_stale_process_manager_state");
  const std::string existing_json = ReadTextFile(report.report_json_path);

  bool persisted_state_invalid = false;
  if (validation.state == ContractValidationOutcome::State::kValid &&
      !existing_json.empty() &&
      !ProcessManagerContractLooksComplete(existing_json, report)) {
    persisted_state_invalid = true;
    AppendUnique(&report.diagnostics, "process_manager_state_incomplete");
    AppendUnique(&report.healing_actions,
                 "rebuild_incomplete_process_manager_state");
    AppendUnique(
        &report.diagnostics,
        "Self-Healing Android Device process manager contract incomplete; rebuilding deterministic state");
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
          "Self-Healing Android Device process manager contract invalid; rebuilding deterministic state");
    }
  }

  if (persisted_state_invalid && !context_.allow_persisted_contract_repair) {
    report.ready = false;
    report.contract_ready = false;
    report.process_state = "process_manager_persisted_state_blocked";
    report.blocking_reason = "persisted_process_manager_state_invalid";
    report.recommended_recovery_action = "rebuild_process_manager_state";
    AppendUnique(&report.errors, report.blocking_reason);
    return report;
  }

  WriteTextFile(report.report_json_path, BuildProcessManagerJson(report));
  return report;
}

}  // namespace wfa
