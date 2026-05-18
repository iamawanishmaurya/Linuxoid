#include "wfa/apk_permission_bridge.hpp"

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
    kIncomplete
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
  return trimmed.size() >= 2 && trimmed.front() == '{' && trimmed.back() == '}';
}

void AppendUnique(std::vector<std::string>* values, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(value);
  }
}

void AppendError(std::vector<std::string>* errors, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(errors->begin(), errors->end(), value) == errors->end()) {
    errors->push_back(value);
  }
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
                                         int user_id, int app_id) {
  const std::uint64_t hash =
      ComputeFnv1a64(package_name + ":" + std::to_string(user_id) + ":" +
                     std::to_string(app_id));
  return 1700000000000ull + (hash % 1000000000ull);
}

std::vector<std::string> ParseRequestedPermissions(std::string_view xml) {
  std::vector<std::string> permissions;
  const std::regex permission_pattern(
      "<uses-permission[^>]*android:name=\"([^\"]+)\"");
  const char* begin = xml.data();
  const char* end = xml.data() + xml.size();
  for (std::cregex_iterator it(begin, end, permission_pattern), end_it;
       it != end_it; ++it) {
    AppendUnique(&permissions, (*it)[1].str());
  }
  std::sort(permissions.begin(), permissions.end());
  return permissions;
}

struct PermissionDecision {
  std::string grant_state;
  std::string protection_level;
  std::string source;
  std::string rationale;
};

PermissionDecision DeterminePermissionDecision(
    const std::string& permission_name) {
  if (permission_name == "android.permission.INTERNET") {
    return {.grant_state = "granted",
            .protection_level = "normal_placeholder",
            .source = "linuxoid_normal_permission_autogrant",
            .rationale =
                "normal placeholder permission granted by local Linuxoid policy"};
  }
  if (permission_name == "android.permission.WRITE_EXTERNAL_STORAGE" ||
      permission_name == "android.permission.READ_EXTERNAL_STORAGE") {
    return {.grant_state = "granted",
            .protection_level = "dangerous_placeholder",
            .source = "linuxoid_storage_policy_grant",
            .rationale =
                "storage-sensitive placeholder permission granted by explicit local policy"};
  }
  if (permission_name == "android.permission.RECORD_AUDIO") {
    return {.grant_state = "denied",
            .protection_level = "dangerous_placeholder",
            .source = "linuxoid_dangerous_permission_default_deny",
            .rationale =
                "dangerous placeholder permission denied without explicit grant state"};
  }
  return {.grant_state = "denied",
          .protection_level = "placeholder",
          .source = "linuxoid_unknown_permission_default_deny",
          .rationale =
              "unsupported permission denied by the current local contract"};
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
  return std::regex_search(
      json, std::regex("\"" + field + "\"\\s*:\\s*\\["));
}

