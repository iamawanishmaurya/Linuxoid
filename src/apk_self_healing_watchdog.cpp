#include "wfa/apk_self_healing_watchdog.hpp"

#include "wfa/apk_activity_launch_bridge.hpp"
#include "wfa/apk_archive.hpp"
#include "wfa/apk_asset_bridge.hpp"
#include "wfa/apk_dex_bridge.hpp"
#include "wfa/apk_lifecycle_bridge.hpp"
#include "wfa/apk_native_launch.hpp"
#include "wfa/apk_permission_bridge.hpp"
#include "wfa/apk_process_bridge.hpp"
#include "wfa/apk_runtime_bridge.hpp"
#include "wfa/apk_storage_bridge.hpp"
#include "wfa/binder_service_manager.hpp"
#include "wfa/native_window_surface.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

struct WorkingHealthState {
  bool launch_ready = false;
  bool surface_ready = false;
  bool window_ready = false;
  bool runtime_ready = false;
  bool asset_ready = false;
  bool resource_ready = false;
  bool lifecycle_ready = false;
  bool looper_ready = false;
  bool input_ready = false;
  bool binder_ready = false;
  bool dex_ready = false;
  bool art_ready = false;
  bool package_manager_ready = false;
  bool intent_resolution_ready = false;
  bool activity_launch_ready = false;
  bool storage_ready = false;
  bool sandbox_ready = false;
  bool permission_ready = false;
  bool app_ops_ready = false;
  bool activity_manager_ready = false;
  bool process_ready = false;
  bool recoverable = false;
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

std::string RenderRecoveryActionsArray(
    const std::vector<SelfHealingAndroidDeviceRecoveryAction>& actions) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < actions.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& action = actions[index];
    output << "{"
           << "\"sequence_id\": " << action.sequence_id << ", "
           << "\"subsystem\": \"" << EscapeJson(action.subsystem) << "\", "
           << "\"reason\": \"" << EscapeJson(action.reason) << "\", "
           << "\"action\": \"" << EscapeJson(action.action) << "\", "
           << "\"result\": \"" << EscapeJson(action.result) << "\", "
           << "\"recoverable\": " << (action.recoverable ? "true" : "false")
           << ", "
           << "\"initial_health\": \"" << EscapeJson(action.initial_health)
           << "\", "
           << "\"final_health\": \"" << EscapeJson(action.final_health)
           << "\"}";
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

void AppendError(std::vector<std::string>* errors, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(errors->begin(), errors->end(), value) == errors->end()) {
    errors->push_back(value);
  }
}

WorkingHealthState BuildWorkingHealthState(
    const NativeApkLaunchReport& report) {
  return {.launch_ready = report.launch_ready,
          .surface_ready = !report.surface_proof_requested ||
                           report.surface_proof_ready,
          .window_ready = !report.window_proof_requested ||
                          report.window_manager.ready,
          .runtime_ready = !report.runtime_proof_requested ||
                           report.runtime_bridge.ready,
          .asset_ready =
              !report.asset_proof_requested || report.asset_bridge.ready,
          .resource_ready = !report.asset_proof_requested ||
                            report.resource_bridge.ready,
          .lifecycle_ready = !report.lifecycle_proof_requested ||
                             report.lifecycle.ready,
          .looper_ready =
              !report.lifecycle_proof_requested || report.looper.ready,
          .input_ready = !report.lifecycle_proof_requested ||
                         report.input_queue.ready,
          .binder_ready = !report.activity_proof_requested ||
                          report.binder_health == "ready",
          .dex_ready = !report.dex_proof_requested || report.dex.ready,
          .art_ready =
              !report.dex_proof_requested || report.art_bootstrap.ready,
          .package_manager_ready =
              !report.activity_proof_requested || report.package_manager.ready,
          .intent_resolution_ready =
              !report.activity_proof_requested || report.intent_resolution.ready,
          .activity_launch_ready =
              !report.activity_proof_requested || report.activity_launch.ready,
          .storage_ready =
              !report.storage_proof_requested || report.storage.ready,
          .sandbox_ready =
              !report.storage_proof_requested ||
              report.sandbox_health == "ready",
          .permission_ready =
              !report.permissions_proof_requested ||
              report.permissions.ready,
          .app_ops_ready =
              !report.permissions_proof_requested || report.app_ops.ready,
          .activity_manager_ready =
              !report.process_proof_requested || report.activity_manager.ready,
          .process_ready =
              !report.process_proof_requested || report.process_manager.ready,
          .recoverable = report.recoverable};
}

bool AllContractsReady(const NativeApkLaunchReport& report,
                       const WorkingHealthState& state) {
  return state.launch_ready && state.surface_ready && state.window_ready &&
         state.runtime_ready &&
         state.asset_ready &&
         state.resource_ready && state.lifecycle_ready && state.looper_ready &&
         state.input_ready && state.binder_ready && state.dex_ready &&
         state.art_ready && state.package_manager_ready &&
         state.intent_resolution_ready && state.activity_launch_ready &&
         state.storage_ready && state.sandbox_ready &&
         state.permission_ready && state.app_ops_ready &&
         state.activity_manager_ready && state.process_ready;
}

std::string ClassifyHealth(const NativeApkLaunchReport& report,
                           const WorkingHealthState& state) {
  const bool fatal_launch =
      report.launch_status == "invalid_request" ||
      report.launch_status == "invalid_apk" ||
      report.launch_status == "manifest_metadata_unavailable" ||
      report.launch_status == "unsafe_archive_entry";
  if (fatal_launch) {
    return "failed";
  }
  if (AllContractsReady(report, state)) {
    return "healthy";
  }
  const bool blocking_issue =
      !state.launch_ready || !state.surface_ready || !state.window_ready ||
      !state.runtime_ready ||
      !state.lifecycle_ready || !state.looper_ready || !state.input_ready || !state.binder_ready ||
      !state.dex_ready || !state.art_ready || !state.package_manager_ready ||
      !state.intent_resolution_ready || !state.activity_launch_ready ||
      !state.storage_ready || !state.sandbox_ready ||
      !state.permission_ready || !state.app_ops_ready ||
      !state.activity_manager_ready || !state.process_ready;
  if (blocking_issue) {
    return state.recoverable ? "blocked" : "unrecoverable";
  }
  return state.recoverable ? "degraded" : "unrecoverable";
}

std::string DetermineRecommendedNextAction(
    const NativeApkLaunchReport& report, const WorkingHealthState& state) {
  if (AllContractsReady(report, state)) {
    return "none";
  }
  if (!state.launch_ready &&
      (report.launch_status == "native_library_staging_failed" ||
       report.launch_status == "libraries_failed_to_load" ||
       report.launch_status == "jni_onload_missing_or_failed" ||
       report.launch_status == "native_activity_entrypoint_missing" ||
       report.launch_status == "jni_registration_dispatch_required" ||
       report.launch_status == "jni_direct_method_dispatch_required" ||
       report.launch_status == "linuxoid_managed_app_start_bridge_required")) {
    return "inspect_native_launch_diagnostics";
  }
  if (!state.asset_ready) {
    return "restage_assets";
  }
  if (!state.resource_ready) {
    return "rebuild_resource_metadata";
  }
  if (!state.storage_ready || !state.sandbox_ready) {
    return "repair_app_storage";
  }
  if (!state.permission_ready || !state.app_ops_ready) {
    return "rebuild_permission_state";
  }
  if (!state.surface_ready) {
    return "restart_surface";
  }
  if (!state.lifecycle_ready || !state.looper_ready) {
    return "restart_lifecycle";
  }
  if (!state.input_ready) {
    return "reset_input_queue";
  }
  if (!state.binder_ready) {
    return "refresh_binder_services";
  }
  if (!state.dex_ready || !state.art_ready) {
    return "rebuild_dex_bootstrap";
  }
  if (!state.package_manager_ready || !state.intent_resolution_ready ||
      !state.activity_launch_ready) {
    if (report.intent_resolution.recommended_recovery_action != "none") {
      return report.intent_resolution.recommended_recovery_action;
    }
    if (report.activity_launch.recommended_recovery_action != "none") {
      return report.activity_launch.recommended_recovery_action;
    }
    return "rerun_intent_resolution";
  }
  if (!state.activity_manager_ready || !state.process_ready) {
    return "rebuild_process_manager_state";
  }
  if (!state.window_ready) {
    return "rebuild_window_manager_state";
  }
  if (!state.runtime_ready) {
    return "retry_runtime_bootstrap";
  }
  if (!state.launch_ready) {
    return "safe_mode_launch";
  }
  if (report.recommended_recovery_action != "none") {
    return report.recommended_recovery_action;
  }
  return "safe_mode_launch";
}

