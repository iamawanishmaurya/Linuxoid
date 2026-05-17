#ifndef WFA_WAYDROID_INTEGRATION_HPP
#define WFA_WAYDROID_INTEGRATION_HPP

#include "wfa/desktop_integration.hpp"
#include "wfa/runtime_bridge.hpp"

#include <string>
#include <vector>

namespace wfa {

struct InstalledPackageVerificationSpec {
  RuntimeBackendKind backend = RuntimeBackendKind::kWaydroid;
  std::string app_name;
  std::string serial;
  std::string package_name;
  std::string component;
  std::string compatctl_path;
  std::string desktop_root;
  std::string launcher_root;
};

struct InstalledPackageVerificationReport {
  std::string backend_name;
  std::string app_name;
  std::string serial;
  std::string package_name;
  std::string component;
  RuntimePreflightReport runtime_preflight;
  bool preflight_ok = false;
  bool target_discovered = false;
  bool package_visible = false;
  bool component_ready = false;
  bool direct_launch_ok = false;
  bool launcher_generation_ok = false;
  bool generated_launcher_ok = false;
  std::string preflight_output;
  std::string preflight_notes;
  std::string direct_launch_output;
  std::string generated_launcher_output;
  InstalledPackageDesktopLaunchArtifacts artifacts;
};

struct InstalledPackageMatrixEntry {
  std::string backend_name;
  std::string serial;
  std::string package_name;
  std::string component;
  bool verification_ok = false;
  std::string error;
  InstalledPackageVerificationReport verification;
};

struct InstalledPackageMatrixReport {
  std::string backend_name;
  std::string artifact_root;
  std::string serial;
  std::vector<InstalledPackageMatrixEntry> entries;
};

struct WaydroidPackageVerificationSpec {
  std::string app_name;
  std::string package_name;
  std::string compatctl_path;
  std::string desktop_root;
  std::string launcher_root;
};

struct WaydroidPackageVerificationReport {
  std::string app_name;
  std::string serial;
  std::string package_name;
  std::string component;
  bool preflight_ok = false;
  bool target_discovered = false;
  bool package_visible = false;
  bool component_ready = false;
  bool direct_launch_ok = false;
  bool launcher_generation_ok = false;
  bool generated_launcher_ok = false;
  std::string preflight_output;
  std::string preflight_notes;
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

InstalledPackageVerificationReport VerifyInstalledPackageWithRunners(
    const InstalledPackageVerificationSpec& spec,
    const CommandRunner& runtime_runner,
    const CommandRunner& launcher_runner);
InstalledPackageVerificationReport VerifyInstalledPackage(
    const InstalledPackageVerificationSpec& spec);
InstalledPackageMatrixReport VerifyInstalledPackageMatrixWithRunners(
    const std::vector<InstalledPackageVerificationSpec>& specs,
    const std::string& artifact_root, const CommandRunner& runtime_runner,
    const CommandRunner& launcher_runner);
InstalledPackageMatrixReport VerifyInstalledPackageMatrix(
    const std::vector<InstalledPackageVerificationSpec>& specs,
    const std::string& artifact_root);
std::string RenderInstalledPackageVerificationReport(
    const InstalledPackageVerificationReport& report);
std::string RenderInstalledPackageMatrixReport(
    const InstalledPackageMatrixReport& report);

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
