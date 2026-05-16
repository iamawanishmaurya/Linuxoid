#ifndef WFA_DESKTOP_INTEGRATION_HPP
#define WFA_DESKTOP_INTEGRATION_HPP

#include "wfa/apk_loader.hpp"

#include <string>

namespace wfa {

struct DesktopLaunchSpec {
  std::string app_name;
  std::string serial;
  std::string apk_path;
  std::string package_name;
  std::string launcher_component;
  std::string ime_component;
  std::string compatctl_path;
  std::string desktop_root;
  std::string launcher_root;
};

struct DesktopLaunchArtifacts {
  std::string app_name;
  std::string serial;
  std::string apk_path;
  std::string package_name;
  std::string launcher_component;
  std::string ime_component;
  std::string compatctl_path;
  std::string desktop_root;
  std::string launcher_root;
  std::string script_path;
  std::string desktop_file_path;
  std::string command_line;
  bool uses_provision_mode = false;
  bool launcher_component_inferred = false;
  bool host_launch_ready = false;
};

struct WaydroidDesktopLaunchSpec {
  std::string app_name;
  std::string package_name;
  std::string compatctl_path;
  std::string desktop_root;
  std::string launcher_root;
};

struct WaydroidDesktopLaunchArtifacts {
  std::string app_name;
  std::string package_name;
  std::string compatctl_path;
  std::string desktop_root;
  std::string launcher_root;
  std::string script_path;
  std::string desktop_file_path;
  std::string command_line;
  bool host_launch_ready = false;
};

std::string NormalizeAndroidComponent(const std::string& package_name,
                                      const std::string& component);
std::string SelectLauncherActivityComponent(const ManifestProfile& profile);
DesktopLaunchArtifacts CreateDesktopLaunchArtifacts(
    const DesktopLaunchSpec& spec);
DesktopLaunchArtifacts CreateDesktopLaunchArtifactsForLoadedApk(
    const LoadedApkReport& report, const std::string& serial,
    const std::string& component, const std::string& compatctl_path,
    const std::string& desktop_root, const std::string& launcher_root = "");
DesktopLaunchArtifacts CreateDesktopLaunchArtifactsForLoadedApkAuto(
    const LoadedApkReport& report, const std::string& serial,
    const std::string& compatctl_path, const std::string& desktop_root,
    const std::string& launcher_root = "");
DesktopLaunchArtifacts DesktopifyApk(const std::string& serial,
                                     const std::string& apk_path,
                                     const std::string& component,
                                     const std::string& compat_root,
                                     const std::string& desktop_root,
                                     const std::string& launcher_root,
                                     const std::string& compatctl_path);
DesktopLaunchArtifacts DesktopifyApkAuto(const std::string& serial,
                                         const std::string& apk_path,
                                         const std::string& compat_root,
                                         const std::string& desktop_root,
                                         const std::string& launcher_root,
                                         const std::string& compatctl_path);
WaydroidDesktopLaunchArtifacts CreateWaydroidDesktopLaunchArtifacts(
    const WaydroidDesktopLaunchSpec& spec);
WaydroidDesktopLaunchArtifacts DesktopifyWaydroidPackage(
    const std::string& package_name, const std::string& desktop_root,
    const std::string& launcher_root, const std::string& compatctl_path);
std::string RenderDesktopLaunchArtifactsReport(
    const DesktopLaunchArtifacts& artifacts);
std::string RenderWaydroidDesktopLaunchArtifactsReport(
    const WaydroidDesktopLaunchArtifacts& artifacts);

}  // namespace wfa

#endif  // WFA_DESKTOP_INTEGRATION_HPP