bool HasUpstreamNativeLaunchBlocker(const NativeApkLaunchReport& report) {
  if (report.launch_ready) {
    return false;
  }
  if (report.launch_status == "native_library_staging_failed" ||
      report.launch_status == "libraries_failed_to_load" ||
      report.launch_status == "jni_onload_missing_or_failed" ||
      report.launch_status == "native_activity_entrypoint_missing" ||
      report.launch_status == "jni_registration_dispatch_required" ||
      report.launch_status == "jni_direct_method_dispatch_required" ||
      report.launch_status == "linuxoid_managed_app_start_bridge_required") {
    return true;
  }
  return report.native_loading_state == "dlopen_failed" ||
         report.native_loading_state == "staging_failed" ||
         report.native_loading_state == "jni_registration_dispatch_required" ||
         report.native_loading_state == "jni_direct_method_dispatch_required" ||
         report.native_loading_state ==
             "linuxoid_managed_app_start_bridge_required" ||
         report.native_jni_state == "crashed" ||
         report.native_jni_state == "missing";
}

std::string DescribeUpstreamNativeLaunchBlocker(
    const NativeApkLaunchReport& report) {
  if (!HasUpstreamNativeLaunchBlocker(report)) {
    return "none";
  }
  if (report.native_loading_state == "dlopen_failed" &&
      !report.native_loading_library_name.empty()) {
    return "native_dlopen_failed:" + report.native_loading_library_name;
  }
  if (report.native_loading_state == "staging_failed" &&
      !report.native_loading_library_name.empty()) {
    return "native_library_staging_failed:" +
           report.native_loading_library_name;
  }
  if (report.native_jni_state == "crashed" &&
      !report.native_loading_library_name.empty()) {
    return "jni_onload_crashed:" + report.native_loading_library_name;
  }
  if (report.native_jni_state == "missing" &&
      !report.native_loading_library_name.empty()) {
    return "jni_onload_missing:" + report.native_loading_library_name;
  }
  if (report.native_loading_state ==
          "jni_registration_dispatch_required" &&
      !report.native_loading_library_name.empty()) {
    return "jni_registration_dispatch_required:" +
           report.native_loading_library_name;
  }
  if (report.native_loading_state ==
          "jni_direct_method_dispatch_required" &&
      !report.native_loading_library_name.empty()) {
    return "jni_direct_method_dispatch_required:" +
           report.native_loading_library_name;
  }
  if (report.native_loading_state ==
          "linuxoid_managed_app_start_bridge_required" &&
      !report.native_loading_library_name.empty()) {
    return "linuxoid_managed_app_start_bridge_required:" +
           report.native_loading_library_name;
  }
  if (!report.native_loading_state.empty() &&
      report.native_loading_state != "not_requested") {
    return report.native_loading_state;
  }
  if (!report.native_jni_state.empty() &&
      report.native_jni_state != "not_requested") {
    return report.native_jni_state;
  }
  return report.launch_status.empty() ? "native_launch_blocked"
                                      : report.launch_status;
}

std::string DeterminePrimaryBlockerReason(const NativeApkLaunchReport& report,
                                          const WorkingHealthState& state) {
  if (HasUpstreamNativeLaunchBlocker(report)) {
    return DescribeUpstreamNativeLaunchBlocker(report);
  }
  if (!state.storage_ready || !state.sandbox_ready) {
    return "storage_or_sandbox_blocked";
  }
  if (!state.permission_ready || !state.app_ops_ready) {
    return "permission_or_appops_blocked";
  }
  if (!state.asset_ready) {
    return "asset_bridge_blocked";
  }
  if (!state.resource_ready) {
    return "resource_bridge_blocked";
  }
  if (!state.surface_ready) {
    return "surface_blocked";
  }
  if (!state.lifecycle_ready || !state.looper_ready) {
    return "lifecycle_or_looper_blocked";
  }
  if (!state.input_ready) {
    return "input_blocked";
  }
  if (!state.binder_ready) {
    return "binder_blocked";
  }
  if (!state.dex_ready || !state.art_ready) {
    return "dex_or_art_blocked";
  }
  if (!state.package_manager_ready || !state.intent_resolution_ready ||
      !state.activity_launch_ready) {
    return "activity_launch_contract_blocked";
  }
  if (!state.activity_manager_ready || !state.process_ready) {
    return "process_manager_contract_blocked";
  }
  if (!state.window_ready) {
    return "window_manager_contract_blocked";
  }
  if (!state.runtime_ready) {
    return "runtime_bridge_blocked";
  }
  if (!state.launch_ready) {
    return report.launch_status.empty() ? "launch_not_ready" : report.launch_status;
  }
  return "none";
}

NativeApkAssetBridgeSession BuildAssetBridgeSession(
    const NativeApkLaunchReport& report) {
  const std::string selected_library_path =
      !report.native_execute.selected_library_path.empty()
          ? report.native_execute.selected_library_path
          : (report.native_libraries.empty() ? ""
                                             : report.native_libraries.front());
  return NativeApkAssetBridgeSession(
      {.session_id = report.package_name + ":" + report.install_id + ":assets",
       .package_name = report.package_name,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .selected_library_path = selected_library_path,
       .launch_status = report.launch_status,
       .asset_root = report.asset_root,
       .resource_root = report.resource_root,
       .manifest_source = report.manifest_source});
}

NativeApkLifecycleBridgeSession BuildLifecycleBridgeSession(
    const NativeApkLaunchReport& report, const fs::path& artifact_root) {
  const std::string selected_library_path =
      !report.native_execute.selected_library_path.empty()
          ? report.native_execute.selected_library_path
          : (report.native_libraries.empty() ? ""
                                             : report.native_libraries.front());
  const int width = report.surface.width > 0 ? report.surface.width : 320;
  const int height = report.surface.height > 0 ? report.surface.height : 240;
  const int format = report.surface.format > 0 ? report.surface.format : 1;
  return NativeApkLifecycleBridgeSession(
      {.session_id = report.package_name + ":" + report.install_id +
                     ":self-heal-lifecycle",
       .package_name = report.package_name,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .selected_abi = report.selected_abi,
       .selected_library_path = selected_library_path,
       .launch_status = report.launch_status,
       .asset_health = report.asset_health,
       .resource_health = report.resource_health,
       .surface_health = report.surface_health,
       .surface_state = report.surface.state,
       .artifact_root = artifact_root.string(),
       .width = width,
       .height = height,
       .format = format});
}

std::string NormalizeAndroidClassName(const std::string& package_name,
                                      const std::string& class_or_component) {
  if (package_name.empty() || class_or_component.empty()) {
    return "";
  }
  std::string value = class_or_component;
  const auto slash = value.find('/');
  if (slash != std::string::npos) {
    value = value.substr(slash + 1);
  }
  if (value.empty()) {
    return "";
  }
  if (value.front() == '.') {
    return package_name + value;
  }
  if (value.rfind(package_name + ".", 0) == 0) {
    return value;
  }
  if (value.find('.') != std::string::npos) {
    return value;
  }
  return package_name + "." + value;
}

