#include "wfa/apk_compatibility_bridge.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace wfa {

namespace fs = std::filesystem;

namespace {

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
  fs::create_directories(path.parent_path());
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

std::string SanitizeLabel(const std::string& value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (const char character : value) {
    if (std::isalnum(static_cast<unsigned char>(character)) != 0) {
      sanitized.push_back(static_cast<char>(
          std::tolower(static_cast<unsigned char>(character))));
    } else {
      sanitized.push_back('_');
    }
  }
  while (!sanitized.empty() && sanitized.back() == '_') {
    sanitized.pop_back();
  }
  if (sanitized.empty()) {
    return "apk";
  }
  return sanitized;
}

std::string DefaultFixtureLabel(const std::string& apk_path) {
  const fs::path path(apk_path);
  const std::string stem =
      path.stem().empty() ? "apk" : SanitizeLabel(path.stem().string());
  if (!path.parent_path().empty() && !path.parent_path().filename().empty()) {
    return SanitizeLabel(path.parent_path().filename().string()) + "_" + stem;
  }
  return stem;
}

void AppendUnique(std::vector<std::string>* values, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(value);
  }
}

std::string ClassifyNativeJniStatus(const NativeApkLaunchReport& report) {
  if (report.launch_status == "no_native_libraries_found" ||
      (!report.native_libraries_present && report.native_libraries.empty())) {
    return "missing-native-lib";
  }
  if (report.native_libraries_present && report.jni_onload_called) {
    return "supported";
  }
  if (report.native_libraries_present) {
    return report.recoverable ? "degraded" : "partial";
  }
  return "blocked";
}

bool IsMissingRuntimeBlockingReason(const std::string& blocking_reason) {
  return blocking_reason == "art_runtime_unavailable" ||
         blocking_reason == "boot_classpath_unavailable" ||
         blocking_reason == "native_runtime_libraries_unavailable" ||
         blocking_reason == "runtime_bootstrap_failed";
}

std::string ClassifyRuntimeStatus(const NativeApkLaunchReport& report) {
  if (!report.runtime_bridge.ready || !report.runtime_bridge.art_runtime_available) {
    if (IsMissingRuntimeBlockingReason(report.runtime_bridge.blocking_reason) ||
        !report.runtime_bridge.art_runtime_available) {
      return "missing-runtime";
    }
    if (report.runtime_bridge.blocking_reason == "surface_not_ready" ||
        report.runtime_bridge.blocking_reason == "window_manager_not_ready") {
      return "missing-surface";
    }
    return "blocked";
  }
  if (report.runtime_bridge.bootstrap_state == "unavailable" ||
      report.runtime_bridge.bootstrap_state == "failed") {
    if (IsMissingRuntimeBlockingReason(report.runtime_bridge.blocking_reason) ||
        !report.runtime_bridge.art_runtime_available) {
      return "missing-runtime";
    }
    return "blocked";
  }
  if (report.runtime_bridge.bootstrap_state == "recovered") {
    return "recovered";
  }
  if (report.runtime_bridge.bootstrap_state == "degraded") {
    return "degraded";
  }
  if (report.runtime_bridge.bootstrap_state == "ready") {
    return "supported";
  }
  return "blocked";
}

std::string ClassifyJavaProofStatus(const NativeApkLaunchReport& report) {
  if (!report.java_apk_proof.ready) {
    if (report.java_apk_proof.blocking_reason.find("runtime") !=
            std::string::npos ||
        report.java_apk_proof.blocking_reason.find("art_runtime") !=
            std::string::npos) {
      return "missing-runtime";
    }
    if (report.java_apk_proof.blocking_reason.find("surface") !=
        std::string::npos) {
      return "missing-surface";
    }
    return report.recoverable ? "degraded" : "blocked";
  }
  if (!report.java_apk_proof.java_execution_supported) {
    return "needs-real-art";
  }
  return "supported";
}

bool HasSuccessfulSelfHealingAction(
    const NativeApkLaunchReport& report,
    std::initializer_list<const char*> action_names) {
  for (const auto& action : report.self_healing_android_device.actions) {
    if (action.result != "attempted_succeeded") {
      continue;
    }
    for (const char* action_name : action_names) {
      if (action.action == action_name) {
        return true;
      }
    }
  }
  return false;
}

