#ifndef WFA_APK_JAVA_PROOF_BRIDGE_HPP
#define WFA_APK_JAVA_PROOF_BRIDGE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wfa {

struct NativeApkJavaProofContext {
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
  std::string launcher_component;
  std::string resolved_component;
  std::string process_session_id;
  std::string process_identity;
  std::string process_name;
  int pid_value = 0;
  std::string pid_source = "linuxoid_placeholder";
  std::string window_session_id;
  std::string window_id;
  std::string runtime_session_id;
  std::string runtime_handle;
  std::string runtime_root;
  std::string runtime_discovery_source = "none";
  std::string runtime_bootstrap_state = "unavailable";
  bool package_manager_ready = false;
  bool intent_resolution_ready = false;
  bool activity_launch_ready = false;
  bool process_ready = false;
  bool window_ready = false;
  bool runtime_ready = false;
  bool storage_ready = false;
  bool sandbox_ready = false;
  bool permission_ready = false;
  bool app_ops_ready = false;
  bool surface_ready = false;
  bool lifecycle_ready = false;
  bool dex_ready = false;
  bool art_ready = false;
  int assets_count = 0;
  bool resource_table_present = false;
  std::vector<std::string> dex_files;
  bool art_runtime_available = false;
  bool class_loader_ready = false;
  bool bytecode_execution_ready = false;
  bool java_execution_supported = false;
  bool self_healing_requested = false;
  bool self_healing_ready = false;
  std::string self_healing_initial_health = "not_requested";
  std::string self_healing_final_health = "not_requested";
  std::string self_healing_recommended_next_action = "none";
  bool persisted_artifact_root_preexisting = false;
  bool allow_persisted_contract_repair = true;
};

struct NativeApkJavaProofReport {
  bool ready = false;
  bool contract_ready = false;
  std::string schema_version = "linuxoid.java_apk_proof.contract.v1";
  std::string session_id;
  std::string artifact_root;
  std::string report_json_path;
  std::string session_map_path;
  std::string event_log_path;
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
  std::string proof_mode = "bootstrap_lifecycle_wiring_only";
  std::string launcher_component;
  std::string resolved_component;
  std::string process_session_id;
  std::string process_identity;
  std::string process_name;
  int pid_value = 0;
  std::string pid_source = "linuxoid_placeholder";
  std::string window_session_id;
  std::string window_id;
  std::string runtime_session_id;
  std::string runtime_handle;
  std::string runtime_root;
  std::string runtime_discovery_source = "none";
  std::string runtime_bootstrap_state = "unavailable";
  int assets_count = 0;
  bool resource_table_present = false;
  int dex_files_count = 0;
  bool package_manager_ready = false;
  bool intent_resolution_ready = false;
  bool activity_launch_ready = false;
  bool process_ready = false;
  bool window_ready = false;
  bool runtime_ready = false;
  bool art_runtime_available = false;
  bool class_loader_ready = false;
  bool bytecode_execution_ready = false;
  bool java_execution_supported = false;
  bool self_healing_requested = false;
  bool self_healing_ready = false;
  std::string self_healing_final_health = "not_requested";
  std::string proof_state = "blocked";
  std::string blocking_reason = "none";
  std::string recommended_recovery_action = "none";
  std::vector<std::string> states_visited;
  std::vector<std::string> healing_actions;
  std::vector<std::string> diagnostics;
  std::vector<std::string> errors;
};

class NativeApkJavaProofSession {
 public:
  explicit NativeApkJavaProofSession(NativeApkJavaProofContext context);

  const NativeApkJavaProofContext& context() const;
  NativeApkJavaProofReport BuildReport() const;

 private:
  NativeApkJavaProofContext context_;
};

}  // namespace wfa

#endif  // WFA_APK_JAVA_PROOF_BRIDGE_HPP