std::string ToDexDescriptorFromClassOrComponent(
    const std::string& package_name, const std::string& class_or_component) {
  const std::string class_name =
      NormalizeAndroidClassName(package_name, class_or_component);
  if (class_name.empty()) {
    return "";
  }
  std::string descriptor = "L";
  descriptor.reserve(class_name.size() + 2);
  for (const char character : class_name) {
    descriptor.push_back(character == '.' ? '/' : character);
  }
  descriptor.push_back(';');
  return descriptor;
}

std::string DetermineDexEntrypointClassDescriptor(
    const NativeApkLaunchReport& report) {
  if (!report.intent_resolution.resolved_activity_class.empty()) {
    return ToDexDescriptorFromClassOrComponent(
        report.package_name, report.intent_resolution.resolved_activity_class);
  }
  const std::vector<std::string> candidates = {
      report.requested_component,
      report.intent_resolution.resolved_component,
      report.launcher_component};
  for (const auto& candidate : candidates) {
    const std::string descriptor =
        ToDexDescriptorFromClassOrComponent(report.package_name, candidate);
    if (!descriptor.empty()) {
      return descriptor;
    }
  }
  return "";
}

std::string DetermineDexEntrypointMethodName(
    const NativeApkLaunchReport& report) {
  return report.first_app_start_proof_requested ? "onCreate"
                                                : "linuxoidCheckpoint";
}

NativeApkDexBridgeSession BuildDexBridgeSession(
    const NativeApkLaunchReport& report, const fs::path& artifact_root) {
  return NativeApkDexBridgeSession(
      {.session_id = report.package_name + ":" + report.install_id +
                     ":self-heal-art",
       .package_name = report.package_name,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .dex_root = (fs::path(report.staged_dir) / "dex").string(),
       .artifact_root = artifact_root.string(),
       .entrypoint_class_descriptor =
           DetermineDexEntrypointClassDescriptor(report),
       .entrypoint_method_name = DetermineDexEntrypointMethodName(report),
       .asset_bridge_status = report.asset_health,
       .lifecycle_status = report.lifecycle_health,
       .binder_service_registry_status =
           fs::exists(fs::path(report.staged_dir) / "binder" /
                      "service-manager.json")
               ? "local_foundation_ready"
               : "not_present"});
}

NativeApkActivityLaunchBridgeSession BuildActivitySession(
    const NativeApkLaunchReport& report, const fs::path& artifact_root,
    const std::string& manifest_contents) {
  return NativeApkActivityLaunchBridgeSession(
      {.session_id = report.package_name + ":" + report.install_id +
                     ":self-heal-activity",
       .package_name = report.package_name,
       .requested_package_name = report.requested_package_name,
       .requested_component = report.requested_component,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .install_id = report.install_id,
       .version_name = report.version_name,
       .version_code = report.version_code,
       .manifest_source = report.manifest_source,
       .manifest_contents = manifest_contents,
       .launcher_component = report.launcher_component,
       .declared_activities = report.declared_activities,
       .staged_native_libraries = report.native_libraries,
       .launch_ready = report.launch_ready,
       .recoverable = report.recoverable,
       .art_runtime_available = report.art_bootstrap.art_runtime_available,
       .java_execution_supported =
           report.art_bootstrap.java_execution_supported,
       .dex_bootstrap_ready = report.art_bootstrap.dex_bootstrap_ready,
       .launch_status = report.launch_status,
       .surface_health = report.surface_health,
       .lifecycle_health = report.lifecycle_health,
       .looper_health = report.looper_health,
       .input_health = report.input_health,
       .dex_health = report.dex_health,
       .art_health = report.art_health,
       .binder_health = report.binder_health,
       .binder_service_registry_status =
           fs::exists(fs::path(report.staged_dir) / "binder" /
                      "service-manager.json")
               ? "local_foundation_ready"
               : "not_present",
       .binder_service_manager_path =
           (fs::path(report.staged_dir) / "binder" / "service-manager.json")
               .string(),
       .lifecycle_states_visited = report.lifecycle.states_visited,
       .lifecycle_current_state = report.lifecycle.current_state,
       .artifact_root = artifact_root.string()});
}

NativeApkStorageBridgeSession BuildStorageSession(
    const NativeApkLaunchReport& report, const fs::path& artifact_root) {
  const fs::path app_data_dir =
      fs::path(report.sandbox_root) / "data" / "data" / report.package_name;
  return NativeApkStorageBridgeSession(
      {.session_id = report.package_name + ":" + report.install_id +
                     ":self-heal-storage",
       .package_name = report.package_name,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .app_data_dir = app_data_dir.string(),
       .files_dir = (app_data_dir / "files").string(),
       .cache_dir = (app_data_dir / "cache").string(),
       .native_lib_dir = report.library_root,
       .asset_root = report.asset_root,
       .resource_root = report.resource_root,
       .artifact_root = artifact_root.string(),
       .uid_placeholder = 10000,
       .gid_placeholder = 10000,
       .isolation_level = "path_sandbox_only",
       .sandbox_state = "path_sandbox_only",
       .permission_metadata = {"uid_placeholder=10000",
                               "gid_placeholder=10000",
                               "app_data_dir_mode=0700",
                               "files_dir_mode=0700",
                               "cache_dir_mode=0700"}});
}

NativeApkPermissionBridgeSession BuildPermissionSession(
    const NativeApkLaunchReport& report, const fs::path& artifact_root,
    const std::string& manifest_contents) {
  const fs::path app_data_dir =
      fs::path(report.sandbox_root) / "data" / "data" / report.package_name;
  return NativeApkPermissionBridgeSession(
      {.session_id = report.package_name + ":" + report.install_id +
                     ":self-heal-permissions",
       .package_name = report.package_name,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .sandbox_root = report.sandbox_root,
       .app_data_dir = app_data_dir.string(),
       .artifact_root = (app_data_dir / "permissions").string(),
       .manifest_source = report.manifest_source,
       .manifest_contents = manifest_contents,
       .user_id = 0,
       .app_id = 10000,
       .storage_health = report.storage_health,
       .binder_health = report.binder_health,
       .dex_health = report.dex_health,
       .art_health = report.art_health,
       .activity_health = report.activity_health,
       .storage_proof_requested = report.storage_proof_requested,
       .activity_proof_requested = report.activity_proof_requested,
       .dex_proof_requested = report.dex_proof_requested});
}

