#ifndef WFA_WAYDROID_INTEGRATION_HPP
#define WFA_WAYDROID_INTEGRATION_HPP

#include "wfa/desktop_integration.hpp"
#include "wfa/runtime_bridge.hpp"

#include <string>
#include <vector>

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

struct WaydroidMatrixEntry {
  std::string package_name;
  bool verification_ok = false;
  std::string error;
  WaydroidPackageVerificationReport verification;
};

struct WaydroidMatrixReport {
  std::string artifact_root;
  std::vector<WaydroidMatrixEntry> entries;
};

WaydroidPackageVerificationReport VerifyWaydroidPackageWithRunners(
    const WaydroidPackageVerificationSpec& spec,
    const CommandRunner& runtime_runner,
    const CommandRunner& launcher_runner);
WaydroidPackageVerificationReport VerifyWaydroidPackage(
    const WaydroidPackageVerificationSpec& spec);
WaydroidMatrixReport VerifyWaydroidPackageMatrixWithRunners(
    const std::vector<std::string>& packages, const std::string& compatctl_path,
    const std::string& artifact_root, const CommandRunner& runtime_runner,
    const CommandRunner& launcher_runner);
WaydroidMatrixReport VerifyWaydroidPackageMatrix(
    const std::vector<std::string>& packages, const std::string& compatctl_path,
    const std::string& artifact_root);
std::string RenderWaydroidPackageVerificationReport(
    const WaydroidPackageVerificationReport& report);
std::string RenderWaydroidMatrixReport(const WaydroidMatrixReport& report);

}  // namespace wfa

#endif  // WFA_WAYDROID_INTEGRATION_HPP
