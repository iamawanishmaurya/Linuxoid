#include "wfa/asset_manager_stub.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace wfa {

namespace fs = std::filesystem;

struct AAssetManagerStub {
  std::string apk_path;
  std::string resource_root;
  std::string asset_root;
};

AAssetManager* MakeStubAssetManager(const std::string& apk_path,
                                    const std::string& resource_root) {
  fs::path asset_root = fs::path(resource_root) / "assets";
  if (!fs::exists(asset_root)) {
    asset_root = resource_root;
  }

  auto* manager = new AAssetManagerStub{
      .apk_path = apk_path,
      .resource_root = resource_root,
      .asset_root = asset_root.string(),
  };
  std::cout << "[asset-stub] manager created for: " << apk_path
            << " using asset root " << manager->asset_root << "\n";
  return manager;
}

AssetReadResult ReadStubAsset(AAssetManager* manager,
                              const std::string& asset_path) {
  AssetReadResult result;
  if (manager == nullptr) {
    result.failure_reason = "asset manager is null";
    return result;
  }
  if (asset_path.empty()) {
    result.failure_reason = "asset path is empty";
    return result;
  }

  const auto* stub = reinterpret_cast<AAssetManagerStub*>(manager);
  const fs::path resolved_path = fs::path(stub->asset_root) / asset_path;
  result.resolved_path = resolved_path.string();
  if (!fs::exists(resolved_path)) {
    result.failure_reason = "asset path does not exist";
    return result;
  }

  std::ifstream input(resolved_path, std::ios::binary);
  if (!input) {
    result.failure_reason = "unable to open asset path";
    return result;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  result.found = true;
  result.contents = buffer.str();
  return result;
}

}  // namespace wfa