NativeApkProcessManagerSession BuildProcessSession(
    const NativeApkLaunchReport& report, const WorkingHealthState& state) {
  const fs::path app_data_dir =
      !report.storage.app_data_dir.empty()
          ? fs::path(report.storage.app_data_dir)
          : (fs::path(report.sandbox_root) / "data" / "data" /
             report.package_name);
  return NativeApkProcessManagerSession(
      {.session_id = report.package_name + ":" + report.install_id +
                     ":self-heal-process-manager",
       .package_name = report.package_name,
       .requested_package_name = report.requested_package_name,
       .requested_component = report.requested_component,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .sandbox_root = report.sandbox_root,
       .app_data_dir = app_data_dir.string(),
       .artifact_root = (app_data_dir / "process-manager").string(),
       .install_id = report.install_id,
       .version_name = report.version_name,
       .version_code = report.version_code,
       .user_id = report.permissions.user_id,
       .app_id = report.permissions.app_id,
       .uid_placeholder = report.storage.uid_placeholder,
       .gid_placeholder = report.storage.gid_placeholder,
       .launch_status = report.launch_status,
       .launch_ready = report.launch_ready,
       .recoverable = report.recoverable,
       .launcher_component = report.launcher_component,
       .resolved_component = report.intent_resolution.resolved_component,
       .resolution_status = report.intent_resolution.resolution_status,
       .resolution_mode = report.intent_resolution.resolution_mode,
       .resolution_reason = report.intent_resolution.resolution_reason,
       .resolution_blocking_reason = report.intent_resolution.blocking_reason,
       .resolution_recovery_action =
           report.intent_resolution.recommended_recovery_action,
       .activity_launch_status = report.activity_launch.activity_launch_status,
       .activity_launch_blocking_reason =
           report.activity_launch.blocking_reason,
       .activity_launch_recovery_action =
           report.activity_launch.recommended_recovery_action,
       .intent_action = report.intent_resolution.action.empty()
                            ? "android.intent.action.MAIN"
                            : report.intent_resolution.action,
       .intent_categories = report.intent_resolution.categories.empty()
                            ? std::vector<std::string>{
                                      "android.intent.category.LAUNCHER"}
                                : report.intent_resolution.categories,
       .lifecycle_state = report.lifecycle.current_state,
       .surface_health = state.surface_ready ? "ready" : report.surface_health,
       .lifecycle_health =
           state.lifecycle_ready ? "ready" : report.lifecycle_health,
       .looper_health = state.looper_ready ? "ready" : report.looper_health,
       .input_health = state.input_ready ? "ready" : report.input_health,
       .dex_health = state.dex_ready ? "ready" : report.dex_health,
       .art_health = state.art_ready ? "ready" : report.art_health,
       .binder_health = state.binder_ready ? "ready" : report.binder_health,
       .activity_health = report.activity_health,
       .storage_health = state.storage_ready ? "ready" : report.storage_health,
       .sandbox_health = state.sandbox_ready ? "ready" : report.sandbox_health,
       .permission_health =
           state.permission_ready ? "ready" : report.permission_health,
       .app_ops_health = state.app_ops_ready ? "ready" : report.app_ops_health,
       .package_manager_ready = state.package_manager_ready,
       .intent_resolution_ready = state.intent_resolution_ready,
       .activity_launch_ready = state.activity_launch_ready,
       .dex_bootstrap_ready =
           state.dex_ready && state.art_ready
               ? true
               : report.art_bootstrap.dex_bootstrap_ready,
       .art_runtime_available = report.art_bootstrap.art_runtime_available,
       .java_execution_supported =
           report.art_bootstrap.java_execution_supported,
       .allow_persisted_contract_repair = true});
}

NativeApkWindowManagerSession BuildWindowSession(
    const NativeApkLaunchReport& report, const WorkingHealthState& state) {
  const fs::path app_data_dir =
      !report.storage.app_data_dir.empty()
          ? fs::path(report.storage.app_data_dir)
          : (fs::path(report.sandbox_root) / "data" / "data" /
             report.package_name);
  return NativeApkWindowManagerSession(
      {.session_id = report.package_name + ":" + report.install_id +
                     ":self-heal-window-manager",
       .package_name = report.package_name,
       .requested_package_name = report.requested_package_name,
       .requested_component = report.requested_component,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .sandbox_root = report.sandbox_root,
       .app_data_dir = app_data_dir.string(),
       .artifact_root = (app_data_dir / "window-manager").string(),
       .install_id = report.install_id,
       .version_name = report.version_name,
       .version_code = report.version_code,
       .user_id = report.permissions.user_id,
       .app_id = report.permissions.app_id,
       .uid_placeholder = report.storage.uid_placeholder,
       .gid_placeholder = report.storage.gid_placeholder,
       .launch_status = report.launch_status,
       .launch_ready = report.launch_ready,
       .recoverable = report.recoverable,
       .launcher_component = report.launcher_component,
       .resolved_component = report.intent_resolution.resolved_component,
       .activity_launch_status = report.activity_launch.activity_launch_status,
       .activity_launch_blocking_reason =
           report.activity_launch.blocking_reason,
       .activity_launch_recovery_action =
           report.activity_launch.recommended_recovery_action,
       .lifecycle_state = report.lifecycle.current_state,
       .process_identity = report.process_manager.process_identity,
       .process_name = report.process_manager.process_name,
       .pid_value = report.process_manager.pid_value,
       .pid_source = report.process_manager.pid_source,
       .surface_session_id = report.surface.session_id,
       .surface_session_root = report.surface.session_root,
       .surface_metadata_path = report.surface.metadata_path,
       .surface_event_log_path = report.surface.event_log_path,
       .surface_marker_path = report.surface.marker_path,
       .surface_state = state.surface_ready ? "recovered" : report.surface.state,
       .surface_backend = report.surface.backend,
       .surface_backing_mode = report.surface.backing_mode,
       .surface_width = report.surface.width > 0 ? report.surface.width : 328,
       .surface_height = report.surface.height > 0 ? report.surface.height : 244,
       .surface_format = report.surface.format > 0 ? report.surface.format : 1,
       .surface_first_frame_presented = state.surface_ready,
       .surface_created = state.surface_ready,
       .wayland_surface_available = report.surface.wayland_surface_available,
       .egl_surface_available = report.surface.egl_surface_available,
       .surface_recovered = state.surface_ready && report.surface_health != "ready",
       .storage_health = state.storage_ready ? "ready" : report.storage_health,
       .sandbox_health = state.sandbox_ready ? "ready" : report.sandbox_health,
       .permission_health =
           state.permission_ready ? "ready" : report.permission_health,
       .app_ops_health = state.app_ops_ready ? "ready" : report.app_ops_health,
       .binder_health = state.binder_ready ? "ready" : report.binder_health,
       .surface_health = state.surface_ready ? "ready" : report.surface_health,
       .lifecycle_health =
           state.lifecycle_ready ? "ready" : report.lifecycle_health,
       .looper_health = state.looper_ready ? "ready" : report.looper_health,
       .input_health = state.input_ready ? "ready" : report.input_health,
       .dex_health = state.dex_ready ? "ready" : report.dex_health,
       .art_health = state.art_ready ? "ready" : report.art_health,
       .activity_health =
           state.activity_launch_ready ? "ready" : report.activity_health,
       .activity_manager_health = state.activity_manager_ready
                                      ? "ready"
                                      : report.activity_manager_health,
       .process_health =
           state.process_ready ? "ready" : report.process_health,
       .activity_manager_ready = state.activity_manager_ready,
       .process_ready = state.process_ready,
       .persisted_artifact_root_preexisting =
           fs::exists(app_data_dir / "window-manager"),
       .allow_persisted_contract_repair = true});
}