std::size_t CountJsonFieldOccurrences(const std::string& json,
                                      const std::string& field) {
  std::size_t count = 0;
  const std::regex pattern("\"" + field + "\"\\s*:");
  for (std::sregex_iterator it(json.begin(), json.end(), pattern),
       end_it;
       it != end_it; ++it) {
    ++count;
  }
  return count;
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

  if (*schema_version != expected_schema || *package_name != expected_package_name ||
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

bool PermissionContractLooksComplete(const std::string& json,
                                     const NativeApkPermissionsReport& report) {
  if (!JsonContainsArrayField(json, "requested_permissions") ||
      !JsonContainsArrayField(json, "granted_permissions") ||
      !JsonContainsArrayField(json, "denied_permissions") ||
      !JsonContainsArrayField(json, "permission_records") ||
      !JsonContainsArrayField(json, "healing_actions") ||
      !JsonContainsArrayField(json, "diagnostics") ||
      !JsonContainsArrayField(json, "errors")) {
    return false;
  }

  const auto decode_level = ExtractJsonStringField(json, "decode_level");
  if (!decode_level || *decode_level != report.decode_level) {
    return false;
  }

  if (CountJsonFieldOccurrences(json, "permission_name") !=
      report.permission_records.size()) {
    return false;
  }

  for (const auto& requested_permission : report.requested_permissions) {
    if (json.find("\"" + requested_permission + "\"") == std::string::npos) {
      return false;
    }
  }

  return true;
}

bool AppOpsContractLooksComplete(const std::string& json,
                                 const NativeApkAppOpsReport& report) {
  if (!JsonContainsArrayField(json, "allowed_operations") ||
      !JsonContainsArrayField(json, "denied_operations") ||
      !JsonContainsArrayField(json, "default_operations") ||
      !JsonContainsArrayField(json, "ignored_placeholder_operations") ||
      !JsonContainsArrayField(json, "app_ops") ||
      !JsonContainsArrayField(json, "healing_actions") ||
      !JsonContainsArrayField(json, "diagnostics") ||
      !JsonContainsArrayField(json, "errors")) {
    return false;
  }

  const auto operations_count = ExtractJsonIntegerField(json, "operations_count");
  if (!operations_count ||
      static_cast<std::size_t>(*operations_count) != report.operations_count) {
    return false;
  }

  if (CountJsonFieldOccurrences(json, "op_name") != report.operation_records.size()) {
    return false;
  }

  for (const auto& operation : report.operation_records) {
    if (json.find("\"op_name\": \"" + operation.operation_name + "\"") ==
        std::string::npos) {
      return false;
    }
  }

  return true;
}

std::string RenderPermissionRecordsArray(
    const std::vector<NativeApkPermissionRecord>& records) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& record = records[index];
    output << "{"
           << "\"permission_name\": \""
           << EscapeJson(record.permission_name) << "\", "
           << "\"grant_state\": \"" << EscapeJson(record.grant_state)
           << "\", "
           << "\"protection_level\": \""
           << EscapeJson(record.protection_level) << "\", "
           << "\"source\": \"" << EscapeJson(record.source) << "\", "
           << "\"rationale\": \"" << EscapeJson(record.rationale) << "\", "
           << "\"updated_at_unix_ms\": " << record.updated_at_unix_ms << "}";
  }
  output << "]";
  return output.str();
}

std::string RenderAppOpRecordsArray(
    const std::vector<NativeApkAppOpRecord>& records) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& record = records[index];
    output << "{"
           << "\"op_name\": \""
           << EscapeJson(record.operation_name) << "\", "
           << "\"mode\": \"" << EscapeJson(record.mode) << "\", "
           << "\"required_permission\": \""
           << EscapeJson(record.required_permission) << "\", "
           << "\"reason\": \"" << EscapeJson(record.reason) << "\", "
           << "\"source\": \"" << EscapeJson(record.source) << "\", "
           << "\"updated_at_unix_ms\": " << record.updated_at_unix_ms << "}";
  }
  output << "]";
  return output.str();
}

std::string RenderPermissionsReportJson(
    const NativeApkPermissionsReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"schema_version\": \"" << EscapeJson(report.schema_version)
         << "\",\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"contract_ready\": "
         << (report.contract_ready ? "true" : "false") << ",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id)
         << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"report_json_path\": \"" << EscapeJson(report.report_json_path)
         << "\",\n"
         << "  \"persisted_state_preexisting\": "
         << (report.persisted_state_preexisting ? "true" : "false") << ",\n"
         << "  \"continuity_validated\": "
         << (report.continuity_validated ? "true" : "false") << ",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"user_id\": " << report.user_id << ",\n"
         << "  \"app_id\": " << report.app_id << ",\n"
         << "  \"sandbox_root\": \"" << EscapeJson(report.sandbox_root)
         << "\",\n"
         << "  \"app_data_dir\": \"" << EscapeJson(report.app_data_dir)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir)
         << "\",\n"
         << "  \"updated_at_unix_ms\": " << report.updated_at_unix_ms
         << ",\n"
         << "  \"continuity_state\": \""
         << EscapeJson(report.continuity_state) << "\",\n"
         << "  \"requested_permissions\": "
         << RenderJsonArray(report.requested_permissions) << ",\n"
         << "  \"granted_permissions\": "
         << RenderJsonArray(report.granted_permissions) << ",\n"
         << "  \"denied_permissions\": "
         << RenderJsonArray(report.denied_permissions) << ",\n"
         << "  \"decode_level\": \"" << EscapeJson(report.decode_level)
         << "\",\n"
         << "  \"permission_records\": "
         << RenderPermissionRecordsArray(report.permission_records) << ",\n"
         << "  \"healing_actions\": "
         << RenderJsonArray(report.healing_actions) << ",\n"
         << "  \"continuity_diagnostics\": "
         << RenderJsonArray(report.continuity_diagnostics) << ",\n"
         << "  \"diagnostics\": "
         << RenderJsonArray(report.diagnostics) << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

