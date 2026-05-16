#include "wfa/asset_manager_stub.hpp"
#include "wfa/apk_archive.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

struct AAssetManagerStub {
  std::string apk_path;
  std::string resource_root;
  std::string asset_root;
};

std::string NormalizeAssetPath(const std::string& asset_path,
                               std::string* failure_reason) {
  if (asset_path.empty()) {
    if (failure_reason != nullptr) {
      *failure_reason = "asset path is empty";
    }
    return {};
  }

  std::string normalized = asset_path;
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  while (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  while (normalized.rfind("./", 0) == 0) {
    normalized.erase(0, 2);
  }
  if (normalized.rfind("assets/", 0) == 0) {
    normalized.erase(0, std::string("assets/").size());
  }

  if (normalized.empty()) {
    if (failure_reason != nullptr) {
      *failure_reason = "asset path is empty";
    }
    return {};
  }

  std::vector<std::string> clean_segments;
  std::stringstream stream(normalized);
  std::string segment;
  while (std::getline(stream, segment, '/')) {
    if (segment.empty() || segment == ".") {
      continue;
    }
    if (segment == "..") {
      if (failure_reason != nullptr) {
        *failure_reason = "asset_path_traversal_rejected";
      }
      return {};
    }
    clean_segments.push_back(segment);
  }

  if (clean_segments.empty()) {
    if (failure_reason != nullptr) {
      *failure_reason = "asset path is empty";
    }
    return {};
  }

  std::ostringstream result;
  for (std::size_t index = 0; index < clean_segments.size(); ++index) {
    if (index != 0) {
      result << '/';
    }
    result << clean_segments[index];
  }
  return result.str();
}

std::vector<std::string> ListFilesystemAssets(const fs::path& asset_root) {
  std::vector<std::string> assets;
  if (!fs::exists(asset_root)) {
    return assets;
  }

  for (const auto& entry : fs::recursive_directory_iterator(asset_root)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const fs::path relative = fs::relative(entry.path(), asset_root);
    assets.push_back(relative.generic_string());
  }
  std::sort(assets.begin(), assets.end());
  return assets;
}

std::vector<std::string> ListArchiveAssets(const std::string& apk_path) {
  std::vector<std::string> assets;
  for (const auto& entry : ListApkArchiveEntries(apk_path)) {
    if (entry.is_directory || entry.path.rfind("assets/", 0) != 0) {
      continue;
    }
    assets.push_back(entry.path.substr(std::string("assets/").size()));
  }
  std::sort(assets.begin(), assets.end());
  return assets;
}

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

std::vector<std::string> ListStubAssets(AAssetManager* manager) {
  if (manager == nullptr) {
    return {};
  }

  const auto* stub = reinterpret_cast<AAssetManagerStub*>(manager);
  if (!stub->asset_root.empty() && fs::exists(stub->asset_root)) {
    return ListFilesystemAssets(stub->asset_root);
  }
  if (!stub->apk_path.empty() && fs::exists(stub->apk_path)) {
    return ListArchiveAssets(stub->apk_path);
  }
  return {};
}

AssetReadResult ReadStubAsset(AAssetManager* manager,
                              const std::string& asset_path) {
  AssetReadResult result;
  if (manager == nullptr) {
    result.failure_reason = "asset manager is null";
    return result;
  }

  const auto* stub = reinterpret_cast<AAssetManagerStub*>(manager);
  const std::string normalized_path =
      NormalizeAssetPath(asset_path, &result.failure_reason);
  if (normalized_path.empty()) {
    return result;
  }

  if (!stub->asset_root.empty() && fs::exists(stub->asset_root)) {
    const fs::path resolved_path = fs::path(stub->asset_root) / normalized_path;
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

  if (!stub->apk_path.empty() && fs::exists(stub->apk_path)) {
    result.resolved_path = "zip:" + stub->apk_path + "!/assets/" + normalized_path;
    const auto archive_read =
        ReadApkArchiveEntry(stub->apk_path, "assets/" + normalized_path);
    if (!archive_read.found) {
      result.failure_reason = "asset path does not exist";
      return result;
    }
    if (!archive_read.readable) {
      result.failure_reason = archive_read.failure_reason;
      return result;
    }
    result.found = true;
    result.contents = archive_read.contents;
    return result;
  }

  result.failure_reason = "asset root is unavailable";
  return result;
}

}  // namespace wfa