NativeApkRuntimeBridgeSession BuildRuntimeSession(
    const NativeApkLaunchReport& report, const WorkingHealthState& state,
    bool bootstrap_recovered, bool simulate_bootstrap_failure) {
  const fs::path app_data_dir =
      !report.storage.app_data_dir.empty()
          ? fs::path(report.storage.app_data_dir)
          : (fs::path(report.sandbox_root) / "data" / "data" /
             report.package_name);
  std::vector<std::string> dex_files;
  for (const auto& dex_file : report.dex.files) {
    if (!dex_file.staged_path.empty()) {
      dex_files.push_back(dex_file.staged_path);
    }
  }
  if (dex_files.empty()) {
    dex_files = report.art_bootstrap.dex_files;
  }

  const char* runtime_root_override_env =
      std::getenv("LINUXOID_ART_RUNTIME_ROOT_OVERRIDE");
  const char* runtime_probe_override_env =
      std::getenv("LINUXOID_ART_RUNTIME_PROBE_OVERRIDE");

  return NativeApkRuntimeBridgeSession(
      {.session_id = report.package_name + ":" + report.install_id +
                     ":self-heal-art-runtime",
       .package_name = report.package_name,
       .requested_package_name = report.requested_package_name,
       .requested_component = report.requested_component,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .sandbox_root = report.sandbox_root,
       .app_data_dir = app_data_dir.string(),
       .artifact_root = (app_data_dir / "runtime-manager").string(),
       .install_id = report.install_id,
       .version_name = report.version_name,
       .version_code = report.version_code,
       .user_id = report.permissions.user_id,
       .app_id = report.permissions.app_id,
       .uid_placeholder = report.storage.uid_placeholder,
       .gid_placeholder = report.storage.gid_placeholder,
       .launch_status = report.launch_status,
       .launch_ready = report.launch_ready,
       .recoverable = report.recoverable,
       .launcher_component = report.launcher_component,
       .resolved_component = report.intent_resolution.resolved_component,
       .activity_launch_status = report.activity_launch.activity_launch_status,
       .activity_launch_blocking_reason =
           report.activity_launch.blocking_reason,
       .activity_launch_recovery_action =
           report.activity_launch.recommended_recovery_action,
       .intent_action = report.intent_resolution.action.empty()
                            ? "android.intent.action.MAIN"
                            : report.intent_resolution.action,
       .intent_categories = report.intent_resolution.categories.empty()
                                ? std::vector<std::string>{
                                      "android.intent.category.LAUNCHER"}
                                : report.intent_resolution.categories,
       .lifecycle_state = report.lifecycle.current_state,
       .process_session_id = report.activity_manager.session_id,
       .process_identity = report.process_manager.process_identity,
       .process_name = report.process_manager.process_name,
       .pid_value = report.process_manager.pid_value,
       .pid_source = report.process_manager.pid_source,
       .window_session_id = report.window_manager.session_id,
       .window_id = report.window_manager.window_id,
       .surface_session_id = report.surface.session_id,
       .surface_state = state.surface_ready ? "recovered" : report.surface.state,
       .surface_backend = report.surface.backend,
       .surface_backing_mode = report.surface.backing_mode,
       .surface_first_frame_presented = state.surface_ready,
       .surface_created = state.surface_ready,
       .wayland_surface_available = report.surface.wayland_surface_available,
       .egl_surface_available = report.surface.egl_surface_available,
       .runtime_root_override =
           runtime_root_override_env == nullptr ? "" : runtime_root_override_env,
       .runtime_probe_override =
           runtime_probe_override_env == nullptr ? "" : runtime_probe_override_env,
       .storage_health = state.storage_ready ? "ready" : report.storage_health,
       .sandbox_health = state.sandbox_ready ? "ready" : report.sandbox_health,
       .permission_health =
           state.permission_ready ? "ready" : report.permission_health,
       .app_ops_health = state.app_ops_ready ? "ready" : report.app_ops_health,
       .binder_health = state.binder_ready ? "ready" : report.binder_health,
       .surface_health = state.surface_ready ? "ready" : report.surface_health,
       .window_health = state.window_ready ? "ready" : report.window_health,
       .lifecycle_health =
           state.lifecycle_ready ? "ready" : report.lifecycle_health,
       .looper_health = state.looper_ready ? "ready" : report.looper_health,
       .input_health = state.input_ready ? "ready" : report.input_health,
       .dex_health = state.dex_ready ? "ready" : report.dex_health,
       .art_health = state.art_ready ? "ready" : report.art_health,
       .activity_health =
           state.activity_launch_ready ? "ready" : report.activity_health,
       .activity_manager_health = state.activity_manager_ready
                                      ? "ready"
                                      : report.activity_manager_health,
       .process_health =
           state.process_ready ? "ready" : report.process_health,
       .package_manager_ready = state.package_manager_ready,
       .intent_resolution_ready = state.intent_resolution_ready,
       .activity_launch_ready = state.activity_launch_ready,
       .activity_manager_ready = state.activity_manager_ready,
       .process_ready = state.process_ready,
       .window_ready = state.window_ready,
       .dex_bootstrap_ready = state.dex_ready && state.art_ready
                                  ? true
                                  : report.art_bootstrap.dex_bootstrap_ready,
       .class_loader_ready = state.dex_ready && state.art_ready
                                 ? true
                                 : report.art_bootstrap.class_loader_ready,
       .java_execution_supported = false,
       .persisted_artifact_root_preexisting =
           fs::exists(app_data_dir / "runtime-manager"),
       .allow_persisted_contract_repair = true,
       .simulate_bootstrap_failure = simulate_bootstrap_failure,
       .bootstrap_recovered = bootstrap_recovered,
       .dex_files = dex_files});
}

bool AttemptRestageAssets(const NativeApkLaunchReport& report,
                          WorkingHealthState* state,
                          std::vector<std::string>* errors) {
  const auto asset_session = BuildAssetBridgeSession(report);
  const auto asset_report = ProveNativeApkAssetBridge(asset_session);
  if (!asset_report.ready) {
    for (const auto& error : asset_report.errors) {
      AppendError(errors, error);
    }
    return false;
  }
  state->asset_ready = true;
  return true;
}

bool AttemptRebuildResourceMetadata(const NativeApkLaunchReport& report,
                                    WorkingHealthState* state,
                                    std::vector<std::string>* errors) {
  const auto asset_session = BuildAssetBridgeSession(report);
  const auto resource_report = asset_session.InspectResources();
  if (!resource_report.ready) {
    for (const auto& error : resource_report.errors) {
      AppendError(errors, error);
    }
    return false;
  }
  state->resource_ready = true;
  return true;
}

bool AttemptRepairAppStorage(const NativeApkLaunchReport& report,
                             const fs::path& artifact_root,
                             WorkingHealthState* state,
                             std::vector<std::string>* errors) {
  const auto storage_report =
      BuildStorageSession(report, artifact_root / "storage-repair")
          .RunStorageProof();
  for (const auto& error : storage_report.errors) {
    AppendError(errors, error);
  }
  state->storage_ready = storage_report.ready;
  state->sandbox_ready =
      storage_report.ready &&
      storage_report.isolation_level == "path_sandbox_only";
  return state->storage_ready && state->sandbox_ready;
}

bool AttemptRebuildPermissionState(const NativeApkLaunchReport& report,
                                   const fs::path& artifact_root,
                                   WorkingHealthState* state,
                                   std::vector<std::string>* errors) {
  const std::string manifest_contents = ReadTextFile(report.manifest_path);
  if (manifest_contents.empty()) {
    AppendError(errors, "permission_manifest_contents_unavailable");
    state->recoverable = false;
    return false;
  }
  const auto permission_session = BuildPermissionSession(
      report, artifact_root / "permissions-rebuild", manifest_contents);
  const auto permissions_report = permission_session.BuildPermissionsReport();
  const auto app_ops_report =
      permission_session.BuildAppOpsReport(permissions_report);
  for (const auto& error : permissions_report.errors) {
    AppendError(errors, error);
  }
  for (const auto& error : app_ops_report.errors) {
    AppendError(errors, error);
  }
  state->permission_ready = permissions_report.ready;
  state->app_ops_ready = app_ops_report.ready;
  return state->permission_ready && state->app_ops_ready;
}

bool AttemptRebuildProcessManagerState(const NativeApkLaunchReport& report,
                                       WorkingHealthState* state,
                                       std::vector<std::string>* errors) {
  const auto process_session = BuildProcessSession(report, *state);
  const auto activity_manager = process_session.BuildActivityManagerReport();
  const auto process_manager =
      process_session.BuildProcessManagerReport(activity_manager);
  for (const auto& error : activity_manager.errors) {
    AppendError(errors, error);
  }
  for (const auto& error : process_manager.errors) {
    AppendError(errors, error);
  }
  state->activity_manager_ready = activity_manager.ready;
  state->process_ready = process_manager.ready;
  return state->activity_manager_ready && state->process_ready;
}

bool AttemptRebuildWindowManagerState(const NativeApkLaunchReport& report,
                                      WorkingHealthState* state,
                                      std::vector<std::string>* errors) {
  const auto window_manager = BuildWindowSession(report, *state).BuildReport();
  for (const auto& error : window_manager.errors) {
    AppendError(errors, error);
  }
  state->window_ready = window_manager.ready;
  return state->window_ready;
}

