#ifndef WFA_APK_NATIVE_LAUNCH_HPP
#define WFA_APK_NATIVE_LAUNCH_HPP

#include "wfa/apk_activity_launch_bridge.hpp"
#include "wfa/apk_asset_bridge.hpp"
#include "wfa/apk_dex_bridge.hpp"
#include "wfa/apk_java_proof_bridge.hpp"
#include "wfa/apk_lifecycle_bridge.hpp"
#include "wfa/apk_process_bridge.hpp"
#include "wfa/apk_runtime_bridge.hpp"
#include "wfa/apk_self_healing_watchdog.hpp"
#include "wfa/apk_storage_bridge.hpp"
#include "wfa/apk_permission_bridge.hpp"
#include "wfa/apk_window_bridge.hpp"
#include "wfa/native_execute_stub.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace wfa {

struct NativeApkLaunchOptions {
  std::string staging_root = "/tmp/linuxoid-apk-launch";
  std::string requested_package_name;
  std::string requested_component;
  int watchdog_seconds = 1;
  bool first_app_start_proof_requested = false;
  bool surface_proof_requested = false;
  bool asset_proof_requested = false;
  bool lifecycle_proof_requested = false;
  bool dex_proof_requested = false;
  bool activity_proof_requested = false;
  bool process_proof_requested = false;
  bool window_proof_requested = false;
  bool runtime_proof_requested = false;
  bool java_proof_requested = false;
  bool storage_proof_requested = false;
  bool permissions_proof_requested = false;
  bool self_heal_proof_requested = false;
  int surface_width = 320;
  int surface_height = 240;
  int surface_format = 1;
  std::string preferred_asset_path;
  bool simulate_missing_asset_bridge = false;
  bool simulate_blocked_surface_proof = false;
  bool simulate_missing_binder_service = false;
  bool simulate_failed_dex_bootstrap = false;
  bool simulate_failed_intent_resolution = false;
  bool simulate_storage_failure = false;
  bool simulate_permission_mismatch = false;
  bool simulate_failed_runtime_bootstrap = false;
};

struct NativeApkSurfaceSession {
  std::string session_id;
  std::string package_name;
  std::string apk_path;
  std::string staged_dir;
  std::string selected_library_path;
  std::string session_root;
  std::string metadata_path;
  std::string event_log_path;
  std::string marker_path;
  std::string backend;
  std::string backing_mode;
  std::string bridge_metadata_path;
  std::string bridge_event_log_path;
  std::string state;
  std::string first_pixel_marker;
  std::string marker_checksum;
  int width = 0;
  int height = 0;
  int format = 0;
  bool surface_created = false;
  bool surface_configured = false;
  bool first_frame_requested = false;
  bool first_frame_presented = false;
  bool cleanup_ready = false;
  bool wayland_surface_available = false;
  bool egl_surface_available = false;
  std::vector<std::string> lifecycle_states;
  std::vector<std::string> errors;
};

struct NativeApkFirstAppStartProof {
  bool ready = false;
  bool contract_ready = false;
  bool checkpoint_boundary_reached = false;
  bool app_started = false;
  std::string schema_version = "linuxoid.first_android_app_start.contract.v1";
  std::string session_id;
  std::string artifact_root;
  std::string report_json_path;
  std::string package_name;
  std::string activity_name;
  std::string activity_component;
  std::string entrypoint_class_descriptor;
  std::string process_session_id;
  std::string process_identity;
  std::string process_name;
  int pid_value = 0;
  std::string pid_source = "linuxoid_placeholder";
  std::string runtime_session_id;
  std::string runtime_handle;
  std::string runtime_state = "unavailable";
  std::string runtime_root;
  std::string dex_state = "unavailable";
  std::string dex_parse_state = "not_requested";
  std::string bytecode_execution_state = "not_attempted";
  std::string bytecode_execution_backend = "none";
  std::string invoked_method_class_descriptor;
  std::string invoked_method_name;
  std::string invoked_method_signature;
  std::string framework_boundary_state = "not_reached";
  std::string framework_boundary_reason = "none";
  std::string lifecycle_method_name;
  std::string lifecycle_method_signature;
  std::string target_method_name;
  std::string target_method_signature;
  int dex_files_count = 0;
  bool class_loader_ready = false;
  bool art_runtime_available = false;
  bool java_execution_supported = false;
  bool java_art_bytecode_execution_requested = false;
  bool java_art_bytecode_execution_attempted = false;
  bool java_art_bytecode_executed = false;
  bool reached_return = false;
  int decoded_instruction_count = 0;
  int executed_instruction_count = 0;
  std::uint32_t instruction_offset = 0;
  std::uint16_t first_executed_opcode_value = 0;
  std::string first_executed_opcode;
  std::uint32_t last_instruction_offset = 0;
  std::uint16_t last_executed_opcode_value = 0;
  std::string last_executed_opcode;
  std::string returned_value_type;
  std::string returned_value;
  std::string activity_lifecycle_state = "not_requested";
  std::vector<std::string> activity_states_visited;
  std::string surface_window_state = "not_requested";
  std::string self_healing_state = "not_requested";
  bool self_healing_ready = false;
  bool recoverable = false;
  std::string checkpoint_state = "blocked";
  std::string blocking_reason = "none";
  std::string recommended_recovery_action = "none";
  std::string next_blocker = "none";
  std::vector<std::string> diagnostics;
  std::vector<std::string> errors;
};

