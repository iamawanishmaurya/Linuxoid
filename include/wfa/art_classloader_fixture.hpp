#ifndef WFA_ART_CLASSLOADER_FIXTURE_HPP
#define WFA_ART_CLASSLOADER_FIXTURE_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace wfa {

struct DexEntryMetadata {
  std::string entry_path;
  std::uint16_t compression_method = 0;
  std::uint32_t archive_uncompressed_size = 0;
  std::uint32_t archive_compressed_size = 0;
  bool readable = false;
  bool valid_dex_magic = false;
  std::string dex_version;
  std::uint32_t dex_file_size = 0;
  std::uint32_t header_size = 0;
  std::uint32_t string_ids_size = 0;
  std::uint32_t type_ids_size = 0;
  std::uint32_t class_defs_size = 0;
  std::string read_failure_reason;
};

struct NativeArtClassloaderFixtureReport {
  std::string package_name;
  std::string install_id;
  std::string bootstrap_manifest_path;
  std::string session_root;
  std::string artifact_root;
  std::string dex_inventory_path;
  std::string classloader_plan_path;
  std::string art_runtime_probe_inventory_path;
  std::string trace_jsonl_path;
  bool dex_entries_present = false;
  bool manifest_targets_ready = false;
  bool classpath_plan_ready = false;
  bool art_runtime_detected = false;
  bool pathclassloader_probe_ready = false;
  std::string art_runtime_probe;
  std::string art_runtime_probe_detection_reason;
  std::string exit_reason;
  std::vector<DexEntryMetadata> dex_entries;
  std::vector<std::string> target_class_names;
  std::vector<std::string> target_class_descriptors;
};

NativeArtClassloaderFixtureReport RunNativeArtClassloaderFixture(
    const std::string& bootstrap_manifest_path);
std::string RenderNativeArtClassloaderFixtureJson(
    const NativeArtClassloaderFixtureReport& report);

}  // namespace wfa

#endif  // WFA_ART_CLASSLOADER_FIXTURE_HPP