bool AttemptRetryRuntimeBootstrap(const NativeApkLaunchReport& report,
                                  WorkingHealthState* state,
                                  std::vector<std::string>* errors) {
  const auto runtime_bridge = BuildRuntimeSession(
                                  report, *state, true,
                                  false)
                                  .BuildReport();
  for (const auto& error : runtime_bridge.errors) {
    AppendError(errors, error);
  }
  state->runtime_ready = runtime_bridge.ready;
  return state->runtime_ready;
}

bool AttemptRestartSurface(const NativeApkLaunchReport& report,
                           const fs::path& artifact_root,
                           WorkingHealthState* state,
                           std::vector<std::string>* errors) {
  if (!report.launch_ready) {
    AppendError(errors, "surface_restart_requires_launch_ready_session");
    return false;
  }

  const NativeWindowMetadata metadata{
      .width = report.surface.width > 0 ? report.surface.width : 320,
      .height = report.surface.height > 0 ? report.surface.height : 240,
      .format = report.surface.format > 0 ? report.surface.format : 1,
      .stride = report.surface.width > 0 ? report.surface.width : 320};
  const auto bridge = RunNativeWindowBridgeFixture(
      (artifact_root / "surface-restart").string(), metadata);
  if (!bridge.native_window_bridge_ready) {
    AppendError(errors, bridge.exit_reason);
    return false;
  }
  const auto first_frame = RunHeadlessFirstPixelFixture(
      (artifact_root / "surface-restart").string(), metadata, 0x1ee7c0de);
  if (!first_frame.render_ready) {
    AppendError(errors, first_frame.exit_reason);
    return false;
  }
  state->surface_ready = true;
  return true;
}

bool AttemptRestartLifecycle(const NativeApkLaunchReport& report,
                             const fs::path& artifact_root,
                             WorkingHealthState* state,
                             std::vector<std::string>* errors) {
  const auto bridge_report =
      BuildLifecycleBridgeSession(report, artifact_root / "lifecycle-restart")
          .RunDeterministicProof();
  if (!bridge_report.ready) {
    for (const auto& error : bridge_report.errors) {
      AppendError(errors, error);
    }
    return false;
  }
  state->lifecycle_ready = bridge_report.lifecycle.ready;
  state->looper_ready = bridge_report.looper.ready;
  state->input_ready = bridge_report.input_queue.ready;
  return state->lifecycle_ready && state->looper_ready && state->input_ready;
}

bool AttemptResetInputQueue(const NativeApkLaunchReport& report,
                            const fs::path& artifact_root,
                            WorkingHealthState* state,
                            std::vector<std::string>* errors) {
  const auto bridge_report =
      BuildLifecycleBridgeSession(report, artifact_root / "input-reset")
          .RunDeterministicProof();
  if (!bridge_report.input_queue.ready) {
    for (const auto& error : bridge_report.input_queue.errors) {
      AppendError(errors, error);
    }
    return false;
  }
  state->lifecycle_ready = bridge_report.lifecycle.ready;
  state->looper_ready = bridge_report.looper.ready;
  state->input_ready = bridge_report.input_queue.ready;
  return state->input_ready;
}

bool AttemptRefreshBinderServices(const NativeApkLaunchReport& report,
                                  WorkingHealthState* state,
                                  std::vector<std::string>* errors) {
  const auto binder_report = RunBinderServiceManagerFixture(
      {.package_name = report.package_name,
       .launcher_component = report.launcher_component,
       .apk_path = report.bundle_apk_path,
       .artifact_root = report.staged_dir,
       .session_id =
           report.package_name + ":" + report.install_id + ":binder-refresh",
       .owner_process_identity =
           "linuxoid-self-heal:" + report.package_name});
  if (!binder_report.manager_ready) {
    AppendError(errors, binder_report.exit_reason);
    return false;
  }
  state->binder_ready = true;
  return true;
}

bool AttemptRebuildDexBootstrap(const NativeApkLaunchReport& report,
                                const fs::path& artifact_root,
                                WorkingHealthState* state,
                                std::vector<std::string>* errors) {
  OpenedApkArchive archive;
  try {
    archive = OpenApkArchive(report.apk_path);
  } catch (const std::exception& error) {
    AppendError(errors, "dex_archive_unavailable:" + std::string(error.what()));
    return false;
  }

  const auto dex_session = BuildDexBridgeSession(report, artifact_root / "art");
  const auto dex_report = dex_session.RunDexProof(archive);
  const auto art_report = dex_session.BuildArtBootstrap(dex_report);
  for (const auto& error : dex_report.errors) {
    AppendError(errors, error);
  }
  for (const auto& error : art_report.errors) {
    AppendError(errors, error);
  }
  state->dex_ready = dex_report.ready;
  state->art_ready = art_report.ready;
  return state->dex_ready && state->art_ready;
}

bool AttemptRerunIntentResolution(const NativeApkLaunchReport& report,
                                  const fs::path& artifact_root,
                                  WorkingHealthState* state,
                                  std::vector<std::string>* errors) {
  const std::string manifest_contents = ReadTextFile(report.manifest_path);
  if (manifest_contents.empty()) {
    AppendError(errors, "manifest_contents_unavailable");
    state->recoverable = false;
    return false;
  }

  const auto activity_session =
      BuildActivitySession(report, artifact_root / "activity", manifest_contents);
  const auto package_manager = activity_session.BuildPackageManagerRecord();
  const auto intent_resolution =
      activity_session.ResolveActivityIntent(package_manager);
  const auto activity_launch = activity_session.BuildActivityLaunchRecord(
      package_manager, intent_resolution);

  for (const auto& error : package_manager.errors) {
    AppendError(errors, error);
  }
  for (const auto& error : intent_resolution.errors) {
    AppendError(errors, error);
  }
  for (const auto& error : activity_launch.errors) {
    AppendError(errors, error);
  }

  state->package_manager_ready = package_manager.ready;
  state->intent_resolution_ready = intent_resolution.ready;
  state->activity_launch_ready = activity_launch.ready;

  if (package_manager.ready && intent_resolution.ready && activity_launch.ready) {
    return true;
  }

  if (intent_resolution.blocking_reason == "package_not_found" ||
      intent_resolution.blocking_reason == "no_launcher_activity" ||
      intent_resolution.blocking_reason == "ambiguous_launcher_activities" ||
      intent_resolution.blocking_reason == "component_disabled" ||
      intent_resolution.blocking_reason == "component_not_exported" ||
      intent_resolution.blocking_reason == "unsupported_component_type" ||
      intent_resolution.blocking_reason == "unresolved_activity_class") {
    state->recoverable = false;
  }
  return false;
}

bool AttemptSafeModeLaunch(const NativeApkLaunchReport& report,
                           WorkingHealthState* state,
                           std::vector<std::string>* errors) {
  if (!report.launch_ready) {
    AppendError(errors, "safe_mode_launch_not_available");
    return false;
  }
  state->launch_ready = true;
  return true;
}

void WriteRecoveryJournal(const SelfHealingAndroidDeviceReport& report) {
  if (report.journal_path.empty()) {
    return;
  }
  std::ostringstream output;
  for (const auto& action : report.actions) {
    output << "{"
           << "\"sequence_id\": " << action.sequence_id << ", "
           << "\"subsystem\": \"" << EscapeJson(action.subsystem) << "\", "
           << "\"reason\": \"" << EscapeJson(action.reason) << "\", "
           << "\"action\": \"" << EscapeJson(action.action) << "\", "
           << "\"result\": \"" << EscapeJson(action.result) << "\", "
           << "\"recoverable\": " << (action.recoverable ? "true" : "false")
           << ", "
           << "\"initial_health\": \"" << EscapeJson(action.initial_health)
           << "\", "
           << "\"final_health\": \"" << EscapeJson(action.final_health)
           << "\"}\n";
  }
  WriteTextFile(report.journal_path, output.str());
}

