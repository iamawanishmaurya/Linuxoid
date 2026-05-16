#ifndef WFA_ART_CLASS_RESOLUTION_FIXTURE_HPP
#define WFA_ART_CLASS_RESOLUTION_FIXTURE_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace wfa {

struct ResolvedClassTarget {
  std::string class_name;
  std::string class_descriptor;
  std::string dex_entry_path;
  bool resolved_in_dex = false;
  std::string resolution_reason;
};

struct NativeArtClassResolutionFixtureReport {
  std::string package_name;
  std::string install_id;
  std::string bootstrap_manifest_path;
  std::string session_root;
  std::string artifact_root;
  std::string dex_inventory_path;
  std::string classloader_plan_path;
  std::string classloader_trace_jsonl_path;
  std::string resolution_map_path;
  std::string trace_jsonl_path;
  std::string result_json_path;
  bool dex_entries_present = false;
  bool manifest_targets_ready = false;
  bool classpath_plan_ready = false;
  bool offline_resolution_ready = false;
  std::size_t resolved_target_count = 0;
  std::size_t missing_target_count = 0;
  std::string exit_reason;
  std::vector<std::string> target_class_names;
  std::vector<std::string> target_class_descriptors;
  std::vector<std::string> dex_entry_paths;
  std::vector<ResolvedClassTarget> target_results;
};

NativeArtClassResolutionFixtureReport RunNativeArtClassResolutionFixture(
    const std::string& bootstrap_manifest_path);
std::string RenderNativeArtClassResolutionFixtureJson(
    const NativeArtClassResolutionFixtureReport& report);

}  // namespace wfa

#endif  // WFA_ART_CLASS_RESOLUTION_FIXTURE_HPP
