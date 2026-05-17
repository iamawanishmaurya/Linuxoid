#ifndef WFA_APK_ASSET_BRIDGE_HPP
#define WFA_APK_ASSET_BRIDGE_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace wfa {

struct NativeApkAssetBridgeContext {
  std::string session_id;
  std::string package_name;
  std::string apk_path;
  std::string staged_dir;
  std::string selected_library_path;
  std::string launch_status;
  std::string asset_root;
  std::string resource_root;
  std::string manifest_source;
};

struct NativeApkAssetReadResult {
  bool opened = false;
  bool rejected_unsafe_path = false;
  std::string requested_asset_path;
  std::string normalized_asset_path;
  std::string resolved_path;
  std::string contents;
  std::string checksum;
  std::size_t size = 0;
  std::vector<std::string> errors;
};

struct NativeApkAssetBridgeReport {
  bool ready = false;
  int assets_count = 0;
  std::vector<std::string> asset_paths;
  std::string opened_asset;
  std::size_t opened_asset_size = 0;
  std::string opened_asset_checksum;
  bool rejected_unsafe_path = false;
  std::vector<std::string> errors;
};

struct NativeApkResourceBridgeReport {
  bool ready = false;
  bool resource_table_present = false;
  int res_entries_count = 0;
  std::string decode_level = "metadata_only";
  std::vector<std::string> res_entries;
  std::vector<std::string> errors;
};

class NativeApkAssetBridgeSession {
 public:
  explicit NativeApkAssetBridgeSession(NativeApkAssetBridgeContext context);

  const NativeApkAssetBridgeContext& context() const;
  std::vector<std::string> ListAssets() const;
  NativeApkAssetReadResult OpenAsset(const std::string& asset_path) const;
  NativeApkResourceBridgeReport InspectResources() const;

 private:
  NativeApkAssetBridgeContext context_;
};

NativeApkAssetBridgeReport ProveNativeApkAssetBridge(
    const NativeApkAssetBridgeSession& session,
    const std::string& preferred_asset_path = "");

}  // namespace wfa

#endif  // WFA_APK_ASSET_BRIDGE_HPP