bool SurfaceOrWindowRecoverySucceeded(const NativeApkLaunchReport& report) {
  return report.window_manager.recovered ||
         HasSuccessfulSelfHealingAction(
             report, {"restart_surface", "rebuild_window_manager_state"});
}

std::string ClassifyWindowSurfaceStatus(const NativeApkLaunchReport& report) {
  if (!report.surface_proof_ready) {
    return "missing-surface";
  }
  if (!report.window_manager.ready) {
    const std::string& blocking_reason = report.window_manager.blocking_reason;
    if (blocking_reason == "surface_not_ready" ||
        blocking_reason == "surface_session_not_materialized" ||
        blocking_reason == "surface_geometry_unavailable") {
      return "missing-surface";
    }
    return "blocked";
  }
  if (SurfaceOrWindowRecoverySucceeded(report)) {
    return "recovered";
  }
  return "supported";
}

std::string DetermineOverallStatus(const NativeApkLaunchReport& report) {
  const std::string runtime_status = ClassifyRuntimeStatus(report);
  const std::string window_status = ClassifyWindowSurfaceStatus(report);
  if (!report.manifest_present || !report.manifest_metadata_ready) {
    return "blocked";
  }
  if (report.launch_status == "no_native_libraries_found" ||
      (!report.native_libraries_present && report.native_libraries.empty())) {
    return "missing-native-lib";
  }
  if (window_status == "missing-surface") {
    return "missing-surface";
  }
  if (window_status == "blocked") {
    return "blocked";
  }
  if (runtime_status == "missing-surface") {
    return "missing-surface";
  }
  if (runtime_status == "missing-runtime") {
    return "missing-runtime";
  }
  if (!report.permissions.denied_permissions.empty()) {
    return "partial";
  }
  if (report.self_healing_android_device.ready &&
      report.self_healing_android_device.final_health == "recovered" &&
      SurfaceOrWindowRecoverySucceeded(report)) {
    return "recovered";
  }
  if (!report.java_apk_proof.ready) {
    return report.recoverable ? "degraded" : "blocked";
  }
  if (!report.java_apk_proof.java_execution_supported) {
    return "needs-real-art";
  }
  if (runtime_status == "blocked") {
    return "blocked";
  }
  if (runtime_status == "degraded") {
    return "degraded";
  }
  if (report.launch_ready && report.native_libraries_present &&
      report.jni_onload_called) {
    return "supported";
  }
  return report.recoverable ? "degraded" : "blocked";
}

std::string DetermineOverallBlockingReason(const NativeApkLaunchReport& report,
                                           const std::string& overall_status) {
  if (overall_status == "missing-native-lib") {
    return "native_library_payload_missing";
  }
  if (overall_status == "missing-surface") {
    return report.window_manager.blocking_reason != "none"
               ? report.window_manager.blocking_reason
               : "surface_contract_unavailable";
  }
  if (overall_status == "missing-runtime") {
    return report.runtime_bridge.blocking_reason != "none"
               ? report.runtime_bridge.blocking_reason
               : report.java_apk_proof.blocking_reason;
  }
  if (overall_status == "needs-real-art") {
    return "managed_bytecode_execution_not_implemented";
  }
  if (overall_status == "partial" &&
      !report.permissions.denied_permissions.empty()) {
    return "requested_permissions_denied";
  }
  if (overall_status == "recovered") {
    return "recovered_by_self_healing_android_device";
  }
  if (report.java_apk_proof.blocking_reason != "none") {
    return report.java_apk_proof.blocking_reason;
  }
  if (report.recommended_recovery_action != "none") {
    return report.recommended_recovery_action;
  }
  if (!report.errors.empty()) {
    return report.errors.front();
  }
  return "none";
}

std::string DetermineOverallRecoveryAction(const NativeApkLaunchReport& report,
                                           const std::string& overall_status) {
  if (overall_status == "missing-native-lib") {
    return "stage_abi_matching_native_library";
  }
  if (overall_status == "missing-surface") {
    return report.window_manager.recommended_recovery_action == "none"
               ? "rebuild_window_manager_state"
               : report.window_manager.recommended_recovery_action;
  }
  if (overall_status == "missing-runtime") {
    return report.runtime_bridge.recommended_recovery_action == "none"
               ? "retry_runtime_bootstrap"
               : report.runtime_bridge.recommended_recovery_action;
  }
  if (overall_status == "needs-real-art") {
    return "needs_real_art_bytecode_execution";
  }
  if (overall_status == "partial" &&
      !report.permissions.denied_permissions.empty()) {
    return "safe_mode_launch";
  }
  if (overall_status == "recovered") {
    return "none";
  }
  if (report.java_apk_proof.recommended_recovery_action != "none") {
    return report.java_apk_proof.recommended_recovery_action;
  }
  if (report.recommended_recovery_action != "none") {
    return report.recommended_recovery_action;
  }
  return "none";
}

