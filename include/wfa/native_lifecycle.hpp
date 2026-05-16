#ifndef WFA_NATIVE_LIFECYCLE_HPP
#define WFA_NATIVE_LIFECYCLE_HPP

#include "wfa/native_spike.hpp"

#include <string>
#include <vector>

namespace wfa {

struct NativeServiceBinding {
  std::string service_name;
  std::string service_kind;
  std::string status;
  std::string notes;
};

struct NativeLifecycleShim {
  NativeActivityBootstrap bootstrap;
  std::string session_id;
  std::string session_root;
  std::string session_manifest_path;
  std::string activity_state_path;
  std::string service_registry_path;
  std::string report_path;
  std::string runner_log_path;
  std::string runner_report_path;
  std::string current_activity_state;
  std::string process_state;
  std::string failure_reason;
  std::string selected_library_path;
  std::vector<NativeServiceBinding> services;
  bool lifecycle_handoff_ready = false;
  bool native_library_found = false;
  bool dlopen_ok = false;
  bool entrypoint_found = false;
  bool activity_called = false;
  bool execution_engine_ready = false;
  int process_id = -1;
  int exit_code = -1;
};

NativeLifecycleShim BuildNativeLifecycleShim(
    const NativeActivityBootstrap& bootstrap);
NativeLifecycleShim BuildNativeLifecycleShimFromManifest(
    const std::string& bootstrap_manifest_path);
NativeLifecycleShim RunNativeProcessBootstrap(
    const NativeActivityBootstrap& bootstrap);
NativeLifecycleShim RunNativeProcessBootstrapFromManifest(
    const std::string& bootstrap_manifest_path);
std::string RenderNativeLifecycleShimReport(
    const NativeLifecycleShim& lifecycle);

}  // namespace wfa

#endif  // WFA_NATIVE_LIFECYCLE_HPP
