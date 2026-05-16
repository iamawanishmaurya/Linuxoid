#ifndef WFA_APK_LOADER_HPP
#define WFA_APK_LOADER_HPP

#include "wfa/manifest_assessment.hpp"
#include "wfa/package_layout.hpp"

#include <string>
#include <string_view>

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

ApktoolMetadata ParseApktoolMetadata(std::string_view yaml);
std::string BuildInstallId(const ApktoolMetadata& metadata);
std::string RenderLoadedApkReport(const LoadedApkReport& report);
LoadedApkReport LoadApkToCompatRoot(const std::string& apk_path,
                                    const std::string& compat_root);

}  // namespace wfa

#endif  // WFA_APK_LOADER_HPP
