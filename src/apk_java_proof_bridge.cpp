#include "wfa/apk_java_proof_bridge.hpp"

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

std::uint64_t ComputeDeterministicUnixMs(const NativeApkJavaProofContext& context) {
  const std::uint64_t hash = ComputeFnv1a64(
      context.package_name + ":" + std::to_string(context.user_id) + ":" +
      std::to_string(context.app_id) + ":" + context.resolved_component +
      ":java-proof");
  return 1740000000000ull + (hash % 1000000000ull);
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
    std::uint64_t expected_updated_at_unix_ms) {
  ContractValidationOutcome outcome;
  if (!fs::exists(path)) {
    outcome.state = ContractValidationOutcome::State::kMissing;
    outcome.diagnostics = {"java_apk_proof_state_missing"};
    outcome.healing_actions = {"rebuild_missing_java_apk_proof_state"};
    return outcome;
  }

  const std::string json = ReadTextFile(path);
  if (json.empty() || !LooksLikeJsonObject(json)) {
    outcome.state = ContractValidationOutcome::State::kMalformed;
    outcome.diagnostics = {"java_apk_proof_state_malformed"};
    outcome.healing_actions = {"rebuild_malformed_java_apk_proof_state"};
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
    outcome.diagnostics = {"java_apk_proof_state_malformed"};
    outcome.healing_actions = {"rebuild_malformed_java_apk_proof_state"};
    return outcome;
  }

  if (*schema_version != expected_schema ||
      *package_name != expected_package_name ||
      *sandbox_root != expected_sandbox_root ||
      *app_data_dir != expected_app_data_dir ||
      *apk_path != expected_apk_path || *staged_dir != expected_staged_dir ||
      *user_id != expected_user_id || *app_id != expected_app_id) {
    outcome.state = ContractValidationOutcome::State::kIncompatible;
    outcome.diagnostics = {"java_apk_proof_state_incompatible"};
    outcome.healing_actions = {"rebuild_incompatible_java_apk_proof_state"};
    return outcome;
  }

  if (static_cast<std::uint64_t>(*updated_at_unix_ms) !=
          expected_updated_at_unix_ms ||
      !*contract_ready) {
    outcome.state = ContractValidationOutcome::State::kStale;
    outcome.diagnostics = {"java_apk_proof_state_stale"};
    outcome.healing_actions = {"rebuild_stale_java_apk_proof_state"};
    return outcome;
  }

  return outcome;
}

bool JavaProofContractLooksComplete(const std::string& json,
                                    const NativeApkJavaProofReport& report) {
  if (!JsonContainsArrayField(json, "states_visited") ||
      !JsonContainsArrayField(json, "healing_actions") ||
      !JsonContainsArrayField(json, "diagnostics") ||
      !JsonContainsArrayField(json, "errors")) {
    return false;
  }

  const auto resolved_component =
      ExtractJsonStringField(json, "resolved_component");
  const auto process_identity =
      ExtractJsonStringField(json, "process_identity");
  const auto window_id = ExtractJsonStringField(json, "window_id");
  const auto runtime_handle = ExtractJsonStringField(json, "runtime_handle");
  const auto proof_state = ExtractJsonStringField(json, "proof_state");
  const auto blocking_reason = ExtractJsonStringField(json, "blocking_reason");
  const auto recommended_recovery_action =
      ExtractJsonStringField(json, "recommended_recovery_action");
  const auto report_json_path = ExtractJsonStringField(json, "report_json_path");
  const auto session_map_path = ExtractJsonStringField(json, "session_map_path");
  const auto event_log_path = ExtractJsonStringField(json, "event_log_path");

  if (!resolved_component || !process_identity || !window_id ||
      !runtime_handle || !proof_state || !blocking_reason ||
      !recommended_recovery_action || !report_json_path || !session_map_path ||
      !event_log_path) {
    return false;
  }

  return *resolved_component == report.resolved_component &&
         *process_identity == report.process_identity &&
         *window_id == report.window_id &&
         *runtime_handle == report.runtime_handle &&
         *proof_state == report.proof_state &&
         *blocking_reason == report.blocking_reason &&
         *recommended_recovery_action ==
             report.recommended_recovery_action &&
         *report_json_path == report.report_json_path &&
         *session_map_path == report.session_map_path &&
         *event_log_path == report.event_log_path;
}

