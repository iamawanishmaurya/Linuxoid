#ifndef WFA_APK_PERMISSION_BRIDGE_HPP
#define WFA_APK_PERMISSION_BRIDGE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wfa {

struct NativeApkPermissionRecord {
  std::string permission_name;
  std::string grant_state = "denied";
  std::string protection_level = "placeholder";
  std::string source = "manifest_uses_permission";
  std::string rationale;
  std::uint64_t updated_at_unix_ms = 0;
};

struct NativeApkAppOpRecord {
  std::string operation_name;
  std::string mode = "default";
  std::string required_permission;
  std::string reason;
  std::string source = "linuxoid_local_appops_policy";
  std::uint64_t updated_at_unix_ms = 0;
};

struct NativeApkPermissionBridgeContext {
  std::string session_id;
  std::string package_name;
  std::string apk_path;
  std::string staged_dir;
  std::string sandbox_root;
  std::string app_data_dir;
  std::string artifact_root;
  std::string manifest_source;
  std::string manifest_contents;
  int user_id = 0;
  int app_id = 10000;
  std::string storage_health = "not_requested";
  std::string binder_health = "not_requested";
  std::string dex_health = "not_requested";
  std::string art_health = "not_requested";
  std::string activity_health = "not_requested";
  bool storage_proof_requested = false;
  bool activity_proof_requested = false;
  bool dex_proof_requested = false;
};

struct NativeApkPermissionsReport {
  bool ready = false;
  bool contract_ready = false;
  bool persisted_state_preexisting = false;
  bool continuity_validated = false;
  std::string schema_version = "linuxoid.permission.contract.v1";
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
  std::string continuity_state = "not_checked";
  std::vector<std::string> requested_permissions;
  std::vector<std::string> granted_permissions;
  std::vector<std::string> denied_permissions;
  std::string decode_level = "not_requested";
  std::vector<NativeApkPermissionRecord> permission_records;
  std::vector<std::string> healing_actions;
  std::vector<std::string> continuity_diagnostics;
  std::vector<std::string> diagnostics;
  std::vector<std::string> errors;
};

struct NativeApkAppOpsReport {
  bool ready = false;
  bool contract_ready = false;
  bool persisted_state_preexisting = false;
  bool continuity_validated = false;
  std::string schema_version = "linuxoid.appops.contract.v1";
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
  std::string continuity_state = "not_checked";
  std::size_t operations_count = 0;
  std::vector<std::string> allowed_operations;
  std::vector<std::string> denied_operations;
  std::vector<std::string> default_operations;
  std::vector<std::string> ignored_placeholder_operations;
  std::vector<NativeApkAppOpRecord> operation_records;
  std::vector<std::string> healing_actions;
  std::vector<std::string> continuity_diagnostics;
  std::vector<std::string> diagnostics;
  std::vector<std::string> errors;
};

class NativeApkPermissionBridgeSession {
 public:
  explicit NativeApkPermissionBridgeSession(
      NativeApkPermissionBridgeContext context);

  const NativeApkPermissionBridgeContext& context() const;
  NativeApkPermissionsReport BuildPermissionsReport() const;
  NativeApkAppOpsReport BuildAppOpsReport(
      const NativeApkPermissionsReport& permissions) const;

 private:
  NativeApkPermissionBridgeContext context_;
};

}  // namespace wfa

#endif  // WFA_APK_PERMISSION_BRIDGE_HPP