std::string RenderAppOpsReportJson(const NativeApkAppOpsReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"schema_version\": \"" << EscapeJson(report.schema_version)
         << "\",\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"contract_ready\": "
         << (report.contract_ready ? "true" : "false") << ",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id)
         << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"report_json_path\": \"" << EscapeJson(report.report_json_path)
         << "\",\n"
         << "  \"persisted_state_preexisting\": "
         << (report.persisted_state_preexisting ? "true" : "false") << ",\n"
         << "  \"continuity_validated\": "
         << (report.continuity_validated ? "true" : "false") << ",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"user_id\": " << report.user_id << ",\n"
         << "  \"app_id\": " << report.app_id << ",\n"
         << "  \"sandbox_root\": \"" << EscapeJson(report.sandbox_root)
         << "\",\n"
         << "  \"app_data_dir\": \"" << EscapeJson(report.app_data_dir)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir)
         << "\",\n"
         << "  \"updated_at_unix_ms\": " << report.updated_at_unix_ms
         << ",\n"
         << "  \"continuity_state\": \""
         << EscapeJson(report.continuity_state) << "\",\n"
         << "  \"operations_count\": " << report.operations_count << ",\n"
         << "  \"allowed_operations\": "
         << RenderJsonArray(report.allowed_operations) << ",\n"
         << "  \"denied_operations\": "
         << RenderJsonArray(report.denied_operations) << ",\n"
         << "  \"default_operations\": "
         << RenderJsonArray(report.default_operations) << ",\n"
         << "  \"ignored_placeholder_operations\": "
         << RenderJsonArray(report.ignored_placeholder_operations) << ",\n"
         << "  \"app_ops\": "
         << RenderAppOpRecordsArray(report.operation_records) << ",\n"
         << "  \"healing_actions\": "
         << RenderJsonArray(report.healing_actions) << ",\n"
         << "  \"continuity_diagnostics\": "
         << RenderJsonArray(report.continuity_diagnostics) << ",\n"
         << "  \"diagnostics\": "
         << RenderJsonArray(report.diagnostics) << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

void RecordOperation(NativeApkAppOpsReport* report,
                     const std::string& operation_name,
                     const std::string& mode,
                     const std::string& required_permission,
                     const std::string& reason,
                     const std::string& source,
                     std::uint64_t updated_at_unix_ms) {
  report->operation_records.push_back(
      {.operation_name = operation_name,
       .mode = mode,
       .required_permission = required_permission,
       .reason = reason,
       .source = source,
       .updated_at_unix_ms = updated_at_unix_ms});
  if (mode == "allowed") {
    report->allowed_operations.push_back(operation_name);
  } else if (mode == "denied") {
    report->denied_operations.push_back(operation_name);
  } else if (mode == "ignored_placeholder") {
    report->ignored_placeholder_operations.push_back(operation_name);
  } else {
    report->default_operations.push_back(operation_name);
  }
}

bool HasGrantedPermission(const NativeApkPermissionsReport& permissions,
                          const std::string& permission_name) {
  return std::find(permissions.granted_permissions.begin(),
                   permissions.granted_permissions.end(),
                   permission_name) != permissions.granted_permissions.end();
}

}  // namespace

NativeApkPermissionBridgeSession::NativeApkPermissionBridgeSession(
    NativeApkPermissionBridgeContext context)
    : context_(std::move(context)) {}

const NativeApkPermissionBridgeContext&
NativeApkPermissionBridgeSession::context() const {
  return context_;
}