std::string DetermineBlockingReason(const NativeApkJavaProofContext& context) {
  if (!context.launch_ready) {
    return "launch_not_ready_for_java_proof";
  }
  if (!context.storage_ready || !context.sandbox_ready) {
    return "storage_not_ready_for_java_proof";
  }
  if (!context.permission_ready || !context.app_ops_ready) {
    return "permission_not_ready_for_java_proof";
  }
  if (!context.package_manager_ready) {
    return "package_manager_not_ready_for_java_proof";
  }
  if (!context.intent_resolution_ready) {
    return "intent_resolution_not_ready_for_java_proof";
  }
  if (!context.surface_ready) {
    return "surface_not_ready_for_java_proof";
  }
  if (!context.lifecycle_ready) {
    return "lifecycle_not_ready_for_java_proof";
  }
  if (!context.dex_ready || context.dex_files.empty()) {
    return context.dex_files.empty() ? "dex_payload_unavailable_for_java_proof"
                                     : "dex_bootstrap_not_ready_for_java_proof";
  }
  if (!context.art_ready || !context.class_loader_ready) {
    return "dex_bootstrap_not_ready_for_java_proof";
  }
  if (!context.runtime_ready) {
    return "runtime_bridge_not_ready_for_java_proof";
  }
  if (!context.art_runtime_available) {
    return "art_runtime_unavailable_for_java_proof";
  }
  if (context.runtime_bootstrap_state == "failed") {
    return "runtime_bootstrap_failed_for_java_proof";
  }
  if (context.runtime_bootstrap_state != "ready" &&
      context.runtime_bootstrap_state != "recovered") {
    return "runtime_bootstrap_incomplete_for_java_proof";
  }
  if (!context.activity_launch_ready) {
    return "activity_launch_not_ready_for_java_proof";
  }
  if (!context.process_ready) {
    return "process_manager_not_ready_for_java_proof";
  }
  if (!context.window_ready) {
    return "window_manager_not_ready_for_java_proof";
  }
  return "none";
}

std::string DetermineRecoveryAction(const std::string& blocking_reason) {
  if (blocking_reason == "storage_not_ready_for_java_proof") {
    return "repair_app_storage";
  }
  if (blocking_reason == "permission_not_ready_for_java_proof") {
    return "rebuild_permission_state";
  }
  if (blocking_reason == "package_manager_not_ready_for_java_proof" ||
      blocking_reason == "intent_resolution_not_ready_for_java_proof" ||
      blocking_reason == "activity_launch_not_ready_for_java_proof") {
    return "rerun_intent_resolution";
  }
  if (blocking_reason == "process_manager_not_ready_for_java_proof") {
    return "rebuild_process_manager_state";
  }
  if (blocking_reason == "window_manager_not_ready_for_java_proof" ||
      blocking_reason == "surface_not_ready_for_java_proof") {
    return "rebuild_window_manager_state";
  }
  if (blocking_reason == "lifecycle_not_ready_for_java_proof") {
    return "rebuild_lifecycle_controller";
  }
  if (blocking_reason == "dex_payload_unavailable_for_java_proof" ||
      blocking_reason == "dex_bootstrap_not_ready_for_java_proof") {
    return "rebuild_dex_bootstrap";
  }
  if (blocking_reason == "runtime_bridge_not_ready_for_java_proof" ||
      blocking_reason == "art_runtime_unavailable_for_java_proof" ||
      blocking_reason == "runtime_bootstrap_failed_for_java_proof" ||
      blocking_reason == "runtime_bootstrap_incomplete_for_java_proof") {
    return "retry_runtime_bootstrap";
  }
  return "inspect_java_kotlin_apk_proof_diagnostics";
}

