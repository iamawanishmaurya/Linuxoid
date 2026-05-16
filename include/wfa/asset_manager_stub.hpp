#ifndef WFA_ASSET_MANAGER_STUB_HPP
#define WFA_ASSET_MANAGER_STUB_HPP

#include "wfa/native_types.hpp"

#include <string>

namespace wfa {

struct AssetReadResult {
  bool found = false;
  std::string resolved_path;
  std::string contents;
  std::string failure_reason;
};

AAssetManager* MakeStubAssetManager(const std::string& apk_path,
                                    const std::string& resource_root);
AssetReadResult ReadStubAsset(AAssetManager* manager,
                              const std::string& asset_path);

}  // namespace wfa

#endif  // WFA_ASSET_MANAGER_STUB_HPP
