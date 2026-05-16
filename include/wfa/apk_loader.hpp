#ifndef WFA_APK_LOADER_HPP
#define WFA_APK_LOADER_HPP

#include "wfa/manifest_assessment.hpp"
#include "wfa/package_layout.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace wfa {

struct ApktoolMetadata {
  std::string apk_file_name;
  int min_sdk = 0;
  int target_sdk = 0;
  int version_code = 0;
  std::string version_name;
};

struct LoadedApkReport {
  std::string apk_path;
  std::string install_id;
  ApktoolMetadata metadata;
  ManifestProfile manifest_profile;
  ManifestAssessment assessment;
  PackageLayout layout;
  std::string install_root;
};

struct ApkManifestMetadata {
  bool manifest_present = false;
  bool manifest_ready = false;
  std::string manifest_source;
  std::string package_name;
  int min_sdk = 0;
  int target_sdk = 0;
  std::string application_name;
  std::vector<std::string> activity_names;
};

struct ApkResourceReadinessReport {
  std::string apk_path;
  ApkManifestMetadata manifest;
  bool asset_listing_ready = false;
  bool asset_read_ready = false;
  bool resources_table_present = false;
  std::string asset_source;
  std::string asset_root_path;
  std::string resource_root_path;
  std::vector<std::string> asset_paths;
  std::vector<std::string> errors;
};

ApktoolMetadata ParseApktoolMetadata(std::string_view yaml);
std::string BuildInstallId(const ApktoolMetadata& metadata);
std::string InspectApkPackageName(const std::string& apk_path);
std::string RenderLoadedApkReport(const LoadedApkReport& report);
ApkResourceReadinessReport InspectApkResourceReadiness(
    const std::string& apk_path, const std::string& resource_root = "");
std::string RenderApkResourceReadinessJson(
    const ApkResourceReadinessReport& report);
LoadedApkReport LoadApkToCompatRoot(const std::string& apk_path,
                                    const std::string& compat_root);

}  // namespace wfa

#endif  // WFA_APK_LOADER_HPP
