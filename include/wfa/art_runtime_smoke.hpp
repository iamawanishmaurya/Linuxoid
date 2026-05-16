#ifndef WFA_ART_RUNTIME_SMOKE_HPP
#define WFA_ART_RUNTIME_SMOKE_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace wfa {

struct NativeArtRuntimeSmokeReport {
  std::string package_name;
  std::string install_id;
  std::string bootstrap_manifest_path;
  std::string artifact_root;
  std::string dex_inventory_path;
  std::string classloader_plan_path;
  std::string classloader_trace_jsonl_path;
  std::string class_resolution_map_path;
  std::string class_resolution_trace_jsonl_path;
  std::string class_resolution_result_json_path;
  std::string invocation_plan_path;
  std::string invocation_log_path;
  std::string trace_jsonl_path;
  std::string result_json_path;
  bool dex_entries_present = false;
  bool manifest_targets_ready = false;
  bool classpath_plan_ready = false;
  bool offline_resolution_ready = false;
  bool art_runtime_detected = false;
  bool safe_runtime_probe_available = false;
  bool runtime_probe_attempted = false;
  bool runtime_probe_succeeded = false;
  bool pathclassloader_resolution_planned = false;
  bool pathclassloader_resolution_attempted = false;
  bool runtime_class_resolution_succeeded = false;
  std::size_t resolved_target_count = 0;
  std::size_t missing_target_count = 0;
  int runtime_exit_code = -1;
  std::string art_runtime_probe;
  std::string runtime_probe_command;
  std::string resolved_target_class_name;
  std::string resolved_target_class_descriptor;
  std::string exit_reason;
  std::vector<std::string> target_class_names;
  std::vector<std::string> target_class_descriptors;
  std::vector<std::string> dex_entry_paths;
};

NativeArtRuntimeSmokeReport RunNativeArtRuntimeSmokeFixture(
    const std::string& bootstrap_manifest_path);
std::string RenderNativeArtRuntimeSmokeFixtureJson(
    const NativeArtRuntimeSmokeReport& report);

}  // namespace wfa

#endif  // WFA_ART_RUNTIME_SMOKE_HPP
