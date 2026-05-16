#ifndef WFA_RUNTIME_BRIDGE_HPP
#define WFA_RUNTIME_BRIDGE_HPP

#include <functional>
#include <string>

namespace wfa {

enum class RuntimeBackendKind {
  kWaydroid,
  kAttachedAdb,
  kNative,
};

struct CommandResult {
  int exit_code = 0;
  std::string output;
};

using CommandRunner = std::function<CommandResult(const std::string&)>;

struct AdbImeStatus {
  std::string serial;
  std::string package_name;
  std::string ime_id;
  std::string settings_component;
  bool package_installed = false;
  bool ime_registered = false;
  bool ime_enabled = false;
  bool is_default_ime = false;
  bool settings_launch_ok = false;
  std::string enabled_input_methods;
  std::string default_input_method;
};

struct AdbProvisioningReport {
  std::string serial;
  std::string apk_path;
  std::string package_name;
  std::string ime_id;
  std::string settings_component;
  std::string apk_declared_package_name;
  bool apk_matches_requested_package = true;
  bool install_ok = false;
  bool enable_ok = false;
  bool set_ok = false;
  bool readback_ok = true;
  bool ready_for_typing = false;
  std::string readback_error;
  std::string install_output;
  std::string enable_output;
  std::string set_output;
  AdbImeStatus final_status;
};

struct AdbActivityLaunchReport {
  std::string serial;
  std::string component;
  bool launch_ok = false;
  std::string output;
};

struct WaydroidAppLaunchReport {
  std::string package_name;
  bool launch_ok = false;
  std::string output;
};

struct InstalledAppLaunchSpec {
  RuntimeBackendKind backend = RuntimeBackendKind::kWaydroid;
  std::string serial;
  std::string package_name;
  std::string component;
};

struct InstalledAppLaunchReport {
  std::string backend_name;
  std::string serial;
  std::string package_name;
  std::string component;
  bool launch_ok = false;
  std::string output;
};

RuntimeBackendKind ParseRuntimeBackendKind(const std::string& backend_name);
std::string RenderRuntimeBackendName(RuntimeBackendKind backend);
bool OutputContainsInstalledPackage(const std::string& output,
                                    const std::string& package_name);
bool OutputContainsImeId(const std::string& output, const std::string& ime_id);
bool EnabledInputMethodsContainIme(const std::string& output,
                                   const std::string& ime_id);
bool InstallOutputLooksSuccessful(const std::string& output);
bool LaunchOutputLooksSuccessful(const std::string& output);
bool LaunchOutputMentionsComponent(const std::string& output,
                                   const std::string& component);
bool LaunchOutputConfirmsComponent(const std::string& output,
                                   const std::string& component);
std::string RenderAdbActivityLaunchReport(
    const AdbActivityLaunchReport& report);
std::string RenderInstalledAppLaunchReport(
    const InstalledAppLaunchReport& report);
std::string RenderWaydroidAppLaunchReport(
    const WaydroidAppLaunchReport& report);
std::string RenderAdbImeStatusReport(const AdbImeStatus& status);
std::string RenderAdbProvisioningReport(const AdbProvisioningReport& report);
AdbActivityLaunchReport LaunchAdbActivityWithRunner(
    const std::string& serial, const std::string& component,
    const CommandRunner& runner);
AdbActivityLaunchReport LaunchAdbActivity(const std::string& serial,
                                          const std::string& component);
InstalledAppLaunchReport LaunchInstalledAppWithRunner(
    const InstalledAppLaunchSpec& spec, const CommandRunner& runner);
InstalledAppLaunchReport LaunchInstalledApp(const InstalledAppLaunchSpec& spec);
WaydroidAppLaunchReport LaunchWaydroidAppWithRunner(
    const std::string& package_name, const CommandRunner& runner);
WaydroidAppLaunchReport LaunchWaydroidApp(const std::string& package_name);
AdbImeStatus QueryAdbImeStatusWithRunner(const std::string& serial,
                                         const std::string& package_name,
                                         const std::string& ime_id,
                                         const std::string& settings_component,
                                         const CommandRunner& runner);
AdbImeStatus QueryAdbImeStatus(const std::string& serial,
                               const std::string& package_name,
                               const std::string& ime_id,
                               const std::string& settings_component = "");
AdbProvisioningReport ProvisionAdbImeWithRunner(
    const std::string& serial, const std::string& apk_path,
    const std::string& package_name, const std::string& ime_id,
    const std::string& settings_component, const CommandRunner& runner,
    const std::string& apk_declared_package_name = "");
AdbProvisioningReport ProvisionAdbIme(const std::string& serial,
                                      const std::string& apk_path,
                                      const std::string& package_name,
                                      const std::string& ime_id,
                                      const std::string& settings_component = "");

}  // namespace wfa

#endif  // WFA_RUNTIME_BRIDGE_HPP
