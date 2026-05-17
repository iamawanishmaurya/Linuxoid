#ifndef WFA_APK_ACTIVITY_LAUNCH_BRIDGE_HPP
#define WFA_APK_ACTIVITY_LAUNCH_BRIDGE_HPP

#include <string>
#include <vector>

namespace wfa {

struct NativeApkIntentFilterRecord {
  std::vector<std::string> actions;
  std::vector<std::string> categories;
};

struct NativeApkActivityComponentRecord {
  std::string component_name;
  std::string class_name;
  std::string component_kind = "activity";
  std::string label;
  std::string exported_source = "unknown";
  std::string target_activity;
  bool exported = false;
  bool enabled = true;
  bool launcher_candidate = false;
  std::vector<NativeApkIntentFilterRecord> intent_filters;
};

struct NativeApkActivityLaunchBridgeContext {
  std::string session_id;
  std::string package_name;
  std::string requested_package_name;
  std::string requested_component;
  std::string apk_path;
  std::string staged_dir;
  std::string install_id;
  std::string version_name;
  int version_code = 0;
  std::string manifest_source;
  std::string manifest_contents;
  std::string launcher_component;
  std::vector<std::string> declared_components;
  std::vector<std::string> declared_activities;
  std::vector<std::string> staged_native_libraries;
  bool launch_ready = false;
  bool recoverable = false;
  bool art_runtime_available = false;
  bool java_execution_supported = false;
  bool dex_bootstrap_ready = false;
  std::string launch_status;
  std::string surface_health = "not_requested";
  std::string lifecycle_health = "not_requested";
  std::string looper_health = "not_requested";
  std::string input_health = "not_requested";
  std::string dex_health = "not_requested";
  std::string art_health = "not_requested";
  std::string binder_health = "not_requested";
  std::string binder_service_registry_status = "not_requested";
  std::string binder_service_manager_path;
  std::vector<std::string> lifecycle_states_visited;
  std::string lifecycle_current_state;
  std::string artifact_root;
};

struct NativeApkPackageManagerReport {
  bool ready = false;
  std::string session_id;
  std::string artifact_root;
  std::string package_record_path;
  std::string package_name;
  std::string requested_package_name;
  std::string apk_path;
  std::string staged_dir;
  std::string install_id;
  std::string version_name;
  int version_code = 0;
  std::string manifest_source;
  std::string package_label;
  std::string launcher_component;
  std::vector<std::string> declared_components;
  std::vector<std::string> declared_activities;
  std::vector<NativeApkActivityComponentRecord> activities;
  std::vector<std::string> staged_native_libraries;
  std::string binder_service_registry_status = "not_requested";
  std::string binder_service_manager_path;
  std::vector<std::string> errors;
};

struct NativeApkIntentResolutionReport {
  bool ready = false;
  std::string session_id;
  std::string artifact_root;
  std::string resolution_json_path;
  std::string package_name;
  std::string requested_package_name;
  std::string requested_component;
  std::string resolution_mode = "main_launcher";
  std::string action = "android.intent.action.MAIN";
  std::vector<std::string> categories = {"android.intent.category.LAUNCHER"};
  std::vector<std::string> candidate_components;
  std::vector<std::string> matched_components;
  std::string resolved_component;
  std::string resolved_activity_class;
  bool launcher_match = false;
  std::string resolution_status;
  std::string resolution_reason;
  std::string blocking_reason = "none";
  std::string recommended_recovery_action = "none";
  std::vector<std::string> errors;
};

struct NativeApkActivityLaunchRecord {
  bool ready = false;
  std::string session_id;
  std::string artifact_root;
  std::string launch_record_path;
  std::string package_name;
  std::string resolved_component;
  std::string activity_launch_status;
  std::string current_state;
  std::string blocking_reason = "none";
  std::string recommended_recovery_action = "none";
  bool dependency_blocked = false;
  bool art_runtime_available = false;
  bool java_execution_supported = false;
  bool dex_bootstrap_ready = false;
  std::string surface_health = "not_requested";
  std::string lifecycle_health = "not_requested";
  std::string looper_health = "not_requested";
  std::string input_health = "not_requested";
  std::string dex_health = "not_requested";
  std::string art_health = "not_requested";
  std::string binder_health = "not_requested";
  std::string binder_service_registry_status = "not_requested";
  std::string binder_service_manager_path;
  std::vector<std::string> states_visited;
  std::vector<std::string> dependency_details;
  std::vector<std::string> errors;
};

class NativeApkActivityLaunchBridgeSession {
 public:
 explicit NativeApkActivityLaunchBridgeSession(
      NativeApkActivityLaunchBridgeContext context);

  const NativeApkActivityLaunchBridgeContext& context() const;
  NativeApkPackageManagerReport BuildPackageManagerRecord() const;
  NativeApkIntentResolutionReport ResolveActivityIntent(
      const NativeApkPackageManagerReport& package_manager) const;
  NativeApkActivityLaunchRecord BuildActivityLaunchRecord(
      const NativeApkPackageManagerReport& package_manager,
      const NativeApkIntentResolutionReport& intent_resolution) const;

 private:
  NativeApkActivityLaunchBridgeContext context_;
};

}  // namespace wfa

#endif  // WFA_APK_ACTIVITY_LAUNCH_BRIDGE_HPP