struct NativeApkLaunchReport {
  std::string apk_path;
  std::string package_name;
  std::string version_name;
  int version_code = 0;
  std::string install_id;
  std::string staged_dir;
  std::string package_root;
  std::string bundle_apk_path;
  std::string manifest_path;
  std::string metadata_json_path;
  std::string bootstrap_manifest_path;
  std::string report_json_path;
  std::string native_execute_log_path;
  std::string launcher_component;
  std::string library_root;
  std::string resource_root;
  std::string asset_root;
  std::string sandbox_root;
  std::string dex_cache_root;
  std::string selected_abi;
  std::string manifest_source;
  std::string requested_package_name;
  std::string requested_component;
  std::string launch_status;
  std::string first_app_start_health = "not_requested";
  std::string surface_health = "not_requested";
  std::string window_health = "not_requested";
  std::string asset_health = "not_requested";
  std::string resource_health = "not_requested";
  std::string lifecycle_health = "not_requested";
  std::string looper_health = "not_requested";
  std::string input_health = "not_requested";
  std::string dex_health = "not_requested";
  std::string art_health = "not_requested";
  std::string binder_health = "not_requested";
  std::string activity_health = "not_requested";
  std::string activity_manager_health = "not_requested";
  std::string process_health = "not_requested";
  std::string runtime_health = "not_requested";
  std::string java_proof_health = "not_requested";
  std::string storage_health = "not_requested";
  std::string sandbox_health = "not_requested";
  std::string permission_health = "not_requested";
  std::string app_ops_health = "not_requested";
  std::string launch_health = "blocked";
  std::string recommended_recovery_action = "none";
  bool manifest_present = false;
  bool manifest_metadata_ready = false;
  bool native_libraries_present = false;
  bool host_abi_supported = false;
  bool jni_onload_called = false;
  int jni_onload_result = 0;
  bool launch_ready = false;
  bool first_app_start_proof_requested = false;
  bool surface_proof_requested = false;
  bool asset_proof_requested = false;
  bool lifecycle_proof_requested = false;
  bool dex_proof_requested = false;
  bool activity_proof_requested = false;
  bool process_proof_requested = false;
  bool window_proof_requested = false;
  bool runtime_proof_requested = false;
  bool java_proof_requested = false;
  bool storage_proof_requested = false;
  bool permissions_proof_requested = false;
  bool self_heal_proof_requested = false;
  bool surface_proof_ready = false;
  bool recoverable = false;
  int assets_count = 0;
  std::vector<std::string> native_libraries;
  std::vector<std::string> unsupported_native_libraries;
  std::vector<std::string> asset_entries;
  std::vector<std::string> declared_activities;
  std::vector<std::string> diagnostics;
  std::vector<std::string> errors;
  std::vector<std::string> limitations;
  NativeApkFirstAppStartProof first_android_app_start;
  NativeApkSurfaceSession surface;
  NativeApkAssetBridgeReport asset_bridge;
  NativeApkResourceBridgeReport resource_bridge;
  NativeApkLifecycleProof lifecycle;
  NativeApkLooperProof looper;
  NativeApkInputQueueProof input_queue;
  NativeApkDexProofReport dex;
  NativeApkArtBootstrapReport art_bootstrap;
  NativeApkPackageManagerReport package_manager;
  NativeApkIntentResolutionReport intent_resolution;
  NativeApkActivityLaunchRecord activity_launch;
  NativeApkActivityManagerReport activity_manager;
  NativeApkProcessManagerReport process_manager;
  NativeApkWindowManagerReport window_manager;
  NativeApkRuntimeBridgeReport runtime_bridge;
  NativeApkJavaProofReport java_apk_proof;
  NativeApkStorageProof storage;
  NativeApkPermissionsReport permissions;
  NativeApkAppOpsReport app_ops;
  SelfHealingAndroidDeviceReport self_healing_android_device;
  NativeExecuteReport native_execute;
};

NativeApkLaunchReport LaunchNativeApk(
    const std::string& apk_path,
    const NativeApkLaunchOptions& options = {});
std::string RenderNativeApkLaunchJson(const NativeApkLaunchReport& report);

}  // namespace wfa

#endif  // WFA_APK_NATIVE_LAUNCH_HPP