std::vector<std::string> BuildStatesVisited(const NativeApkJavaProofContext& context,
                                            const std::string& proof_state) {
  std::vector<std::string> states;
  if (context.package_manager_ready) {
    states.push_back("package_inspected");
  }
  if (context.intent_resolution_ready) {
    states.push_back("intent_resolved");
  }
  if (context.activity_launch_ready) {
    states.push_back("activity_launch_ready");
  }
  if (context.process_ready) {
    states.push_back("process_session_ready");
  }
  if (context.window_ready) {
    states.push_back("window_session_ready");
  }
  if (context.runtime_ready) {
    states.push_back("runtime_contract_ready");
  }
  if (context.art_runtime_available) {
    states.push_back("runtime_discovered");
  }
  if (context.class_loader_ready) {
    states.push_back("class_loader_prepared");
  }
  states.push_back(proof_state == "blocked" ? "proof_blocked"
                                            : (proof_state == "recovered"
                                                   ? "proof_recovered"
                                                   : "proof_ready"));
  return states;
}

std::string RenderJavaProofJson(const NativeApkJavaProofReport& report) {
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
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir)
         << "\",\n"
         << "  \"updated_at_unix_ms\": " << report.updated_at_unix_ms << ",\n"
         << "  \"proof_mode\": \"" << EscapeJson(report.proof_mode)
         << "\",\n"
         << "  \"launcher_component\": \""
         << EscapeJson(report.launcher_component) << "\",\n"
         << "  \"resolved_component\": \""
         << EscapeJson(report.resolved_component) << "\",\n"
         << "  \"process_session_id\": \""
         << EscapeJson(report.process_session_id) << "\",\n"
         << "  \"process_identity\": \""
         << EscapeJson(report.process_identity) << "\",\n"
         << "  \"process_name\": \"" << EscapeJson(report.process_name)
         << "\",\n"
         << "  \"pid_value\": " << report.pid_value << ",\n"
         << "  \"pid_source\": \"" << EscapeJson(report.pid_source)
         << "\",\n"
         << "  \"window_session_id\": \""
         << EscapeJson(report.window_session_id) << "\",\n"
         << "  \"window_id\": \"" << EscapeJson(report.window_id) << "\",\n"
         << "  \"runtime_session_id\": \""
         << EscapeJson(report.runtime_session_id) << "\",\n"
         << "  \"runtime_handle\": \"" << EscapeJson(report.runtime_handle)
         << "\",\n"
         << "  \"runtime_root\": \"" << EscapeJson(report.runtime_root)
         << "\",\n"
         << "  \"runtime_discovery_source\": \""
         << EscapeJson(report.runtime_discovery_source) << "\",\n"
         << "  \"runtime_bootstrap_state\": \""
         << EscapeJson(report.runtime_bootstrap_state) << "\",\n"
         << "  \"assets_count\": " << report.assets_count << ",\n"
         << "  \"resource_table_present\": "
         << (report.resource_table_present ? "true" : "false") << ",\n"
         << "  \"dex_files_count\": " << report.dex_files_count << ",\n"
         << "  \"package_manager_ready\": "
         << (report.package_manager_ready ? "true" : "false") << ",\n"
         << "  \"intent_resolution_ready\": "
         << (report.intent_resolution_ready ? "true" : "false") << ",\n"
         << "  \"activity_launch_ready\": "
         << (report.activity_launch_ready ? "true" : "false") << ",\n"
         << "  \"process_ready\": " << (report.process_ready ? "true" : "false")
         << ",\n"
         << "  \"window_ready\": " << (report.window_ready ? "true" : "false")
         << ",\n"
         << "  \"runtime_ready\": " << (report.runtime_ready ? "true" : "false")
         << ",\n"
         << "  \"art_runtime_available\": "
         << (report.art_runtime_available ? "true" : "false") << ",\n"
         << "  \"class_loader_ready\": "
         << (report.class_loader_ready ? "true" : "false") << ",\n"
         << "  \"bytecode_execution_ready\": "
         << (report.bytecode_execution_ready ? "true" : "false") << ",\n"
         << "  \"java_execution_supported\": "
         << (report.java_execution_supported ? "true" : "false") << ",\n"
         << "  \"self_healing_requested\": "
         << (report.self_healing_requested ? "true" : "false") << ",\n"
         << "  \"self_healing_ready\": "
         << (report.self_healing_ready ? "true" : "false") << ",\n"
         << "  \"self_healing_final_health\": \""
         << EscapeJson(report.self_healing_final_health) << "\",\n"
         << "  \"proof_state\": \"" << EscapeJson(report.proof_state)
         << "\",\n"
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