bool StatusCountsAsReady(const std::string& status) {
  return status == "supported" || status == "partial" ||
         status == "needs-real-art" || status == "recovered";
}

NativeApkCompatibilityDomainReport BuildDomainReport(
    const std::string& domain_name, const std::string& status,
    const std::string& blocking_reason,
    const std::string& recommended_recovery_action,
    const std::vector<std::string>& diagnostics) {
  return {.domain_name = domain_name,
          .status = status,
          .ready = StatusCountsAsReady(status),
          .blocking_reason = blocking_reason,
          .recommended_recovery_action = recommended_recovery_action,
          .diagnostics = diagnostics};
}

std::string RenderCompatibilityDomainsJson(
    const std::vector<NativeApkCompatibilityDomainReport>& domains) {
  std::ostringstream output;
  output << "{\n  \"domains\": [";
  for (std::size_t index = 0; index < domains.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& domain = domains[index];
    output << "{"
           << "\"domain_name\": \"" << EscapeJson(domain.domain_name) << "\", "
           << "\"status\": \"" << EscapeJson(domain.status) << "\", "
           << "\"ready\": " << (domain.ready ? "true" : "false") << ", "
           << "\"blocking_reason\": \"" << EscapeJson(domain.blocking_reason)
           << "\", "
           << "\"recommended_recovery_action\": \""
           << EscapeJson(domain.recommended_recovery_action) << "\", "
           << "\"diagnostics\": " << RenderJsonArray(domain.diagnostics)
           << "}";
  }
  output << "]\n}\n";
  return output.str();
}

std::string RenderCompatibilityEventLog(
    const NativeApkCompatibilityReport& report) {
  std::ostringstream output;
  output << "{\"state\": \"package_inspected\", \"status\": \""
         << EscapeJson(report.overall_status) << "\"}\n";
  for (const auto& domain : report.domains) {
    output << "{\"domain\": \"" << EscapeJson(domain.domain_name)
           << "\", \"status\": \"" << EscapeJson(domain.status) << "\"}\n";
  }
  output << "{\"state\": \"self_healing_android_device\", \"ready\": "
         << (report.self_healing_ready ? "true" : "false")
         << ", \"final_health\": \""
         << EscapeJson(report.self_healing_final_health) << "\"}\n";
  return output.str();
}

