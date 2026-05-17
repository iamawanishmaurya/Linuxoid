#ifndef WFA_APK_PROCESS_BRIDGE_HPP
#define WFA_APK_PROCESS_BRIDGE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wfa {

struct NativeApkProcessManagerContext {
  std::string session_id;
  std::string package_name;
  std::string requested_package_name;
  std::string requested_component;
  std::string apk_path;
  std::string staged_dir;
  std::string sandbox_root;
  std::string app_data_dir;
  std::string artifact_root;
  std::string install_id;
  std::string version_name;
  int version_code = 0;
  int user_id = 0;
  int app_id = 10000;
  int uid_placeholder = 10000;
  int gid_placeholder = 10000;
  std::string launch_status;
  bool launch_ready = false;
  bool recoverable = false;
  std::string launcher_component;
  std::string resolved_component;
  std::string resolution_status = "not_requested";
  std::string resolution_mode = "main_launcher";
  std::string resolution_reason;
  std::string resolution_blocking_reason = "none";
  std::string resolution_recovery_action = "none";
  std::string activity_launch_status = "not_requested";
  std::string activity_launch_blocking_reason = "none";
  std::string activity_launch_recovery_action = "none";
  std::string intent_action = "android.intent.action.MAIN";
  std::vector<std::string> intent_categories = {
      "android.intent.category.LAUNCHER"};
  std::string lifecycle_state = "not_requested";
  std::string surface_health = "not_requested";
  std::string lifecycle_health = "not_requested";
  std::string looper_health = "not_requested";
  std::string input_health = "not_requested";
  std::string dex_health = "not_requested";
  std::string art_health = "not_requested";
  std::string binder_health = "not_requested";
  std::string activity_health = "not_requested";
  std::string storage_health = "not_requested";
  std::string sandbox_health = "not_requested";
  std::string permission_health = "not_requested";
  std::string app_ops_health = "not_requested";
  bool package_manager_ready = false;
  bool intent_resolution_ready = false;
  bool activity_launch_ready = false;
  bool dex_bootstrap_ready = false;
  bool art_runtime_available = false;
  bool java_execution_supported = false;
  bool persisted_artifact_root_preexisting = false;
  bool allow_persisted_contract_repair = true;
};

struct NativeApkActivityManagerReport {
  bool ready = false;
  bool contract_ready = false;
  std::string schema_version = "linuxoid.activity_manager.contract.v1";
  std::string session_id;
  std::string artifact_root;
  std::string report_json_path;
  std::string package_name;
  int user_id = 0;
  int app_id = 10000;
  std::string sandbox_root;
  std::string app_data_dir;
  std::string apk_path;
  std::string staged_dir;
  std::uint64_t updated_at_unix_ms = 0;
  std::string requested_package_name;
  std::string requested_component;
  std::string resolved_component;
  std::string launch_component;
  std::string process_name;
  std::string start_reason;
  std::string resolution_mode = "main_launcher";
  std::string intent_action = "android.intent.action.MAIN";
  std::vector<std::string> intent_categories = {
      "android.intent.category.LAUNCHER"};
  std::string resolution_status = "not_requested";
  std::string resolution_reason;
  std::string launch_state = "activity_manager_dependency_blocked";
  std::string lifecycle_state = "not_requested";
  std::string blocking_reason = "none";
  std::string recommended_recovery_action = "none";
  std::string restart_policy = "restart_on_contract_failure";
  std::string termination_policy = "graceful_placeholder";
  std::string storage_health = "not_requested";
  std::string sandbox_health = "not_requested";
  std::string permission_health = "not_requested";
  std::string app_ops_health = "not_requested";
  std::string binder_health = "not_requested";
  std::string surface_health = "not_requested";
  std::string lifecycle_health = "not_requested";
  std::string looper_health = "not_requested";
  std::string input_health = "not_requested";
  std::string dex_health = "not_requested";
  std::string art_health = "not_requested";
  std::vector<std::string> healing_actions;
  std::vector<std::string> diagnostics;
  std::vector<std::string> errors;
};

struct NativeApkProcessManagerReport {
  bool ready = false;
  bool contract_ready = false;
  std::string schema_version = "linuxoid.process_manager.contract.v1";
  std::string session_id;
  std::string artifact_root;
  std::string report_json_path;
  std::string package_name;
  int user_id = 0;
  int app_id = 10000;
  int uid_placeholder = 10000;
  int gid_placeholder = 10000;
  std::string sandbox_root;
  std::string app_data_dir;
  std::string apk_path;
  std::string staged_dir;
  std::uint64_t updated_at_unix_ms = 0;
  std::string process_identity;
  std::string process_name;
  int pid_value = 0;
  std::string pid_source = "linuxoid_placeholder";
  std::string launch_component;
  std::string start_reason;
  std::string process_state = "process_manager_dependency_blocked";
  std::string lifecycle_state = "not_requested";
  std::string restart_policy = "restart_on_contract_failure";
  std::string termination_policy = "graceful_placeholder";
  std::string intent_action = "android.intent.action.MAIN";
  std::vector<std::string> intent_categories = {
      "android.intent.category.LAUNCHER"};
  std::string blocking_reason = "none";
  std::string recommended_recovery_action = "none";
  std::vector<std::string> dependency_details;
  std::vector<std::string> healing_actions;
  std::vector<std::string> diagnostics;
  std::vector<std::string> errors;
};

class NativeApkProcessManagerSession {
 public:
  explicit NativeApkProcessManagerSession(
      NativeApkProcessManagerContext context);

  const NativeApkProcessManagerContext& context() const;
  NativeApkActivityManagerReport BuildActivityManagerReport() const;
  NativeApkProcessManagerReport BuildProcessManagerReport(
      const NativeApkActivityManagerReport& activity_manager) const;

 private:
  NativeApkProcessManagerContext context_;
};

}  // namespace wfa

#endif  // WFA_APK_PROCESS_BRIDGE_HPP