void WriteJavaProofSessionMap(const NativeApkJavaProofReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"resolved_component\": \""
         << EscapeJson(report.resolved_component) << "\",\n"
         << "  \"process_session_id\": \""
         << EscapeJson(report.process_session_id) << "\",\n"
         << "  \"window_session_id\": \""
         << EscapeJson(report.window_session_id) << "\",\n"
         << "  \"runtime_session_id\": \""
         << EscapeJson(report.runtime_session_id) << "\",\n"
         << "  \"process_identity\": \""
         << EscapeJson(report.process_identity) << "\",\n"
         << "  \"window_id\": \"" << EscapeJson(report.window_id) << "\",\n"
         << "  \"runtime_handle\": \"" << EscapeJson(report.runtime_handle)
         << "\"\n"
         << "}\n";
  WriteTextFile(report.session_map_path, output.str());
}

void WriteJavaProofEventLog(const NativeApkJavaProofReport& report) {
  std::ostringstream output;
  for (const auto& state : report.states_visited) {
    output << "{\"state\": \"" << EscapeJson(state) << "\"}\n";
  }
  WriteTextFile(report.event_log_path, output.str());
}

}  // namespace

NativeApkJavaProofSession::NativeApkJavaProofSession(
    NativeApkJavaProofContext context)
    : context_(std::move(context)) {}

const NativeApkJavaProofContext& NativeApkJavaProofSession::context() const {
  return context_;
}