NativeApkPermissionsReport
NativeApkPermissionBridgeSession::BuildPermissionsReport() const {
  fs::create_directories(context_.artifact_root);
  fs::create_directories(context_.app_data_dir);

  NativeApkPermissionsReport report;
  report.session_id = context_.session_id;
  report.artifact_root = context_.artifact_root;
  report.report_json_path =
      (fs::path(context_.artifact_root) / "permission-state.json").string();
  report.package_name = context_.package_name;
  report.user_id = context_.user_id;
  report.app_id = context_.app_id;
  report.sandbox_root = context_.sandbox_root;
  report.app_data_dir = context_.app_data_dir;
  report.apk_path = context_.apk_path;
  report.staged_dir = context_.staged_dir;
  report.updated_at_unix_ms =
      ComputeDeterministicUnixMs(context_.package_name, context_.user_id,
                                 context_.app_id);
  report.persisted_state_preexisting = fs::exists(report.report_json_path);

  if (context_.manifest_source != "archive_plain_xml" &&
      context_.manifest_source != "archive_binary_xml_decoded") {
    report.decode_level = "unsupported_manifest_source";
    AppendError(&report.errors,
                "permissions_manifest_source_unsupported:" +
                    context_.manifest_source);
    AppendError(&report.diagnostics,
                "binary_manifest_permission_decode_not_supported_yet");
    WriteTextFile(report.report_json_path, RenderPermissionsReportJson(report));
    return report;
  }

  report.decode_level =
      context_.manifest_source == "archive_binary_xml_decoded"
          ? "uses_permission_decoded_binary_xml"
          : "uses_permission_plain_xml";
  report.requested_permissions =
      ParseRequestedPermissions(context_.manifest_contents);
  for (const auto& permission_name : report.requested_permissions) {
    const auto decision = DeterminePermissionDecision(permission_name);
    report.permission_records.push_back(
        {.permission_name = permission_name,
         .grant_state = decision.grant_state,
         .protection_level = decision.protection_level,
         .source = decision.source,
         .rationale = decision.rationale,
         .updated_at_unix_ms = report.updated_at_unix_ms});
    if (decision.grant_state == "granted") {
      report.granted_permissions.push_back(permission_name);
    } else {
      report.denied_permissions.push_back(permission_name);
      AppendUnique(&report.diagnostics,
                   "permission_denied:" + permission_name);
    }
  }

  const auto validation = ValidateExistingContract(
      report.report_json_path, report.schema_version, report.package_name,
      report.user_id, report.app_id, report.sandbox_root, report.app_data_dir,
      report.apk_path, report.staged_dir,
      report.updated_at_unix_ms, "permission_state_missing",
      "initialize_missing_permission_state",
      "permission_state_json_malformed",
      "rebuild_malformed_permission_state",
      "permission_state_incompatible",
      "rebuild_incompatible_permission_state", "permission_state_stale",
      "refresh_stale_permission_state");
  report.healing_actions = validation.healing_actions;
  for (const auto& diagnostic : validation.diagnostics) {
    AppendUnique(&report.diagnostics, diagnostic);
  }
  switch (validation.state) {
    case ContractValidationOutcome::State::kMissing:
      report.continuity_state = "initialized_missing_state";
      report.continuity_validated = true;
      AppendUnique(&report.continuity_diagnostics,
                   "permission_state_initialized_from_missing_artifact");
      break;
    case ContractValidationOutcome::State::kMalformed:
      report.continuity_state = "recovered_malformed_state";
      report.continuity_validated = true;
      AppendUnique(&report.continuity_diagnostics,
                   "permission_state_rebuilt_after_malformed_json");
      break;
    case ContractValidationOutcome::State::kIncompatible:
      report.continuity_state = "recovered_incompatible_state";
      report.continuity_validated = true;
      AppendUnique(&report.continuity_diagnostics,
                   "permission_state_rebuilt_after_identity_mismatch");
      break;
    case ContractValidationOutcome::State::kStale:
      report.continuity_state = "refreshed_stale_state";
      report.continuity_validated = true;
      AppendUnique(&report.continuity_diagnostics,
                   "permission_state_refreshed_after_stale_contract");
      break;
    case ContractValidationOutcome::State::kIncomplete:
      break;
    case ContractValidationOutcome::State::kValid:
      break;
  }
  if (validation.state == ContractValidationOutcome::State::kValid) {
    const std::string existing_json = ReadTextFile(report.report_json_path);
    if (!PermissionContractLooksComplete(existing_json, report)) {
      report.healing_actions = {"rebuild_incomplete_permission_state"};
      report.continuity_state = "recovered_incomplete_state";
      report.continuity_validated = true;
      AppendUnique(&report.continuity_diagnostics,
                   "permission_state_rebuilt_after_incomplete_contract");
      AppendUnique(&report.diagnostics, "permission_state_incomplete");
      AppendUnique(
          &report.diagnostics,
          "Self-Healing Android Device permission contract incomplete; rebuilding deterministic state");
    } else {
      report.continuity_state = "validated_existing_state";
      report.continuity_validated = true;
      AppendUnique(&report.continuity_diagnostics,
                   "permission_state_validated_without_rewrite");
    }
  }

  report.ready = true;
  report.contract_ready = true;
  WriteTextFile(report.report_json_path, RenderPermissionsReportJson(report));
  return report;
}

