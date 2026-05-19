#ifndef WFA_APK_WINDOW_BRIDGE_HPP
#define WFA_APK_WINDOW_BRIDGE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wfa {

struct NativeApkWindowManagerContext {
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
  std::string native_loading_state = "not_requested";
  std::string native_jni_state = "not_requested";
  std::string native_loading_library_name;
  std::string native_loading_detail;
  std::string native_post_dispatch_state = "not_reached";
  std::string native_post_dispatch_blocker = "none";
  std::string native_post_dispatch_recovery_action = "none";
  std::string native_post_dispatch_backend = "none";
  bool launch_ready = false;
  bool recoverable = false;
  std::string launcher_component;
  std::string resolved_component;
  std::string activity_launch_status = "not_requested";
  std::string activity_launch_blocking_reason = "none";
  std::string activity_launch_recovery_action = "none";
  std::string lifecycle_state = "not_requested";
  std::string process_identity;
  std::string process_name;
  int pid_value = 0;
  std::string pid_source = "linuxoid_placeholder";
  std::string surface_session_id;
  std::string surface_session_root;
  std::string surface_metadata_path;
  std::string surface_event_log_path;
  std::string surface_marker_path;
  std::string surface_state = "not_requested";
  std::string surface_backend = "headless";
  std::string surface_backing_mode = "headless_fallback";
  int surface_width = 0;
  int surface_height = 0;
  int surface_format = 0;
  bool surface_first_frame_presented = false;
  bool surface_created = false;
  bool wayland_surface_available = false;
  bool egl_surface_available = false;
  bool surface_recovered = false;
  std::string storage_health = "not_requested";
  std::string sandbox_health = "not_requested";
  std::string permission_health = "not_requested";
  std::string app_ops_health = "not_requested";
  std::string binder_health = "not_requested";
  std::string surface_health = "not_requested";
  std::string lifecycle_health = "not_requested";
  std::string looper_health = "not_requested";
  std::string input_health = "not_requested";
  bool input_focus_owned = false;
  std::string input_focus_owner;
  std::size_t pointer_events_injected = 0;
  std::size_t key_events_injected = 0;
  std::string dex_health = "not_requested";
  std::string art_health = "not_requested";
  std::string activity_health = "not_requested";
  std::string activity_manager_health = "not_requested";
  std::string process_health = "not_requested";
  bool activity_manager_ready = false;
  bool process_ready = false;
  bool persisted_artifact_root_preexisting = false;
  bool allow_persisted_contract_repair = true;
};

struct NativeApkWindowManagerReport {
  bool ready = false;
  bool contract_ready = false;
  std::string schema_version = "linuxoid.window_manager.contract.v1";
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
  std::string window_id;
  std::string process_identity;
  std::string process_name;
  int pid_value = 0;
  std::string pid_source = "linuxoid_placeholder";
  std::string launch_component;
  std::string activity_component;
  std::string lifecycle_state = "not_requested";
  std::string surface_state = "not_requested";
  std::string backend = "headless";
  std::string backing_mode = "headless_fallback";
  bool headless_safe = true;
  bool wayland_surface_available = false;
  bool egl_surface_available = false;
  int width = 0;
  int height = 0;
  int format = 0;
  std::string surface_session_id;
  std::string surface_session_root;
  std::string surface_metadata_path;
  std::string surface_event_log_path;
  std::string marker_path;
  bool surface_created = false;
  bool attached = false;
  bool visible = false;
  bool hidden = false;
  bool resized = false;
  bool destroyed = false;
  bool failed = false;
  bool recovered = false;
  std::string window_state = "window_manager_dependency_blocked";
  std::string visible_target_state = "not_requested";
  std::string focus_state = "not_requested";
  bool focus_owned = false;
  std::string focus_owner;
  std::size_t pointer_events_injected = 0;
  std::size_t key_events_injected = 0;
  std::string interaction_state = "not_requested";
  std::string interaction_target_component;
  std::string interaction_target_window_id;
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

class NativeApkWindowManagerSession {
 public:
  explicit NativeApkWindowManagerSession(
      NativeApkWindowManagerContext context);

  const NativeApkWindowManagerContext& context() const;
  NativeApkWindowManagerReport BuildReport() const;

 private:
  NativeApkWindowManagerContext context_;
};

}  // namespace wfa

#endif  // WFA_APK_WINDOW_BRIDGE_HPP
