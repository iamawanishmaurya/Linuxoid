#ifndef WFA_ART_ACTIVITY_BOOTSTRAP_FIXTURE_HPP
#define WFA_ART_ACTIVITY_BOOTSTRAP_FIXTURE_HPP

#include "wfa/art_runtime_smoke.hpp"

#include <string>
#include <vector>

namespace wfa {

struct NativeArtActivityBootstrapFixtureReport {
  std::string package_name;
  std::string install_id;
  std::string bootstrap_manifest_path;
  std::string session_root;
  std::string artifact_root;
  std::string launcher_component;
  std::string selected_application_class_name;
  std::string selected_application_class_descriptor;
  std::string selected_activity_class_name;
  std::string selected_activity_class_descriptor;
  std::string class_resolution_result_json_path;
  std::string runtime_smoke_result_json_path;
  std::string activity_bootstrap_plan_path;
  std::string trace_jsonl_path;
  std::string result_json_path;
  std::string application_bootstrap_command;
  std::string activity_bootstrap_command;
  bool manifest_targets_ready = false;
  bool classpath_plan_ready = false;
  bool offline_resolution_ready = false;
  bool art_runtime_detected = false;
  bool safe_runtime_probe_available = false;
  std::string art_runtime_probe_source;
  std::string art_runtime_probe_inventory_path;
  std::string art_runtime_probe_detection_reason;
  std::string art_runtime_probe_capability;
  bool runtime_class_resolution_succeeded = false;
  bool application_probe_attempted = false;
  bool application_probe_succeeded = false;
  bool activity_probe_attempted = false;
  bool activity_probe_succeeded = false;
  bool runtime_bootstrap_planned = false;
  bool runtime_bootstrap_attempted = false;
  bool runtime_bootstrap_succeeded = false;
  bool dependency_blocked = true;
  std::size_t dependency_count = 0;
  std::string exit_reason;
  std::vector<std::string> bootstrap_dependencies;
  std::vector<std::string> missing_dependencies;
  std::vector<std::string> planned_bootstrap_steps;
};

NativeArtActivityBootstrapFixtureReport BuildNativeArtActivityBootstrapFixture(
    const NativeArtRuntimeSmokeReport& runtime_smoke_report);
NativeArtActivityBootstrapFixtureReport RunNativeArtActivityBootstrapFixture(
    const std::string& bootstrap_manifest_path);
std::string RenderNativeArtActivityBootstrapFixtureJson(
    const NativeArtActivityBootstrapFixtureReport& report);

}  // namespace wfa

#endif  // WFA_ART_ACTIVITY_BOOTSTRAP_FIXTURE_HPP
