#ifndef WFA_APK_RUNTIME_BRIDGE_HPP
#define WFA_APK_RUNTIME_BRIDGE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wfa {

struct NativeApkRuntimeBridgeContext {
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
  std::string native_post_dispatch_state = "not_reached";
  std::string native_post_dispatch_blocker = "none";
  std::string native_post_dispatch_recovery_action = "none";
  std::string native_post_dispatch_backend = "none";
  std::string launcher_component;
  std::string resolved_component;
  std::string activity_launch_status = "not_requested";
  std::string activity_launch_blocking_reason = "none";
  std::string activity_launch_recovery_action = "none";
  std::string intent_action = "android.intent.action.MAIN";
  std::vector<std::string> intent_categories = {
      "android.intent.category.LAUNCHER"};
  std::string lifecycle_state = "not_requested";
  std::string process_session_id;
  std::string process_identity;
  std::string process_name;
  int pid_value = 0;
  std::string pid_source = "linuxoid_placeholder";
  std::string window_session_id;
  std::string window_id;
  std::string surface_session_id;
  std::string surface_state = "not_requested";
  std::string surface_backend = "headless";
  std::string surface_backing_mode = "headless_fallback";
  bool surface_first_frame_presented = false;
  bool surface_created = false;
  bool wayland_surface_available = false;
  bool egl_surface_available = false;
  std::string runtime_root_override;
  std::string runtime_probe_override;
  bool disable_host_runtime_probe = false;
  std::string storage_health = "not_requested";
  std::string sandbox_health = "not_requested";
  std::string permission_health = "not_requested";
  std::string app_ops_health = "not_requested";
  std::string binder_health = "not_requested";
  std::string surface_health = "not_requested";
  std::string window_health = "not_requested";
  std::string lifecycle_health = "not_requested";
  std::string looper_health = "not_requested";
  std::string input_health = "not_requested";
  std::string dex_health = "not_requested";
  std::string art_health = "not_requested";
  std::string activity_health = "not_requested";
  std::string activity_manager_health = "not_requested";
  std::string process_health = "not_requested";
  bool package_manager_ready = false;
  bool intent_resolution_ready = false;
  bool activity_launch_ready = false;
  bool activity_manager_ready = false;
  bool process_ready = false;
  bool window_ready = false;
  bool dex_bootstrap_ready = false;
  bool class_loader_ready = false;
  bool java_execution_supported = false;
  bool persisted_artifact_root_preexisting = false;
  bool allow_persisted_contract_repair = true;
  bool simulate_bootstrap_failure = false;
  bool bootstrap_recovered = false;
  std::vector<std::string> dex_files;
};

struct NativeApkRuntimeBridgeReport {
  bool ready = false;
  bool contract_ready = false;
  std::string schema_version = "linuxoid.runtime_bridge.contract.v1";
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
  std::string runtime_handle;
  std::string process_session_id;
  std::string process_identity;
  std::string process_name;
  int pid_value = 0;
  std::string pid_source = "linuxoid_placeholder";
  std::string window_session_id;
  std::string window_id;
  std::string activity_component;
  std::string launch_component;
  std::string runtime_root;
  std::string discovery_source = "none";
  std::string boot_classpath;
  std::vector<std::string> boot_classpath_entries;
  std::vector<std::string> native_library_dirs;
  std::vector<std::string> dex_files;
  bool art_runtime_required = true;
  bool art_runtime_available = false;
  bool class_loader_ready = false;
  bool bytecode_execution_ready = false;
  bool java_execution_supported = false;
  bool headless_safe = true;
  bool wayland_surface_available = false;
  bool egl_surface_available = false;
  std::string bootstrap_state = "unavailable";
  std::string native_post_dispatch_state = "not_reached";
  std::string native_post_dispatch_blocker = "none";
  std::string native_post_dispatch_recovery_action = "none";
  std::string native_post_dispatch_backend = "none";
  std::string blocking_reason = "none";
  std::string recommended_recovery_action = "none";
  std::vector<std::string> states_visited;
  std::vector<std::string> healing_actions;
  std::vector<std::string> diagnostics;
  std::vector<std::string> errors;
};

class NativeApkRuntimeBridgeSession {
 public:
  explicit NativeApkRuntimeBridgeSession(NativeApkRuntimeBridgeContext context);

  const NativeApkRuntimeBridgeContext& context() const;
  NativeApkRuntimeBridgeReport BuildReport() const;

 private:
  NativeApkRuntimeBridgeContext context_;
};

}  // namespace wfa

#endif  // WFA_APK_RUNTIME_BRIDGE_HPP