NativeApkJavaProofReport NativeApkJavaProofSession::BuildReport() const {
  NativeApkJavaProofReport report;
  report.session_id = context_.session_id;
  report.artifact_root = context_.artifact_root;
  report.report_json_path =
      (fs::path(report.artifact_root) / "java-proof-state.json").string();
  report.session_map_path =
      (fs::path(report.artifact_root) / "java-proof-session-map.json").string();
  report.event_log_path =
      (fs::path(report.artifact_root) / "java-proof-events.jsonl").string();
  report.package_name = context_.package_name;
  report.user_id = context_.user_id;
  report.app_id = context_.app_id;
  report.uid_placeholder = context_.uid_placeholder;
  report.gid_placeholder = context_.gid_placeholder;
  report.sandbox_root = context_.sandbox_root;
  report.app_data_dir = context_.app_data_dir;
  report.apk_path = context_.apk_path;
  report.staged_dir = context_.staged_dir;
  report.updated_at_unix_ms = ComputeDeterministicUnixMs(context_);
  report.launcher_component = context_.launcher_component;
  report.resolved_component = context_.resolved_component;
  report.process_session_id = context_.process_session_id;
  report.process_identity = context_.process_identity;
  report.process_name = context_.process_name;
  report.pid_value = context_.pid_value;
  report.pid_source = context_.pid_source;
  report.window_session_id = context_.window_session_id;
  report.window_id = context_.window_id;
  report.runtime_session_id = context_.runtime_session_id;
  report.runtime_handle = context_.runtime_handle;
  report.runtime_root = context_.runtime_root;
  report.runtime_discovery_source = context_.runtime_discovery_source;
  report.runtime_bootstrap_state = context_.runtime_bootstrap_state;
  report.assets_count = context_.assets_count;
  report.resource_table_present = context_.resource_table_present;
  report.dex_files_count = static_cast<int>(context_.dex_files.size());
  report.package_manager_ready = context_.package_manager_ready;
  report.intent_resolution_ready = context_.intent_resolution_ready;
  report.activity_launch_ready = context_.activity_launch_ready;
  report.process_ready = context_.process_ready;
  report.window_ready = context_.window_ready;
  report.runtime_ready = context_.runtime_ready;
  report.art_runtime_available = context_.art_runtime_available;
  report.class_loader_ready = context_.class_loader_ready;
  report.bytecode_execution_ready = context_.bytecode_execution_ready;
  report.java_execution_supported = context_.java_execution_supported;
  report.self_healing_requested = context_.self_healing_requested;
  report.self_healing_ready = context_.self_healing_ready;
  report.self_healing_final_health = context_.self_healing_final_health;

  fs::create_directories(report.artifact_root);

  auto validation = ValidateExistingContract(
      report.report_json_path, report.schema_version, report.package_name,
      report.user_id, report.app_id, report.sandbox_root, report.app_data_dir,
      report.apk_path, report.staged_dir, report.updated_at_unix_ms);

  const std::string existing_json = ReadTextFile(report.report_json_path);
  bool persisted_state_invalid = false;
  if (validation.state == ContractValidationOutcome::State::kValid &&
      (!JavaProofContractLooksComplete(existing_json, report) ||
       !fs::exists(report.session_map_path) || !fs::exists(report.event_log_path))) {
    persisted_state_invalid = true;
    validation.state = ContractValidationOutcome::State::kIncomplete;
    AppendUnique(&report.diagnostics, "java_apk_proof_state_incomplete");
    AppendUnique(&report.healing_actions,
                 "rebuild_incomplete_java_apk_proof_state");
    AppendUnique(&report.diagnostics,
                 "Self-Healing Android Device Java/Kotlin proof contract incomplete; rebuilding deterministic state");
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
      AppendUnique(&report.diagnostics,
                   "Self-Healing Android Device Java/Kotlin proof contract invalid; rebuilding deterministic state");
    }
  }

  report.blocking_reason = DetermineBlockingReason(context_);
  report.recommended_recovery_action =
      DetermineRecoveryAction(report.blocking_reason);

  bool base_ready = report.blocking_reason == "none";
  report.proof_state = base_ready ? "ready" : "blocked";
  if (!base_ready && report.self_healing_requested && report.self_healing_ready &&
      (report.self_healing_final_health == "recovered" ||
       report.self_healing_final_health == "healthy")) {
    report.proof_state =
        report.self_healing_final_health == "recovered" ? "recovered" : "ready";
    report.blocking_reason = "none";
    report.recommended_recovery_action = "none";
    base_ready = true;
    AppendUnique(&report.diagnostics,
                 "Self-Healing Android Device Java/Kotlin APK proof recovered after watchdog replay");
  } else if (!base_ready) {
    AppendUnique(&report.diagnostics,
                 "Self-Healing Android Device recommends " +
                     report.recommended_recovery_action +
                     " for the current Java/Kotlin APK proof state");
  }

  report.states_visited = BuildStatesVisited(context_, report.proof_state);

  if (persisted_state_invalid && !context_.allow_persisted_contract_repair) {
    report.ready = false;
    report.contract_ready = false;
    report.proof_state = "blocked";
    report.blocking_reason = "persisted_java_apk_proof_state_invalid";
    report.recommended_recovery_action = "rebuild_java_apk_proof_state";
    AppendUnique(&report.errors, report.blocking_reason);
    return report;
  }

  WriteJavaProofSessionMap(report);
  WriteJavaProofEventLog(report);
  WriteTextFile(report.report_json_path, RenderJavaProofJson(report));

  report.contract_ready = true;
  report.ready = base_ready;
  if (!report.ready) {
    AppendUnique(&report.errors,
                 "java_apk_proof_blocked:" + report.blocking_reason);
  }

  return report;
}

}  // namespace wfa
