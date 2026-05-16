#ifndef WFA_WAYDROID_INTEGRATION_HPP
#define WFA_WAYDROID_INTEGRATION_HPP

#include "wfa/desktop_integration.hpp"
#include "wfa/runtime_bridge.hpp"

#include <string>

namespace wfa {

struct WaydroidPackageVerificationSpec {
  std::string app_name;
  std::string package_name;
  std::string compatctl_path;
  std::string desktop_root;
  std::string launcher_root;
};

struct WaydroidPackageVerificationReport {
  std::string app_name;
  std::string package_name;
  bool direct_launch_ok = false;
  bool launcher_generation_ok = false;
  bool generated_launcher_ok = false;
  std::string direct_launch_output;
  std::string generated_launcher_output;
  WaydroidDesktopLaunchArtifacts artifacts;
};

WaydroidPackageVerificationReport VerifyWaydroidPackageWithRunners(
    const WaydroidPackageVerificationSpec& spec,
    const CommandRunner& runtime_runner,
    const CommandRunner& launcher_runner);
WaydroidPackageVerificationReport VerifyWaydroidPackage(
    const WaydroidPackageVerificationSpec& spec);
std::string RenderWaydroidPackageVerificationReport(
    const WaydroidPackageVerificationReport& report);

}  // namespace wfa

#endif  // WFA_WAYDROID_INTEGRATION_HPP