NativeApkCompatibilityReport BuildCompatibilityReport(
    const std::string& apk_path, const std::string& fixture_label,
    const std::string& staging_root, NativeApkLaunchReport launch_report) {
  NativeApkCompatibilityReport report;
  report.fixture_label = fixture_label;
  report.apk_path = apk_path;
  report.package_name = launch_report.package_name;
  report.requested_package_name = launch_report.requested_package_name;
  report.requested_component = launch_report.requested_component;
  report.install_id = launch_report.install_id;
  report.launcher_component = launch_report.launcher_component;
  report.resolved_component = launch_report.intent_resolution.resolved_component;
  report.process_identity = launch_report.process_manager.process_identity;
  report.process_name = launch_report.process_manager.process_name;
  report.pid_value = launch_report.process_manager.pid_value;
  report.pid_source = launch_report.process_manager.pid_source;
  report.surface_session_id = launch_report.surface.session_id;
  report.window_id = launch_report.window_manager.window_id;
  report.runtime_session_id = launch_report.runtime_bridge.session_id;
  report.runtime_handle = launch_report.runtime_bridge.runtime_handle;
  report.launch_report_json_path = launch_report.report_json_path;
  report.self_healing_ready = launch_report.self_healing_android_device.ready;
  report.self_healing_final_health =
      launch_report.self_healing_android_device.final_health;
  report.self_healing_journal_path =
      launch_report.self_healing_android_device.journal_path;
  report.recoverable = launch_report.recoverable;
  report.launch_report = std::move(launch_report);

  fs::path artifact_root;
  if (!report.launch_report.storage.app_data_dir.empty()) {
    artifact_root =
        fs::path(report.launch_report.storage.app_data_dir) / "compatibility";
  } else if (!report.launch_report.staged_dir.empty()) {
    artifact_root =
        fs::path(report.launch_report.staged_dir) / "compatibility";
  } else {
    artifact_root = fs::path(staging_root) / "compatibility" / fixture_label;
  }
  fs::create_directories(artifact_root);
  report.artifact_root = artifact_root.string();
  report.report_json_path = (artifact_root / "compatibility-report.json").string();
  report.domains_json_path = (artifact_root / "compatibility-domains.json").string();
  report.event_log_path = (artifact_root / "compatibility-events.jsonl").string();

  const bool manifest_ready =
      report.launch_report.manifest_present &&
      report.launch_report.manifest_metadata_ready;
  report.domains.push_back(BuildDomainReport(
      "manifest",
      manifest_ready ? "supported" : "blocked",
      manifest_ready ? "none" : "manifest_metadata_unavailable",
      manifest_ready ? "none" : "inspect_manifest_metadata",
      manifest_ready ? std::vector<std::string>{"manifest_metadata_ready"}
                     : report.launch_report.errors));

  const bool package_ready = report.launch_report.package_manager.ready;
  report.domains.push_back(BuildDomainReport(
      "package_metadata",
      package_ready ? "supported" : "blocked",
      package_ready ? "none"
                    : report.launch_report.package_manager.errors.empty()
                          ? "package_manager_not_ready"
                          : report.launch_report.package_manager.errors.front(),
      package_ready ? "none"
                    : report.launch_report.intent_resolution
                          .recommended_recovery_action,
      package_ready ? std::vector<std::string>{"package_manager_ready"}
                    : report.launch_report.package_manager.errors));

  const bool activity_ready = report.launch_report.intent_resolution.ready &&
                              report.launch_report.activity_launch.ready;
  report.domains.push_back(BuildDomainReport(
      "activities_intents",
      activity_ready ? "supported"
                     : (report.launch_report.intent_resolution.blocking_reason ==
                                "no_launcher_activity" ||
                            report.launch_report.intent_resolution
                                    .blocking_reason ==
                                "ambiguous_launcher_activities"
                        ? "blocked"
                        : "degraded"),
      activity_ready ? "none"
                     : (report.launch_report.intent_resolution.blocking_reason !=
                                "none"
                            ? report.launch_report.intent_resolution
                                  .blocking_reason
                            : report.launch_report.activity_launch
                                  .blocking_reason),
      activity_ready ? "none"
                     : (report.launch_report.intent_resolution
                                    .recommended_recovery_action != "none"
                            ? report.launch_report.intent_resolution
                                  .recommended_recovery_action
                            : report.launch_report.activity_launch
                                  .recommended_recovery_action),
      activity_ready ? std::vector<std::string>{"activity_launch_ready"}
                     : report.launch_report.activity_launch.dependency_details));

  const bool permissions_ready =
      report.launch_report.permissions.ready && report.launch_report.app_ops.ready;
  const bool permission_limited =
      !report.launch_report.permissions.denied_permissions.empty();
  std::vector<std::string> permissions_diagnostics =
      report.launch_report.permissions.diagnostics;
  permissions_diagnostics.insert(permissions_diagnostics.end(),
                                 report.launch_report.app_ops.diagnostics.begin(),
                                 report.launch_report.app_ops.diagnostics.end());
  report.domains.push_back(BuildDomainReport(
      "permissions_appops",
      permissions_ready ? (permission_limited ? "partial" : "supported")
                        : "blocked",
      permissions_ready
          ? (permission_limited ? "requested_permissions_denied" : "none")
          : "permission_or_app_ops_contract_not_ready",
      permissions_ready
          ? (permission_limited ? "safe_mode_launch" : "none")
          : "rebuild_permission_state",
      permissions_diagnostics));

  const bool storage_ready = report.launch_report.storage.ready &&
                             report.launch_report.sandbox_health == "ready";
  report.domains.push_back(BuildDomainReport(
      "storage_sandbox", storage_ready ? "supported" : "blocked",
      storage_ready ? "none" : "storage_sandbox_not_ready",
      storage_ready ? "none" : "repair_app_storage",
      storage_ready ? std::vector<std::string>{"storage_and_sandbox_ready"}
                    : report.launch_report.storage.errors));

  std::vector<std::string> native_diagnostics = report.launch_report.diagnostics;
  if (!report.launch_report.native_execute.output.empty()) {
    native_diagnostics.push_back(report.launch_report.native_execute.output);
  }
  report.domains.push_back(BuildDomainReport(
      "native_jni", ClassifyNativeJniStatus(report.launch_report),
      report.launch_report.launch_status == "no_native_libraries_found"
          ? "native_library_payload_missing"
          : "none",
      report.launch_report.launch_status == "no_native_libraries_found"
          ? "stage_abi_matching_native_library"
          : "none",
      native_diagnostics));

  const bool assets_ready = report.launch_report.asset_bridge.ready &&
                            report.launch_report.resource_bridge.ready;
  std::vector<std::string> asset_diagnostics =
      report.launch_report.asset_bridge.errors;
  asset_diagnostics.insert(asset_diagnostics.end(),
                           report.launch_report.resource_bridge.errors.begin(),
                           report.launch_report.resource_bridge.errors.end());
  report.domains.push_back(BuildDomainReport(
      "assets_resources", assets_ready ? "supported" : "blocked",
      assets_ready ? "none" : "assets_or_resources_not_ready",
      assets_ready ? "none" : "restage_assets", asset_diagnostics));

  const bool process_ready = report.launch_report.activity_manager.ready &&
                             report.launch_report.process_manager.ready;
  std::vector<std::string> process_diagnostics =
      report.launch_report.process_manager.diagnostics;
  process_diagnostics.insert(process_diagnostics.end(),
                             report.launch_report.process_manager.errors.begin(),
                             report.launch_report.process_manager.errors.end());
  report.domains.push_back(BuildDomainReport(
      "process_session", process_ready ? "supported" : "degraded",
      process_ready ? "none"
                    : report.launch_report.process_manager.blocking_reason,
      process_ready ? "none"
                    : report.launch_report.process_manager
                          .recommended_recovery_action,
      process_diagnostics));

  const std::string window_status =
      ClassifyWindowSurfaceStatus(report.launch_report);
  std::vector<std::string> window_diagnostics =
      report.launch_report.window_manager.diagnostics;
  window_diagnostics.insert(window_diagnostics.end(),
                            report.launch_report.window_manager.errors.begin(),
                            report.launch_report.window_manager.errors.end());
  report.domains.push_back(BuildDomainReport(
      "window_surface", window_status,
      window_status == "supported"
          ? "none"
          : report.launch_report.window_manager.blocking_reason,
      window_status == "supported"
          ? "none"
          : report.launch_report.window_manager.recommended_recovery_action,
      window_diagnostics));

  report.domains.push_back(BuildDomainReport(
      "runtime_bootstrap", ClassifyRuntimeStatus(report.launch_report),
      report.launch_report.runtime_bridge.blocking_reason,
      report.launch_report.runtime_bridge.recommended_recovery_action,
      report.launch_report.runtime_bridge.diagnostics));

  report.domains.push_back(BuildDomainReport(
      "java_kotlin_proof", ClassifyJavaProofStatus(report.launch_report),
      report.launch_report.java_apk_proof.blocking_reason,
      report.launch_report.java_apk_proof.recommended_recovery_action,
      report.launch_report.java_apk_proof.diagnostics));

  std::vector<std::string> self_heal_diagnostics =
      report.launch_report.self_healing_android_device.errors;
  if (report.self_healing_ready) {
    self_heal_diagnostics.push_back(
        "Self-Healing Android Device final health: " +
        report.self_healing_final_health);
  }
  report.domains.push_back(BuildDomainReport(
      "self_healing_android_device",
      !report.self_healing_ready
          ? (report.recoverable ? "degraded" : "blocked")
          : (report.self_healing_final_health == "recovered"
                 ? "recovered"
                 : (report.self_healing_final_health == "healthy"
                        ? "supported"
                        : (report.recoverable ? "degraded" : "blocked"))),
      report.self_healing_ready ? "none" : "self_healing_not_ready",
      report.self_healing_ready
          ? report.launch_report.self_healing_android_device
                .recommended_next_action
          : report.launch_report.recommended_recovery_action,
      self_heal_diagnostics));

  report.overall_status = DetermineOverallStatus(report.launch_report);
  report.blocking_reason =
      DetermineOverallBlockingReason(report.launch_report, report.overall_status);
  report.recommended_recovery_action = DetermineOverallRecoveryAction(
      report.launch_report, report.overall_status);

  report.ready = true;
  report.contract_ready = true;

  AppendUnique(&report.diagnostics,
               "Self-Healing Android Device compatibility overall status: " +
                   report.overall_status);
  if (report.overall_status == "needs-real-art") {
    AppendUnique(&report.diagnostics,
                 "Self-Healing Android Device compatibility remains at bootstrap wiring proof; real Java/Kotlin bytecode execution still needs ART-owned execution");
  }
  if (permission_limited) {
    AppendUnique(&report.healing_actions, "safe_mode_launch");
    AppendUnique(&report.diagnostics,
                 "Self-Healing Android Device compatibility is partial because requested dangerous permissions remain denied");
  }
  if (report.self_healing_ready &&
      report.self_healing_final_health == "recovered") {
    AppendUnique(&report.healing_actions, "watchdog_recovered_fixture_state");
  }
  if (report.recommended_recovery_action != "none") {
    AppendUnique(&report.healing_actions, report.recommended_recovery_action);
  }
  for (const auto& error : report.launch_report.errors) {
    AppendUnique(&report.errors, error);
  }

  WriteTextFile(report.domains_json_path,
                RenderCompatibilityDomainsJson(report.domains));
  WriteTextFile(report.event_log_path, RenderCompatibilityEventLog(report));
  WriteTextFile(report.report_json_path, RenderNativeApkCompatibilityJson(report));

  return report;
}

