#ifndef WFA_RUNTIME_BRIDGE_HPP
#define WFA_RUNTIME_BRIDGE_HPP

#include <functional>
#include <string>
#include <vector>

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
  std::string launch_classification;
  std::string art_runtime_probe_source;
  std::string art_runtime_probe_capability;
  std::string runtime_probe_inventory_json_path;
  std::string runtime_probe_detection_reason;
  std::string bootstrap_manifest_path;
  std::string bootstrap_execution_result_path;
  std::string bootstrap_execution_trace_jsonl_path;
  std::string bootstrap_execution_runner_state_json_path;
  std::string runtime_health_json_path;
  std::string runtime_health_trace_jsonl_path;
  std::string runtime_recovery_plan_path;
  std::string runtime_recovery_actions_jsonl_path;
  std::string runtime_health_replay_json_path;
  std::string runtime_diagnostic_replay_json_path;
  std::string runtime_diagnostic_trace_index_path;
  std::string runtime_diagnostic_events_jsonl_path;
  bool runtime_health_ready = false;
  bool runtime_dependency_blocked = false;
  int runtime_failing_subsystem_count = 0;
  int runtime_recovery_actions_selected = 0;
  int runtime_canonical_recovery_scenario_count = 0;
  bool runtime_diagnostic_replay_ready = false;
  bool runtime_trace_bundle_complete = false;
  int runtime_canonical_trace_source_count = 0;
  int runtime_trace_sources_found = 0;
  int runtime_missing_trace_source_count = 0;
  std::vector<std::string> runtime_failing_subsystems;
  std::vector<std::string> runtime_selected_recovery_actions;
  std::vector<std::string> runtime_selected_recovery_action_details;
  std::vector<std::string> runtime_canonical_recovery_scenarios;
  std::vector<std::string> runtime_canonical_recovery_scenario_details;
  std::string output;
};

struct InstalledPackageMetadataSpec {
  RuntimeBackendKind backend = RuntimeBackendKind::kAttachedAdb;
  std::string serial;
  std::string package_name;
};

struct InstalledPackageMetadataReport {
  std::string backend_name;
  std::string serial;
  std::string package_name;
  bool package_visible = false;
  bool launcher_resolved = false;
  std::string resolved_component;
  std::string install_path;
  std::string version_code;
  std::string version_name;
  std::string package_check_output;
  std::string launcher_query_output;
  std::string path_query_output;
  std::string dump_output;
  std::string notes;
};

struct RuntimeTarget {
  std::string backend_name;
  std::string serial;
  std::string state;
  std::string model;
  std::string android_release;
  std::string abi;
  bool online = false;
};

struct RuntimeDiscoveryReport {
  std::string backend_name;
  bool backend_available = false;
  std::string backend_check_output;
  std::vector<RuntimeTarget> targets;
};

struct RuntimePreflightSpec {
  RuntimeBackendKind backend = RuntimeBackendKind::kAttachedAdb;
  std::string serial;
  std::string package_name;
  std::string component;
};

struct RuntimePreflightReport {
  std::string backend_name;
  std::string serial;
  std::string package_name;
  std::string component;
  bool backend_available = false;
  bool target_discovered = false;
  bool target_selected = false;
  bool target_online = false;
  bool package_visible = false;
  bool component_ready = false;
  bool ready_for_launch = false;
  bool runtime_probe_ready = false;
  bool bootstrap_planned = false;
  bool dependency_blocked = false;
  std::string model;
  std::string android_release;
  std::string abi;
  std::string art_runtime_probe_source;
  std::string art_runtime_probe_capability;
  std::string runtime_probe_inventory_json_path;
  std::string runtime_probe_detection_reason;
  std::string bootstrap_manifest_path;
  std::string runtime_health_json_path;
  std::string runtime_health_trace_jsonl_path;
  std::string runtime_recovery_actions_jsonl_path;
  std::string runtime_health_replay_json_path;
  std::string runtime_diagnostic_replay_json_path;
  std::string runtime_diagnostic_trace_index_path;
  std::string runtime_diagnostic_events_jsonl_path;
  std::string backend_check_output;
  std::string discovery_output;
  std::string package_check_output;
  std::string notes;
  int failing_subsystem_count = 0;
  int recovery_actions_selected = 0;
  int canonical_recovery_scenario_count = 0;
  bool runtime_diagnostic_replay_ready = false;
  bool runtime_trace_bundle_complete = false;
  int runtime_canonical_trace_source_count = 0;
  int runtime_trace_sources_found = 0;
  int runtime_missing_trace_source_count = 0;
  std::vector<std::string> failing_subsystems;
  std::vector<std::string> selected_recovery_actions;
  std::vector<std::string> selected_recovery_action_details;
  std::vector<std::string> canonical_recovery_scenarios;
  std::vector<std::string> canonical_recovery_scenario_details;
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
std::string RenderInstalledPackageMetadataReport(
    const InstalledPackageMetadataReport& report);
std::string RenderRuntimeDiscoveryReport(
    const RuntimeDiscoveryReport& report);
std::string RenderRuntimePreflightReport(
    const RuntimePreflightReport& report);
std::string RenderWaydroidAppLaunchReport(
    const WaydroidAppLaunchReport& report);
std::string RenderAdbImeStatusReport(const AdbImeStatus& status);
std::string RenderAdbProvisioningReport(const AdbProvisioningReport& report);
RuntimeDiscoveryReport DiscoverRuntimeTargetsWithRunner(
    RuntimeBackendKind backend, const CommandRunner& runner);
RuntimeDiscoveryReport DiscoverRuntimeTargets(RuntimeBackendKind backend);
RuntimePreflightReport PreflightRuntimeWithRunner(
    const RuntimePreflightSpec& spec, const CommandRunner& runner);
RuntimePreflightReport PreflightRuntime(const RuntimePreflightSpec& spec);
AdbActivityLaunchReport LaunchAdbActivityWithRunner(
    const std::string& serial, const std::string& component,
    const CommandRunner& runner);
AdbActivityLaunchReport LaunchAdbActivity(const std::string& serial,
                                          const std::string& component);
InstalledPackageMetadataReport QueryInstalledPackageMetadataWithRunner(
    const InstalledPackageMetadataSpec& spec, const CommandRunner& runner);
InstalledPackageMetadataReport QueryInstalledPackageMetadata(
    const InstalledPackageMetadataSpec& spec);
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
