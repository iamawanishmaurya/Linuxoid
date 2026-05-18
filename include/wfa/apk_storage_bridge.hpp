#ifndef WFA_APK_STORAGE_BRIDGE_HPP
#define WFA_APK_STORAGE_BRIDGE_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace wfa {

struct NativeApkStorageBridgeContext {
  std::string session_id;
  std::string package_name;
  std::string apk_path;
  std::string staged_dir;
  std::string app_data_dir;
  std::string files_dir;
  std::string cache_dir;
  std::string native_lib_dir;
  std::string asset_root;
  std::string resource_root;
  std::string artifact_root;
  int uid_placeholder = 10000;
  int gid_placeholder = 10000;
  std::string isolation_level = "path_sandbox_only";
  std::string sandbox_state = "path_sandbox_only";
  std::vector<std::string> permission_metadata;
};

struct NativeApkStoragePathResolution {
  bool accepted = false;
  bool rejected = false;
  std::string requested_path;
  std::string normalized_relative_path;
  std::string resolved_path;
  std::vector<std::string> errors;
};

struct NativeApkStorageProof {
  bool ready = false;
  bool persisted_state_preexisting = false;
  bool continuity_validated = false;
  bool marker_preexisting = false;
  bool marker_reused = false;
  std::string session_id;
  std::string artifact_root;
  std::string report_json_path;
  std::string package_name;
  std::string apk_path;
  std::string staged_dir;
  std::string app_data_dir;
  std::string files_dir;
  std::string cache_dir;
  std::string native_lib_dir;
  std::string asset_root;
  std::string resource_root;
  std::string marker_path;
  std::size_t marker_size = 0;
  std::string marker_checksum;
  std::string continuity_state = "not_checked";
  int uid_placeholder = 10000;
  int gid_placeholder = 10000;
  std::string isolation_level = "path_sandbox_only";
  std::string sandbox_state = "path_sandbox_only";
  bool permission_metadata_ready = false;
  bool marker_written = false;
  bool marker_read_back = false;
  int rejected_escape_paths = 0;
  std::vector<std::string> accepted_paths;
  std::vector<std::string> rejected_paths;
  std::vector<std::string> permission_metadata;
  std::vector<std::string> continuity_diagnostics;
  std::vector<std::string> errors;
};

class NativeApkStorageBridgeSession {
 public:
  explicit NativeApkStorageBridgeSession(NativeApkStorageBridgeContext context);

  const NativeApkStorageBridgeContext& context() const;
  NativeApkStoragePathResolution ResolveAppRelativePath(
      const std::string& relative_path) const;
  NativeApkStorageProof RunStorageProof() const;

 private:
  NativeApkStorageBridgeContext context_;
};

}  // namespace wfa

#endif  // WFA_APK_STORAGE_BRIDGE_HPP
