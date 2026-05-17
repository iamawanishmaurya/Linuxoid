#ifndef WFA_ART_BOOTSTRAP_EXECUTION_FIXTURE_HPP
#define WFA_ART_BOOTSTRAP_EXECUTION_FIXTURE_HPP

#include "wfa/art_activity_bootstrap_fixture.hpp"

#include <string>
#include <vector>

namespace wfa {

struct NativeArtBootstrapExecutionFixtureReport {
  std::string package_name;
  std::string install_id;
  std::string bootstrap_manifest_path;
  std::string session_root;
  std::string artifact_root;
  std::string activity_bootstrap_result_json_path;
  std::string execution_plan_path;
  std::string trace_jsonl_path;
  std::string result_json_path;
  std::string selected_application_class_name;
  std::string selected_application_class_descriptor;
  std::string selected_activity_class_name;
  std::string selected_activity_class_descriptor;
  std::string application_bootstrap_command;
  std::string activity_bootstrap_command;
  bool art_runtime_detected = false;
  bool safe_runtime_probe_available = false;
  bool runtime_class_resolution_succeeded = false;
  bool execution_attempt_planned = false;
  bool execution_attempted = false;
  bool execution_succeeded = false;
  bool application_execution_attempted = false;
  bool application_execution_succeeded = false;
  bool activity_execution_attempted = false;
  bool activity_execution_succeeded = false;
  bool dependency_blocked = true;
  std::size_t dependency_count = 0;
  std::string exit_reason;
  std::vector<std::string> missing_dependencies;
  std::vector<std::string> bootstrap_sequence;
};

NativeArtBootstrapExecutionFixtureReport
BuildNativeArtBootstrapExecutionFixture(
    const NativeArtActivityBootstrapFixtureReport& activity_report);
NativeArtBootstrapExecutionFixtureReport
RunNativeArtBootstrapExecutionFixture(
    const std::string& bootstrap_manifest_path);
std::string RenderNativeArtBootstrapExecutionFixtureJson(
    const NativeArtBootstrapExecutionFixtureReport& report);

}  // namespace wfa

#endif  // WFA_ART_BOOTSTRAP_EXECUTION_FIXTURE_HPP