NativeApkAppOpsReport NativeApkPermissionBridgeSession::BuildAppOpsReport(
    const NativeApkPermissionsReport& permissions) const {
  fs::create_directories(context_.artifact_root);
  fs::create_directories(context_.app_data_dir);

  NativeApkAppOpsReport report;
  report.session_id = context_.session_id;
  report.artifact_root = context_.artifact_root;
  report.report_json_path =
      (fs::path(context_.artifact_root) / "app-ops.json").string();
  report.package_name = context_.package_name;
  report.user_id = context_.user_id;
  report.app_id = context_.app_id;
  report.sandbox_root = context_.sandbox_root;
  report.app_data_dir = context_.app_data_dir;
  report.apk_path = context_.apk_path;
  report.staged_dir = context_.staged_dir;
  report.updated_at_unix_ms =
      ComputeDeterministicUnixMs(context_.package_name, context_.user_id,
                                 context_.app_id);
  report.persisted_state_preexisting = fs::exists(report.report_json_path);

  const auto validation = ValidateExistingContract(
      report.report_json_path, report.schema_version, report.package_name,
      report.user_id, report.app_id, report.sandbox_root, report.app_data_dir,
      report.apk_path, report.staged_dir,
      report.updated_at_unix_ms, "app_ops_state_missing",
      "initialize_missing_app_ops_state", "app_ops_json_malformed",
      "rebuild_malformed_app_ops_state", "app_ops_state_incompatible",
      "rebuild_incompatible_app_ops_state", "app_ops_state_stale",
      "refresh_stale_app_ops_state");
  report.healing_actions = validation.healing_actions;
  for (const auto& diagnostic : validation.diagnostics) {
    AppendUnique(&report.diagnostics, diagnostic);
  }
  switch (validation.state) {
    case ContractValidationOutcome::State::kMissing:
      report.continuity_state = "initialized_missing_state";
      report.continuity_validated = true;
      AppendUnique(&report.continuity_diagnostics,
                   "app_ops_state_initialized_from_missing_artifact");
      break;
    case ContractValidationOutcome::State::kMalformed:
      report.continuity_state = "recovered_malformed_state";
      report.continuity_validated = true;
      AppendUnique(&report.continuity_diagnostics,
                   "app_ops_state_rebuilt_after_malformed_json");
      break;
    case ContractValidationOutcome::State::kIncompatible:
      report.continuity_state = "recovered_incompatible_state";
      report.continuity_validated = true;
      AppendUnique(&report.continuity_diagnostics,
                   "app_ops_state_rebuilt_after_identity_mismatch");
      break;
    case ContractValidationOutcome::State::kStale:
      report.continuity_state = "refreshed_stale_state";
      report.continuity_validated = true;
      AppendUnique(&report.continuity_diagnostics,
                   "app_ops_state_refreshed_after_stale_contract");
      break;
    case ContractValidationOutcome::State::kIncomplete:
      break;
    case ContractValidationOutcome::State::kValid:
      break;
  }

  if (!permissions.ready) {
    AppendError(&report.errors, "permission_state_not_ready");
    WriteTextFile(report.report_json_path, RenderAppOpsReportJson(report));
    return report;
  }

  RecordOperation(&report, "storage_private_access", "allowed", "",
                  "app-private storage contract is always path-sandboxed",
                  "linuxoid_storage_private_policy", report.updated_at_unix_ms);

  const bool storage_permission_granted =
      HasGrantedPermission(permissions,
                           "android.permission.WRITE_EXTERNAL_STORAGE") ||
      HasGrantedPermission(permissions,
                           "android.permission.READ_EXTERNAL_STORAGE");
  RecordOperation(
      &report, "storage_sensitive_access",
      storage_permission_granted ? "allowed" : "denied",
      "android.permission.WRITE_EXTERNAL_STORAGE",
      storage_permission_granted
          ? "external storage placeholder permission granted by local policy"
          : "missing external storage placeholder permission",
      "linuxoid_storage_sensitive_policy", report.updated_at_unix_ms);

  const bool record_audio_granted =
      HasGrantedPermission(permissions, "android.permission.RECORD_AUDIO");
  RecordOperation(&report, "audio_capture_access",
                  record_audio_granted ? "allowed" : "denied",
                  "android.permission.RECORD_AUDIO",
                  record_audio_granted
                      ? "audio capture placeholder permission granted"
                      : "missing android.permission.RECORD_AUDIO grant",
                  "linuxoid_audio_capture_policy",
                  report.updated_at_unix_ms);

  const bool internet_granted =
      HasGrantedPermission(permissions, "android.permission.INTERNET");
  RecordOperation(&report, "network_placeholder_access",
                  internet_granted ? "ignored_placeholder" : "default",
                  "android.permission.INTERNET",
                  internet_granted
                      ? "network access placeholder deferred to future runtime"
                      : "network permission not granted",
                  "linuxoid_network_placeholder_policy",
                  report.updated_at_unix_ms);

  const std::string binder_mode =
      context_.activity_proof_requested
          ? (context_.binder_health == "ready" ? "allowed" : "denied")
          : "default";
  RecordOperation(&report, "binder_service_access", binder_mode, "",
                  context_.activity_proof_requested
                      ? (context_.binder_health == "ready"
                             ? "binder service registry local foundation ready"
                             : "binder service registry not ready")
                      : "activity proof not requested",
                  "linuxoid_binder_policy", report.updated_at_unix_ms);

  const std::string activity_mode =
      context_.activity_proof_requested
          ? (context_.activity_health == "ready" ? "allowed" : "denied")
          : "default";
  RecordOperation(&report, "activity_launch_access", activity_mode, "",
                  context_.activity_proof_requested
                      ? (context_.activity_health == "ready"
                             ? "activity launch contract ready"
                             : "activity launch contract blocked")
                      : "activity proof not requested",
                  "linuxoid_activity_launch_policy",
                  report.updated_at_unix_ms);

  const std::string dex_mode =
      context_.dex_proof_requested
          ? ((context_.dex_health == "ready" &&
              context_.art_health == "ready")
                 ? "allowed"
                 : "default")
          : "default";
  RecordOperation(&report, "dex_runtime_access", dex_mode, "",
                  context_.dex_proof_requested
                      ? ((context_.dex_health == "ready" &&
                          context_.art_health == "ready")
                             ? "dex metadata and ART bootstrap probe ready"
                             : "full ART execution still metadata-only")
                      : "dex proof not requested",
                  "linuxoid_dex_runtime_policy",
                  report.updated_at_unix_ms);

  report.operations_count = report.operation_records.size();
  if (validation.state == ContractValidationOutcome::State::kValid) {
    const std::string existing_json = ReadTextFile(report.report_json_path);
    if (!AppOpsContractLooksComplete(existing_json, report)) {
      report.healing_actions = {"rebuild_incomplete_app_ops_state"};
      report.continuity_state = "recovered_incomplete_state";
      report.continuity_validated = true;
      AppendUnique(&report.continuity_diagnostics,
                   "app_ops_state_rebuilt_after_incomplete_contract");
      AppendUnique(&report.diagnostics, "app_ops_state_incomplete");
      AppendUnique(
          &report.diagnostics,
          "Self-Healing Android Device AppOps contract incomplete; rebuilding deterministic state");
    } else {
      report.continuity_state = "validated_existing_state";
      report.continuity_validated = true;
      AppendUnique(&report.continuity_diagnostics,
                   "app_ops_state_validated_without_rewrite");
    }
  }
  report.ready = true;
  report.contract_ready = true;
  if (!report.denied_operations.empty()) {
    for (const auto& operation_name : report.denied_operations) {
      AppendUnique(&report.diagnostics, "app_op_denied:" + operation_name);
    }
  }
  WriteTextFile(report.report_json_path, RenderAppOpsReportJson(report));
  return report;
}

}  // namespace wfa
