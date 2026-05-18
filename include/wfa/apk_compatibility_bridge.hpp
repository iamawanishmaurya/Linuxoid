#ifndef WFA_APK_COMPATIBILITY_BRIDGE_HPP
#define WFA_APK_COMPATIBILITY_BRIDGE_HPP

#include "wfa/apk_native_launch.hpp"

#include <string>
#include <vector>

namespace wfa {

struct NativeApkCompatibilityOptions {
  std::string staging_root = "/tmp/linuxoid-apk-compatibility";
  std::string requested_package_name;
  std::string requested_component;
  bool simulate_missing_asset_bridge = false;
  bool simulate_blocked_surface_proof = false;
  bool simulate_missing_binder_service = false;
  bool simulate_failed_dex_bootstrap = false;
  bool simulate_failed_intent_resolution = false;
  bool simulate_storage_failure = false;
  bool simulate_permission_mismatch = false;
  bool simulate_failed_runtime_bootstrap = false;
};

struct NativeApkCompatibilityDomainReport {
  std::string domain_name;
  std::string status = "blocked";
  bool ready = false;
  std::string blocking_reason = "none";
  std::string recommended_recovery_action = "none";
  std::vector<std::string> diagnostics;
};

struct NativeApkCompatibilityReport {
  bool ready = false;
  bool contract_ready = false;
  std::string schema_version = "linuxoid.apk_compatibility.contract.v1";
  std::string fixture_label;
  std::string package_name;
  std::string requested_package_name;
  std::string requested_component;
  std::string apk_path;
  std::string install_id;
  std::string launcher_component;
  std::string resolved_component;
  std::string process_identity;
  std::string process_name;
  int pid_value = 0;
  std::string pid_source = "linuxoid_placeholder";
  std::string surface_session_id;
  std::string window_id;
  std::string runtime_session_id;
  std::string runtime_handle;
  std::string overall_status = "blocked";
  std::string blocking_reason = "none";
  std::string recommended_recovery_action = "none";
  bool recoverable = false;
  bool self_healing_ready = false;
  std::string self_healing_final_health = "not_requested";
  std::string artifact_root;
  std::string report_json_path;
  std::string domains_json_path;
  std::string event_log_path;
  std::string launch_report_json_path;
  std::string self_healing_journal_path;
  std::vector<std::string> healing_actions;
  std::vector<std::string> diagnostics;
  std::vector<std::string> errors;
  std::vector<NativeApkCompatibilityDomainReport> domains;
  NativeApkLaunchReport launch_report;
};

struct NativeApkCompatibilitySuiteEntry {
  std::string fixture_label;
  std::string package_name;
  std::string apk_path;
  std::string overall_status = "blocked";
  std::string blocking_reason = "none";
  std::string recommended_recovery_action = "none";
  bool recoverable = false;
  std::string report_json_path;
  std::string launch_report_json_path;
  std::string self_healing_journal_path;
};

struct NativeApkCompatibilitySuiteReport {
  bool ready = false;
  std::string schema_version =
      "linuxoid.apk_compatibility_suite.contract.v1";
  std::string suite_root;
  std::string report_json_path;
  int total_entries = 0;
  int supported_count = 0;
  int partial_count = 0;
  int blocked_count = 0;
  int missing_runtime_count = 0;
  int missing_surface_count = 0;
  int missing_native_lib_count = 0;
  int needs_real_art_count = 0;
  int recovered_count = 0;
  int degraded_count = 0;
  std::vector<std::string> errors;
  std::vector<NativeApkCompatibilitySuiteEntry> entries;
};

NativeApkCompatibilityReport InspectNativeApkCompatibility(
    const std::string& apk_path,
    const NativeApkCompatibilityOptions& options = {});

NativeApkCompatibilitySuiteReport InspectNativeApkCompatibilitySuite(
    const std::vector<std::string>& apk_paths, const std::string& suite_root,
    const NativeApkCompatibilityOptions& options = {});

std::string RenderNativeApkCompatibilityJson(
    const NativeApkCompatibilityReport& report);

std::string RenderNativeApkCompatibilitySuiteJson(
    const NativeApkCompatibilitySuiteReport& report);

}  // namespace wfa

#endif  // WFA_APK_COMPATIBILITY_BRIDGE_HPP