void IncrementSuiteCounter(NativeApkCompatibilitySuiteReport* report,
                           const std::string& status) {
  if (status == "supported") {
    ++report->supported_count;
  } else if (status == "partial") {
    ++report->partial_count;
  } else if (status == "blocked") {
    ++report->blocked_count;
  } else if (status == "missing-runtime") {
    ++report->missing_runtime_count;
  } else if (status == "missing-surface") {
    ++report->missing_surface_count;
  } else if (status == "missing-native-lib") {
    ++report->missing_native_lib_count;
  } else if (status == "needs-real-art") {
    ++report->needs_real_art_count;
  } else if (status == "recovered") {
    ++report->recovered_count;
  } else if (status == "degraded") {
    ++report->degraded_count;
  } else {
    ++report->blocked_count;
  }
}

}  // namespace

NativeApkCompatibilityReport InspectNativeApkCompatibility(
    const std::string& apk_path, const NativeApkCompatibilityOptions& options) {
  const NativeApkLaunchOptions launch_options{
      .staging_root = options.staging_root,
      .requested_package_name = options.requested_package_name,
      .requested_component = options.requested_component,
      .watchdog_seconds = 1,
      .surface_proof_requested = true,
      .asset_proof_requested = true,
      .lifecycle_proof_requested = true,
      .dex_proof_requested = true,
      .activity_proof_requested = true,
      .process_proof_requested = true,
      .window_proof_requested = true,
      .runtime_proof_requested = true,
      .java_proof_requested = true,
      .storage_proof_requested = true,
      .permissions_proof_requested = true,
      .self_heal_proof_requested = true,
      .simulate_missing_asset_bridge = options.simulate_missing_asset_bridge,
      .simulate_blocked_surface_proof = options.simulate_blocked_surface_proof,
      .simulate_missing_binder_service = options.simulate_missing_binder_service,
      .simulate_failed_dex_bootstrap = options.simulate_failed_dex_bootstrap,
      .simulate_failed_intent_resolution =
          options.simulate_failed_intent_resolution,
      .simulate_storage_failure = options.simulate_storage_failure,
      .simulate_permission_mismatch = options.simulate_permission_mismatch,
      .simulate_failed_runtime_bootstrap =
          options.simulate_failed_runtime_bootstrap,
  };

  auto launch_report = LaunchNativeApk(apk_path, launch_options);
  return BuildCompatibilityReport(apk_path, DefaultFixtureLabel(apk_path),
                                  options.staging_root,
                                  std::move(launch_report));
}

