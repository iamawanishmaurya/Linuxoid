#include "wfa/apk_asset_bridge.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

void AppendUnique(std::vector<std::string>* values, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(value);
  }
}

std::string NormalizeRelativeAssetPath(const std::string& asset_path,
                                       bool* rejected_unsafe_path,
                                       std::vector<std::string>* errors) {
  if (rejected_unsafe_path != nullptr) {
    *rejected_unsafe_path = false;
  }

  if (asset_path.empty()) {
    AppendUnique(errors, "asset_path_empty");
    return {};
  }

  std::string normalized = asset_path;
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  if (!normalized.empty() && normalized.front() == '/') {
    if (rejected_unsafe_path != nullptr) {
      *rejected_unsafe_path = true;
    }
    AppendUnique(errors, "asset_path_absolute_rejected");
    return {};
  }
  while (normalized.rfind("./", 0) == 0) {
    normalized.erase(0, 2);
  }
  if (normalized.rfind("assets/", 0) == 0) {
    normalized.erase(0, std::string("assets/").size());
  }
  if (normalized.empty()) {
    AppendUnique(errors, "asset_path_empty");
    return {};
  }

  std::stringstream stream(normalized);
  std::string segment;
  std::vector<std::string> segments;
  while (std::getline(stream, segment, '/')) {
    if (segment.empty() || segment == ".") {
      continue;
    }
    if (segment == "..") {
      if (rejected_unsafe_path != nullptr) {
        *rejected_unsafe_path = true;
      }
      AppendUnique(errors, "asset_path_traversal_rejected");
      return {};
    }
    segments.push_back(segment);
  }

  if (segments.empty()) {
    AppendUnique(errors, "asset_path_empty");
    return {};
  }

  std::ostringstream output;
  for (std::size_t index = 0; index < segments.size(); ++index) {
    if (index != 0) {
      output << '/';
    }
    output << segments[index];
  }
  return output.str();
}

std::string ComputeChecksum(const std::string& contents) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const unsigned char byte : contents) {
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= 1099511628211ull;
  }

  std::ostringstream output;
  output << "fnv1a64:" << std::hex << std::nouppercase << std::setfill('0')
         << std::setw(16) << hash;
  return output.str();
}

std::vector<std::string> ListSortedFiles(const fs::path& root) {
  std::vector<std::string> entries;
  if (!fs::exists(root)) {
    return entries;
  }

  for (const auto& entry : fs::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    entries.push_back(fs::relative(entry.path(), root).generic_string());
  }
  std::sort(entries.begin(), entries.end());
  return entries;
}

std::string ReadTextFile(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {};
  }
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

}  // namespace

NativeApkAssetBridgeSession::NativeApkAssetBridgeSession(
    NativeApkAssetBridgeContext context)
    : context_(std::move(context)) {}

const NativeApkAssetBridgeContext& NativeApkAssetBridgeSession::context() const {
  return context_;
}

std::vector<std::string> NativeApkAssetBridgeSession::ListAssets() const {
  return ListSortedFiles(context_.asset_root);
}

NativeApkAssetReadResult NativeApkAssetBridgeSession::OpenAsset(
    const std::string& asset_path) const {
  NativeApkAssetReadResult result;
  result.requested_asset_path = asset_path;
  result.normalized_asset_path = NormalizeRelativeAssetPath(
      asset_path, &result.rejected_unsafe_path, &result.errors);
  if (result.normalized_asset_path.empty()) {
    return result;
  }

  const fs::path resolved_path =
      fs::path(context_.asset_root) / result.normalized_asset_path;
  result.resolved_path = resolved_path.string();
  if (!fs::exists(resolved_path)) {
    AppendUnique(&result.errors, "asset_not_found");
    return result;
  }

  result.contents = ReadTextFile(resolved_path);
  if (result.contents.empty() && fs::file_size(resolved_path) != 0) {
    AppendUnique(&result.errors, "asset_read_failed");
    return result;
  }

  result.opened = true;
  result.size = result.contents.size();
  result.checksum = ComputeChecksum(result.contents);
  return result;
}

NativeApkResourceBridgeReport NativeApkAssetBridgeSession::InspectResources()
    const {
  NativeApkResourceBridgeReport report;
  const fs::path resource_root = context_.resource_root;
  if (context_.resource_root.empty() || !fs::exists(resource_root)) {
    AppendUnique(&report.errors, "resource_root_missing");
    return report;
  }

  report.resource_table_present =
      fs::exists(resource_root / "resources.arsc");
  report.res_entries = ListSortedFiles(resource_root / "res");
  report.res_entries_count = static_cast<int>(report.res_entries.size());
  report.ready = true;
  return report;
}

NativeApkAssetBridgeReport ProveNativeApkAssetBridge(
    const NativeApkAssetBridgeSession& session,
    const std::string& preferred_asset_path) {
  NativeApkAssetBridgeReport report;
  report.asset_paths = session.ListAssets();
  report.assets_count = static_cast<int>(report.asset_paths.size());
  if (report.asset_paths.empty()) {
    AppendUnique(&report.errors, "asset_listing_empty");
    return report;
  }

  const std::string asset_to_open =
      preferred_asset_path.empty() ? report.asset_paths.front()
                                   : preferred_asset_path;
  const auto read = session.OpenAsset(asset_to_open);
  report.opened_asset = read.normalized_asset_path;
  report.opened_asset_size = read.size;
  report.opened_asset_checksum = read.checksum;
  report.rejected_unsafe_path = read.rejected_unsafe_path;
  report.errors = read.errors;
  report.ready = read.opened;
  return report;
}

}  // namespace wfa
