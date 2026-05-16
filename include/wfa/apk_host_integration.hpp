#ifndef WFA_APK_HOST_INTEGRATION_HPP
#define WFA_APK_HOST_INTEGRATION_HPP

#include "wfa/apk_loader.hpp"
#include "wfa/desktop_integration.hpp"
#include "wfa/runtime_bridge.hpp"

#include <string>

namespace wfa {

struct ApkHostVerificationSpec {
  std::string serial;
  std::string compatctl_path;
  std::string desktop_root;
  std::string launcher_root;
};

struct ApkHostVerificationReport {
  LoadedApkReport loaded_apk;
  DesktopLaunchArtifacts artifacts;
  bool apk_load_ok = false;
  bool launcher_generation_ok = false;
  bool generated_launcher_ok = false;
  std::string generated_launcher_output;
};

ApkHostVerificationReport VerifyLoadedApkHostLaunchAutoWithRunner(
    const LoadedApkReport& report, const ApkHostVerificationSpec& spec,
    const CommandRunner& launcher_runner);
ApkHostVerificationReport VerifyLoadedApkHostLaunchAuto(
    const LoadedApkReport& report, const ApkHostVerificationSpec& spec);
ApkHostVerificationReport VerifyApkHostLaunchAuto(
    const std::string& apk_path, const std::string& compat_root,
    const ApkHostVerificationSpec& spec);
std::string RenderApkHostVerificationReport(
    const ApkHostVerificationReport& report);

}  // namespace wfa

#endif  // WFA_APK_HOST_INTEGRATION_HPP