NativeApkCompatibilitySuiteReport InspectNativeApkCompatibilitySuite(
    const std::vector<std::string>& apk_paths, const std::string& suite_root,
    const NativeApkCompatibilityOptions& options) {
  NativeApkCompatibilitySuiteReport report;
  report.suite_root = suite_root;
  report.report_json_path =
      (fs::path(suite_root) / "suite-compatibility-report.json").string();
  fs::create_directories(suite_root);

  for (std::size_t index = 0; index < apk_paths.size(); ++index) {
    const std::string fixture_label = DefaultFixtureLabel(apk_paths[index]);
    NativeApkCompatibilityOptions entry_options = options;
    entry_options.staging_root =
        (fs::path(suite_root) / ("entry-" + std::to_string(index + 1) + "-" +
                                 fixture_label))
            .string();
    auto entry_report =
        InspectNativeApkCompatibility(apk_paths[index], entry_options);

    NativeApkCompatibilitySuiteEntry entry;
    entry.fixture_label = fixture_label;
    entry.package_name = entry_report.package_name;
    entry.apk_path = entry_report.apk_path;
    entry.overall_status = entry_report.overall_status;
    entry.blocking_reason = entry_report.blocking_reason;
    entry.recommended_recovery_action =
        entry_report.recommended_recovery_action;
    entry.recoverable = entry_report.recoverable;
    entry.report_json_path = entry_report.report_json_path;
    entry.launch_report_json_path = entry_report.launch_report_json_path;
    entry.self_healing_journal_path = entry_report.self_healing_journal_path;
    report.entries.push_back(entry);
    IncrementSuiteCounter(&report, entry.overall_status);
  }

  report.total_entries = static_cast<int>(report.entries.size());
  report.ready = !report.entries.empty();
  WriteTextFile(report.report_json_path,
                RenderNativeApkCompatibilitySuiteJson(report));
  return report;
}