void WriteRecoveryReport(const SelfHealingAndroidDeviceReport& report) {
  if (report.report_json_path.empty()) {
    return;
  }
  std::ostringstream output;
  output << "{\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"phase_name\": \"" << EscapeJson(report.phase_name)
         << "\",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id)
         << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"report_json_path\": \"" << EscapeJson(report.report_json_path)
         << "\",\n"
         << "  \"journal_path\": \"" << EscapeJson(report.journal_path)
         << "\",\n"
         << "  \"initial_health\": \"" << EscapeJson(report.initial_health)
         << "\",\n"
         << "  \"final_health\": \"" << EscapeJson(report.final_health)
         << "\",\n"
         << "  \"primary_blocker_reason\": \""
         << EscapeJson(report.primary_blocker_reason) << "\",\n"
         << "  \"recovery_gating_state\": \""
         << EscapeJson(report.recovery_gating_state) << "\",\n"
         << "  \"recovery_gating_reason\": \""
         << EscapeJson(report.recovery_gating_reason) << "\",\n"
         << "  \"storage_health\": \"" << EscapeJson(report.storage_health)
         << "\",\n"
         << "  \"sandbox_health\": \"" << EscapeJson(report.sandbox_health)
         << "\",\n"
         << "  \"permission_health\": \""
         << EscapeJson(report.permission_health) << "\",\n"
         << "  \"app_ops_health\": \"" << EscapeJson(report.app_ops_health)
         << "\",\n"
         << "  \"activity_manager_health\": \""
         << EscapeJson(report.activity_manager_health) << "\",\n"
         << "  \"process_health\": \"" << EscapeJson(report.process_health)
         << "\",\n"
         << "  \"window_health\": \"" << EscapeJson(report.window_health)
         << "\",\n"
         << "  \"runtime_health\": \"" << EscapeJson(report.runtime_health)
         << "\",\n"
         << "  \"recoverable\": " << (report.recoverable ? "true" : "false")
         << ",\n"
         << "  \"actions_attempted\": " << report.actions_attempted << ",\n"
         << "  \"actions_succeeded\": " << report.actions_succeeded << ",\n"
         << "  \"actions_failed\": " << report.actions_failed << ",\n"
         << "  \"recommended_next_action\": \""
         << EscapeJson(report.recommended_next_action) << "\",\n"
         << "  \"actions\": " << RenderRecoveryActionsArray(report.actions)
         << ",\n"
         << "  \"limitation_flags\": "
         << RenderJsonArray(report.limitation_flags) << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  WriteTextFile(report.report_json_path, output.str());
}

}  // namespace

SelfHealingAndroidDeviceWatchdog::SelfHealingAndroidDeviceWatchdog(
    const NativeApkLaunchReport& report)
    : report_(report) {}

