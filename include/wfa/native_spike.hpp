#ifndef WFA_NATIVE_SPIKE_HPP
#define WFA_NATIVE_SPIKE_HPP

#include "wfa/apk_loader.hpp"

#include <string>
#include <vector>

namespace wfa {

struct NativeSpikeAssessment {
  std::string package_name;
  std::string install_id;
  std::string app_profile;
  std::string launcher_component;
  int min_sdk = 0;
  int target_sdk = 0;
  bool native_spike_candidate = false;
  std::vector<std::string> blockers;
  std::vector<std::string> next_steps;
};

struct NativeLaunchPlan {
  NativeSpikeAssessment assessment;
  std::string apk_path;
  std::string staged_apk_path;
  std::string selected_abi;
  std::string native_root;
  std::string package_root;
  std::string bundle_root;
  std::string sandbox_root;
  std::string dex_cache_root;
  std::string resource_root;
  std::string asset_root;
  std::string library_root;
  std::string bootstrap_root;
  std::string bundle_apk_path;
  std::string manifest_copy_path;
  std::string assessment_copy_path;
  std::string bootstrap_spec_path;
  bool host_abi_supported = false;
  bool native_libraries_declared = false;
  int discovered_native_library_count = 0;
  std::vector<std::string> staged_native_libraries;
  std::vector<std::string> unsupported_native_libraries;
  bool plan_written = false;
};

struct NativeActivityBootstrap {
  NativeLaunchPlan plan;
  std::string compatctl_path;
  std::string bootstrap_manifest_path;
  std::string env_script_path;
  std::string entrypoint_script_path;
  std::string report_path;
  std::string command_line;
  bool bootstrap_ready = false;
  bool execution_engine_ready = false;
};

NativeSpikeAssessment AssessNativeSpikeCandidate(
    const LoadedApkReport& report);
NativeLaunchPlan BuildNativeLaunchPlan(const LoadedApkReport& report,
                                       const std::string& native_root);
NativeLaunchPlan PlanNativeLaunchSpike(const std::string& apk_path,
                                       const std::string& compat_root,
                                       const std::string& native_root);
NativeActivityBootstrap BuildNativeActivityBootstrap(
    const NativeLaunchPlan& plan, const std::string& compatctl_path);
NativeActivityBootstrap BootstrapNativeLaunchSpike(
    const std::string& apk_path, const std::string& compat_root,
    const std::string& native_root, const std::string& compatctl_path);
std::string RenderNativeLaunchPlanReport(const NativeLaunchPlan& plan);
std::string RenderNativeActivityBootstrapReport(
    const NativeActivityBootstrap& bootstrap);

}  // namespace wfa

#endif  // WFA_NATIVE_SPIKE_HPP