std::string RenderNativeApkCompatibilityJson(
    const NativeApkCompatibilityReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"schema_version\": \"" << EscapeJson(report.schema_version)
         << "\",\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"contract_ready\": "
         << (report.contract_ready ? "true" : "false") << ",\n"
         << "  \"fixture_label\": \"" << EscapeJson(report.fixture_label)
         << "\",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"requested_package_name\": \""
         << EscapeJson(report.requested_package_name) << "\",\n"
         << "  \"requested_component\": \""
         << EscapeJson(report.requested_component) << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id) << "\",\n"
         << "  \"launcher_component\": \""
         << EscapeJson(report.launcher_component) << "\",\n"
         << "  \"resolved_component\": \""
         << EscapeJson(report.resolved_component) << "\",\n"
         << "  \"process_identity\": \""
         << EscapeJson(report.process_identity) << "\",\n"
         << "  \"process_name\": \"" << EscapeJson(report.process_name)
         << "\",\n"
         << "  \"pid_value\": " << report.pid_value << ",\n"
         << "  \"pid_source\": \"" << EscapeJson(report.pid_source)
         << "\",\n"
         << "  \"surface_session_id\": \""
         << EscapeJson(report.surface_session_id) << "\",\n"
         << "  \"window_id\": \"" << EscapeJson(report.window_id) << "\",\n"
         << "  \"runtime_session_id\": \""
         << EscapeJson(report.runtime_session_id) << "\",\n"
         << "  \"runtime_handle\": \"" << EscapeJson(report.runtime_handle)
         << "\",\n"
         << "  \"overall_status\": \"" << EscapeJson(report.overall_status)
         << "\",\n"
         << "  \"blocking_reason\": \"" << EscapeJson(report.blocking_reason)
         << "\",\n"
         << "  \"recommended_recovery_action\": \""
         << EscapeJson(report.recommended_recovery_action) << "\",\n"
         << "  \"recoverable\": " << (report.recoverable ? "true" : "false")
         << ",\n"
         << "  \"self_healing_ready\": "
         << (report.self_healing_ready ? "true" : "false") << ",\n"
         << "  \"self_healing_final_health\": \""
         << EscapeJson(report.self_healing_final_health) << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"report_json_path\": \"" << EscapeJson(report.report_json_path)
         << "\",\n"
         << "  \"domains_json_path\": \""
         << EscapeJson(report.domains_json_path) << "\",\n"
         << "  \"event_log_path\": \"" << EscapeJson(report.event_log_path)
         << "\",\n"
         << "  \"launch_report_json_path\": \""
         << EscapeJson(report.launch_report_json_path) << "\",\n"
         << "  \"self_healing_journal_path\": \""
         << EscapeJson(report.self_healing_journal_path) << "\",\n"
         << "  \"healing_actions\": "
         << RenderJsonArray(report.healing_actions) << ",\n"
         << "  \"diagnostics\": " << RenderJsonArray(report.diagnostics)
         << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << ",\n"
         << "  \"domains\": [";
  for (std::size_t index = 0; index < report.domains.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& domain = report.domains[index];
    output << "{"
           << "\"domain_name\": \"" << EscapeJson(domain.domain_name) << "\", "
           << "\"status\": \"" << EscapeJson(domain.status) << "\", "
           << "\"ready\": " << (domain.ready ? "true" : "false") << ", "
           << "\"blocking_reason\": \"" << EscapeJson(domain.blocking_reason)
           << "\", "
           << "\"recommended_recovery_action\": \""
           << EscapeJson(domain.recommended_recovery_action) << "\", "
           << "\"diagnostics\": " << RenderJsonArray(domain.diagnostics)
           << "}";
  }
  output << "],\n"
         << "  \"self_healing_android_device\": {\n"
         << "    \"ready\": "
         << (report.launch_report.self_healing_android_device.ready ? "true"
                                                                    : "false")
         << ",\n"
         << "    \"phase_name\": \""
         << EscapeJson(
                report.launch_report.self_healing_android_device.phase_name)
         << "\",\n"
         << "    \"initial_health\": \""
         << EscapeJson(report.launch_report.self_healing_android_device
                           .initial_health)
         << "\",\n"
         << "    \"final_health\": \""
         << EscapeJson(
                report.launch_report.self_healing_android_device.final_health)
         << "\",\n"
         << "    \"actions_attempted\": "
         << report.launch_report.self_healing_android_device.actions_attempted
         << ",\n"
         << "    \"actions_succeeded\": "
         << report.launch_report.self_healing_android_device.actions_succeeded
         << ",\n"
         << "    \"actions_failed\": "
         << report.launch_report.self_healing_android_device.actions_failed
         << ",\n"
         << "    \"recommended_next_action\": \""
         << EscapeJson(report.launch_report.self_healing_android_device
                           .recommended_next_action)
         << "\",\n"
         << "    \"journal_path\": \""
         << EscapeJson(
                report.launch_report.self_healing_android_device.journal_path)
         << "\"\n"
         << "  }\n"
         << "}\n";
  return output.str();
}