SelfHealingAndroidDeviceReport SelfHealingAndroidDeviceWatchdog::Run() const {
  SelfHealingAndroidDeviceReport watchdog;
  watchdog.session_id =
      report_.package_name.empty() || report_.install_id.empty()
          ? ""
          : report_.package_name + ":" + report_.install_id + ":self-heal";
  watchdog.limitation_flags = {
      "self_healing_android_device_local_watchdog_only",
      "linuxoid_session_contract_only",
      "full_android_framework_recovery_not_implemented"};

  if (report_.staged_dir.empty() || report_.package_name.empty()) {
    watchdog.ready = false;
    watchdog.initial_health = "failed";
    watchdog.final_health = "failed";
    watchdog.recoverable = false;
    watchdog.recommended_next_action =
        report_.recommended_recovery_action.empty()
            ? "provide_valid_apk_archive"
            : report_.recommended_recovery_action;
    AppendError(&watchdog.errors, "self_heal_session_not_materialized");
    return watchdog;
  }

  watchdog.ready = true;
  watchdog.artifact_root =
      (fs::path(report_.staged_dir) / "self-healing-android-device").string();
  watchdog.report_json_path =
      (fs::path(watchdog.artifact_root) / "watchdog-report.json").string();
  watchdog.journal_path =
      (fs::path(watchdog.artifact_root) / "recovery-journal.jsonl").string();
  fs::create_directories(watchdog.artifact_root);

  WorkingHealthState state = BuildWorkingHealthState(report_);
  watchdog.initial_health = ClassifyHealth(report_, state);
  watchdog.primary_blocker_reason =
      DeterminePrimaryBlockerReason(report_, state);
  watchdog.storage_health = report_.storage_health;
  watchdog.sandbox_health = report_.sandbox_health;
  watchdog.permission_health = report_.permission_health;
  watchdog.app_ops_health = report_.app_ops_health;
  watchdog.activity_manager_health = report_.activity_manager_health;
  watchdog.process_health = report_.process_health;
  watchdog.window_health = report_.window_health;
  watchdog.runtime_health = report_.runtime_health;

  auto attempt_action = [&](const std::string& subsystem,
                            const std::string& reason,
                            const std::string& action_name,
                            auto&& callback) {
    SelfHealingAndroidDeviceRecoveryAction action;
    action.sequence_id = watchdog.actions.size() + 1;
    action.subsystem = subsystem;
    action.reason = reason;
    action.action = action_name;
    action.initial_health = ClassifyHealth(report_, state);

    std::vector<std::string> action_errors;
    const bool success = callback(&action_errors);
    action.result = success ? "attempted_succeeded" : "attempted_failed";
    action.recoverable = state.recoverable;
    action.final_health = ClassifyHealth(report_, state);
    for (const auto& error : action_errors) {
      AppendError(&watchdog.errors, error);
    }
    watchdog.actions.push_back(std::move(action));
    ++watchdog.actions_attempted;
    if (success) {
      ++watchdog.actions_succeeded;
    } else {
      ++watchdog.actions_failed;
    }
  };

  auto record_skipped_action = [&](const std::string& subsystem,
                                   const std::string& reason,
                                   const std::string& action_name,
                                   const std::string& result) {
    watchdog.actions.push_back(
        {.sequence_id = watchdog.actions.size() + 1,
         .subsystem = subsystem,
         .reason = reason,
         .action = action_name,
         .result = result,
         .recoverable = state.recoverable,
         .initial_health = ClassifyHealth(report_, state),
         .final_health = ClassifyHealth(report_, state)});
  };

  if (!AllContractsReady(report_, state)) {
    if (report_.permissions_proof_requested &&
        (!state.permission_ready || !state.app_ops_ready)) {
      attempt_action("permission_state", "permission_or_appops_blocked",
                     "rebuild_permission_state",
                     [&](std::vector<std::string>* errors) {
                       return AttemptRebuildPermissionState(
                           report_, fs::path(watchdog.artifact_root), &state,
                           errors);
                    });
    }
    if (report_.storage_proof_requested &&
        (!state.storage_ready || !state.sandbox_ready)) {
      attempt_action("app_storage", "storage_or_sandbox_blocked",
                     "repair_app_storage",
                     [&](std::vector<std::string>* errors) {
                       return AttemptRepairAppStorage(
                           report_, fs::path(watchdog.artifact_root), &state,
                           errors);
                     });
    }
    if (report_.asset_proof_requested && !state.asset_ready) {
      attempt_action("asset_bridge", "asset_health_blocked", "restage_assets",
                     [&](std::vector<std::string>* errors) {
                       return AttemptRestageAssets(report_, &state, errors);
                     });
    }
    if (report_.asset_proof_requested && !state.resource_ready) {
      attempt_action("resource_bridge", "resource_health_blocked",
                     "rebuild_resource_metadata",
                     [&](std::vector<std::string>* errors) {
                       return AttemptRebuildResourceMetadata(report_, &state,
                                                             errors);
                     });
    }
    const bool gate_launch_dependent_repairs =
        HasUpstreamNativeLaunchBlocker(report_) && !state.launch_ready;
    if (gate_launch_dependent_repairs) {
      watchdog.recovery_gating_state = "upstream_native_blocker_gated";
      watchdog.recovery_gating_reason =
          DescribeUpstreamNativeLaunchBlocker(report_);
      if (report_.surface_proof_requested && !state.surface_ready) {
        record_skipped_action("surface", "surface_health_blocked",
                              "restart_surface", "skipped_upstream_blocker");
      }
      if (report_.lifecycle_proof_requested &&
          (!state.lifecycle_ready || !state.looper_ready)) {
        record_skipped_action("lifecycle", "lifecycle_or_looper_blocked",
                              "restart_lifecycle", "skipped_upstream_blocker");
      }
      if (report_.lifecycle_proof_requested && !state.input_ready) {
        record_skipped_action("input_queue", "input_health_blocked",
                              "reset_input_queue", "skipped_upstream_blocker");
      }
      if (report_.activity_proof_requested && !state.binder_ready) {
        record_skipped_action("binder_service_registry",
                              "binder_health_blocked",
                              "refresh_binder_services",
                              "skipped_upstream_blocker");
      }
      if (report_.dex_proof_requested && (!state.dex_ready || !state.art_ready)) {
        record_skipped_action("dex_art_bootstrap",
                              "dex_or_art_health_blocked",
                              "rebuild_dex_bootstrap",
                              "skipped_upstream_blocker");
      }
      if (report_.activity_proof_requested &&
          (!state.package_manager_ready || !state.intent_resolution_ready ||
           !state.activity_launch_ready)) {
        record_skipped_action("intent_resolution",
                              "activity_launch_contract_blocked",
                              "rerun_intent_resolution",
                              "skipped_upstream_blocker");
      }
      if (report_.process_proof_requested &&
          (!state.activity_manager_ready || !state.process_ready)) {
        record_skipped_action("process_manager",
                              "process_manager_contract_blocked",
                              "rebuild_process_manager_state",
                              "skipped_upstream_blocker");
      }
      if (report_.window_proof_requested && !state.window_ready) {
        record_skipped_action("window_manager", "window_health_blocked",
                              "rebuild_window_manager_state",
                              "skipped_upstream_blocker");
      }
      if (report_.runtime_proof_requested && !state.runtime_ready) {
        record_skipped_action("art_runtime_bridge", "runtime_health_blocked",
                              "retry_runtime_bootstrap",
                              "skipped_upstream_blocker");
      }
      if (!state.launch_ready && watchdog.actions.empty()) {
        record_skipped_action("launch", "launch_not_ready", "safe_mode_launch",
                              "skipped_upstream_blocker");
      }
    } else {
      if (report_.surface_proof_requested && !state.surface_ready) {
        attempt_action("surface", "surface_health_blocked", "restart_surface",
                       [&](std::vector<std::string>* errors) {
                         return AttemptRestartSurface(
                             report_, fs::path(watchdog.artifact_root), &state,
                             errors);
                       });
      }
      if (report_.lifecycle_proof_requested &&
          (!state.lifecycle_ready || !state.looper_ready)) {
        attempt_action("lifecycle", "lifecycle_or_looper_blocked",
                       "restart_lifecycle",
                       [&](std::vector<std::string>* errors) {
                         return AttemptRestartLifecycle(
                             report_, fs::path(watchdog.artifact_root), &state,
                             errors);
                       });
      }
      if (report_.lifecycle_proof_requested && !state.input_ready) {
        attempt_action("input_queue", "input_health_blocked",
                       "reset_input_queue",
                       [&](std::vector<std::string>* errors) {
                         return AttemptResetInputQueue(
                             report_, fs::path(watchdog.artifact_root), &state,
                             errors);
                       });
      }
      if (report_.activity_proof_requested && !state.binder_ready) {
        attempt_action("binder_service_registry", "binder_health_blocked",
                       "refresh_binder_services",
                       [&](std::vector<std::string>* errors) {
                         return AttemptRefreshBinderServices(report_, &state,
                                                             errors);
                       });
      }
      if (report_.dex_proof_requested &&
          (!state.dex_ready || !state.art_ready)) {
        attempt_action("dex_art_bootstrap", "dex_or_art_health_blocked",
                       "rebuild_dex_bootstrap",
                       [&](std::vector<std::string>* errors) {
                         return AttemptRebuildDexBootstrap(
                             report_, fs::path(watchdog.artifact_root), &state,
                             errors);
                       });
      }
      if (report_.activity_proof_requested &&
          (!state.package_manager_ready || !state.intent_resolution_ready ||
           !state.activity_launch_ready)) {
        attempt_action("intent_resolution", "activity_launch_contract_blocked",
                       "rerun_intent_resolution",
                       [&](std::vector<std::string>* errors) {
                         return AttemptRerunIntentResolution(
                             report_, fs::path(watchdog.artifact_root), &state,
                             errors);
                       });
      }
      if (report_.process_proof_requested &&
          (!state.activity_manager_ready || !state.process_ready)) {
        attempt_action("process_manager", "process_manager_contract_blocked",
                       "rebuild_process_manager_state",
                       [&](std::vector<std::string>* errors) {
                         return AttemptRebuildProcessManagerState(
                             report_, &state, errors);
                       });
      }
      if (report_.window_proof_requested && !state.window_ready) {
        attempt_action("window_manager", "window_health_blocked",
                       "rebuild_window_manager_state",
                       [&](std::vector<std::string>* errors) {
                         return AttemptRebuildWindowManagerState(report_, &state,
                                                                 errors);
                       });
      }
      if (report_.runtime_proof_requested && !state.runtime_ready) {
        attempt_action("art_runtime_bridge", "runtime_health_blocked",
                       "retry_runtime_bootstrap",
                       [&](std::vector<std::string>* errors) {
                         return AttemptRetryRuntimeBootstrap(report_, &state,
                                                             errors);
                       });
      }
      if (!state.launch_ready && watchdog.actions.empty()) {
        attempt_action("launch", "launch_not_ready", "safe_mode_launch",
                       [&](std::vector<std::string>* errors) {
                         return AttemptSafeModeLaunch(report_, &state, errors);
                       });
      }
    }
  } else {
    watchdog.actions.push_back(
        {.sequence_id = 1,
         .subsystem = "watchdog",
         .reason = "all_subsystems_ready",
         .action = "no_action",
         .result = "skipped",
         .recoverable = false,
         .initial_health = watchdog.initial_health,
         .final_health = "healthy"});
  }

  const std::string raw_final_health = ClassifyHealth(report_, state);
  watchdog.final_health =
      (watchdog.initial_health != "healthy" &&
       raw_final_health == "healthy")
          ? "recovered"
          : raw_final_health;
  watchdog.storage_health = state.storage_ready ? "ready" : report_.storage_health;
  watchdog.sandbox_health = state.sandbox_ready ? "ready" : report_.sandbox_health;
  watchdog.permission_health =
      state.permission_ready ? "ready" : report_.permission_health;
  watchdog.app_ops_health = state.app_ops_ready ? "ready" : report_.app_ops_health;
  watchdog.activity_manager_health =
      state.activity_manager_ready ? "ready"
                                   : report_.activity_manager_health;
  watchdog.process_health =
      state.process_ready ? "ready" : report_.process_health;
  watchdog.window_health =
      state.window_ready ? "ready" : report_.window_health;
  watchdog.runtime_health =
      state.runtime_ready ? "ready" : report_.runtime_health;
  watchdog.recoverable = state.recoverable;
  watchdog.primary_blocker_reason =
      DeterminePrimaryBlockerReason(report_, state);
  watchdog.recommended_next_action =
      DetermineRecommendedNextAction(report_, state);

  WriteRecoveryJournal(watchdog);
  WriteRecoveryReport(watchdog);
  return watchdog;
}

}  // namespace wfa
