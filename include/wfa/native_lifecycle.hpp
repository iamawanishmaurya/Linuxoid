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
  std::string current_activity_state;
  std::vector<NativeServiceBinding> services;
  bool lifecycle_handoff_ready = false;
  bool execution_engine_ready = false;
};

NativeLifecycleShim BuildNativeLifecycleShim(
    const NativeActivityBootstrap& bootstrap);
NativeLifecycleShim BuildNativeLifecycleShimFromManifest(
    const std::string& bootstrap_manifest_path);
std::string RenderNativeLifecycleShimReport(
    const NativeLifecycleShim& lifecycle);

}  // namespace wfa

#endif  // WFA_NATIVE_LIFECYCLE_HPP