std::string RenderNativeApkCompatibilitySuiteJson(
    const NativeApkCompatibilitySuiteReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"schema_version\": \"" << EscapeJson(report.schema_version)
         << "\",\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"suite_root\": \"" << EscapeJson(report.suite_root) << "\",\n"
         << "  \"report_json_path\": \"" << EscapeJson(report.report_json_path)
         << "\",\n"
         << "  \"total_entries\": " << report.total_entries << ",\n"
         << "  \"supported_count\": " << report.supported_count << ",\n"
         << "  \"partial_count\": " << report.partial_count << ",\n"
         << "  \"blocked_count\": " << report.blocked_count << ",\n"
         << "  \"missing_runtime_count\": " << report.missing_runtime_count
         << ",\n"
         << "  \"missing_surface_count\": " << report.missing_surface_count
         << ",\n"
         << "  \"missing_native_lib_count\": "
         << report.missing_native_lib_count << ",\n"
         << "  \"needs_real_art_count\": " << report.needs_real_art_count
         << ",\n"
         << "  \"recovered_count\": " << report.recovered_count << ",\n"
         << "  \"degraded_count\": " << report.degraded_count << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << ",\n"
         << "  \"entries\": [";
  for (std::size_t index = 0; index < report.entries.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& entry = report.entries[index];
    output << "{"
           << "\"fixture_label\": \"" << EscapeJson(entry.fixture_label)
           << "\", "
           << "\"package_name\": \"" << EscapeJson(entry.package_name)
           << "\", "
           << "\"apk_path\": \"" << EscapeJson(entry.apk_path) << "\", "
           << "\"overall_status\": \"" << EscapeJson(entry.overall_status)
           << "\", "
           << "\"blocking_reason\": \"" << EscapeJson(entry.blocking_reason)
           << "\", "
           << "\"recommended_recovery_action\": \""
           << EscapeJson(entry.recommended_recovery_action) << "\", "
           << "\"recoverable\": " << (entry.recoverable ? "true" : "false")
           << ", "
           << "\"report_json_path\": \""
           << EscapeJson(entry.report_json_path) << "\", "
           << "\"launch_report_json_path\": \""
           << EscapeJson(entry.launch_report_json_path) << "\", "
           << "\"self_healing_journal_path\": \""
           << EscapeJson(entry.self_healing_journal_path) << "\"}";
  }
  output << "]\n}\n";
  return output.str();
}

}  // namespace wfa
