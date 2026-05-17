#ifndef WFA_APK_NATIVE_LAUNCH_HPP
#define WFA_APK_NATIVE_LAUNCH_HPP

#include "wfa/apk_activity_launch_bridge.hpp"
#include "wfa/apk_asset_bridge.hpp"
#include "wfa/apk_dex_bridge.hpp"
#include "wfa/apk_lifecycle_bridge.hpp"
#include "wfa/apk_process_bridge.hpp"
#include "wfa/apk_self_healing_watchdog.hpp"
#include "wfa/apk_storage_bridge.hpp"
#include "wfa/apk_permission_bridge.hpp"
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
  bool surface_proof_requested = false;
  bool asset_proof_requested = false;
  bool lifecycle_proof_requested = false;
  bool dex_proof_requested = false;
  bool activity_proof_requested = false;
  bool process_proof_requested = false;
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
  std::string surface_health = "not_requested";
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
  bool surface_proof_requested = false;
  bool asset_proof_requested = false;
  bool lifecycle_proof_requested = false;
  bool dex_proof_requested = false;
  bool activity_proof_requested = false;
  bool process_proof_requested = false;
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
