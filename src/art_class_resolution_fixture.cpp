#include "wfa/art_class_resolution_fixture.hpp"

#include "wfa/apk_archive.hpp"
#include "wfa/art_classloader_fixture.hpp"
#include "wfa/native_lifecycle.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

struct DexDescriptorIndex {
  bool readable = false;
  std::set<std::string> descriptors;
  std::string failure_reason;
};

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

std::string EscapeJson(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (const char character : value) {
    switch (character) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      default:
        escaped.push_back(character);
        break;
    }
  }
  return escaped;
}

std::string RenderJsonArray(const std::vector<std::string>& values) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    output << "\"" << EscapeJson(values[index]) << "\"";
  }
  output << "]";
  return output.str();
}

std::uint16_t ReadLe16(const std::string& bytes, std::size_t offset) {
  if (offset + 2 > bytes.size()) {
    throw std::runtime_error("dex_u16_out_of_bounds");
  }
  return static_cast<std::uint16_t>(
             static_cast<unsigned char>(bytes[offset + 0])) |
         (static_cast<std::uint16_t>(
              static_cast<unsigned char>(bytes[offset + 1]))
          << 8u);
}

std::uint32_t ReadLe32(const std::string& bytes, std::size_t offset) {
  if (offset + 4 > bytes.size()) {
    throw std::runtime_error("dex_u32_out_of_bounds");
  }
  return static_cast<std::uint32_t>(
             static_cast<unsigned char>(bytes[offset + 0])) |
         (static_cast<std::uint32_t>(
              static_cast<unsigned char>(bytes[offset + 1]))
          << 8u) |
         (static_cast<std::uint32_t>(
              static_cast<unsigned char>(bytes[offset + 2]))
          << 16u) |
         (static_cast<std::uint32_t>(
              static_cast<unsigned char>(bytes[offset + 3]))
          << 24u);
}

std::uint32_t ReadUleb128(const std::string& bytes, std::size_t* offset) {
  std::uint32_t value = 0;
  int shift = 0;
  while (*offset < bytes.size()) {
    const unsigned char byte =
        static_cast<unsigned char>(bytes[*offset]);
    ++(*offset);
    value |= static_cast<std::uint32_t>(byte & 0x7Fu) << shift;
    if ((byte & 0x80u) == 0) {
      return value;
    }
    shift += 7;
    if (shift > 28) {
      throw std::runtime_error("dex_uleb128_too_large");
    }
  }
  throw std::runtime_error("dex_uleb128_out_of_bounds");
}

std::string ReadDexStringByIndex(const std::string& bytes,
                                 std::uint32_t string_ids_off,
                                 std::uint32_t string_ids_size,
                                 std::uint32_t string_index) {
  if (string_index >= string_ids_size) {
    throw std::runtime_error("dex_string_index_out_of_bounds");
  }

  const std::size_t string_id_offset =
      static_cast<std::size_t>(string_ids_off) + string_index * 4u;
  const std::uint32_t string_data_off = ReadLe32(bytes, string_id_offset);
  std::size_t cursor = string_data_off;
  if (cursor >= bytes.size()) {
    throw std::runtime_error("dex_string_data_out_of_bounds");
  }

  static_cast<void>(ReadUleb128(bytes, &cursor));
  std::string value;
  while (cursor < bytes.size() && bytes[cursor] != '\0') {
    value.push_back(bytes[cursor]);
    ++cursor;
  }
  if (cursor >= bytes.size()) {
    throw std::runtime_error("dex_string_not_terminated");
  }
  return value;
}

DexDescriptorIndex BuildDexDescriptorIndex(const std::string& dex_bytes) {
  DexDescriptorIndex index;
  if (dex_bytes.size() < 0x70) {
    index.failure_reason = "dex_header_too_small";
    return index;
  }
  if (dex_bytes.compare(0, 4, "dex\n") != 0 || dex_bytes[7] != '\0') {
    index.failure_reason = "invalid_dex_magic";
    return index;
  }

  try {
    const std::uint32_t string_ids_size = ReadLe32(dex_bytes, 56);
    const std::uint32_t string_ids_off = ReadLe32(dex_bytes, 60);
    const std::uint32_t type_ids_size = ReadLe32(dex_bytes, 64);
    const std::uint32_t type_ids_off = ReadLe32(dex_bytes, 68);
    const std::uint32_t class_defs_size = ReadLe32(dex_bytes, 96);
    const std::uint32_t class_defs_off = ReadLe32(dex_bytes, 100);

    for (std::uint32_t class_index = 0; class_index < class_defs_size;
         ++class_index) {
      const std::size_t class_def_offset =
          static_cast<std::size_t>(class_defs_off) + class_index * 32u;
      const std::uint32_t type_index = ReadLe32(dex_bytes, class_def_offset);
      if (type_index >= type_ids_size) {
        throw std::runtime_error("dex_type_index_out_of_bounds");
      }
      const std::size_t type_id_offset =
          static_cast<std::size_t>(type_ids_off) + type_index * 4u;
      const std::uint32_t descriptor_string_index =
          ReadLe32(dex_bytes, type_id_offset);
      index.descriptors.insert(ReadDexStringByIndex(
          dex_bytes, string_ids_off, string_ids_size,
          descriptor_string_index));
    }
  } catch (const std::exception& error) {
    index.failure_reason = error.what();
    return index;
  }

  index.readable = true;
  return index;
}

std::string BuildResolutionMapJson(
    const NativeArtClassResolutionFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id)
         << "\",\n"
         << "  \"offline_resolution_ready\": "
         << (report.offline_resolution_ready ? "true" : "false") << ",\n"
         << "  \"resolved_target_count\": " << report.resolved_target_count
         << ",\n"
         << "  \"missing_target_count\": " << report.missing_target_count
         << ",\n"
         << "  \"targets\": [\n";
  for (std::size_t index = 0; index < report.target_results.size(); ++index) {
    const auto& target = report.target_results[index];
    output << "    {\"class_name\": \"" << EscapeJson(target.class_name)
           << "\", \"class_descriptor\": \""
           << EscapeJson(target.class_descriptor)
           << "\", \"dex_entry_path\": \""
           << EscapeJson(target.dex_entry_path)
           << "\", \"resolved_in_dex\": "
           << (target.resolved_in_dex ? "true" : "false")
           << ", \"resolution_reason\": \""
           << EscapeJson(target.resolution_reason) << "\"}";
    if (index + 1 != report.target_results.size()) {
      output << ",";
    }
    output << "\n";
  }
  output << "  ]\n"
         << "}\n";
  return output.str();
}

void WriteTrace(const NativeArtClassResolutionFixtureReport& report) {
  std::ostringstream trace;
  trace << "{\"event_type\": \"class_resolution_started\", "
        << "\"classpath_plan_ready\": "
        << (report.classpath_plan_ready ? "true" : "false") << ", "
        << "\"target_class_names\": "
        << RenderJsonArray(report.target_class_names) << "}\n";
  for (const auto& target : report.target_results) {
    trace << "{\"event_type\": \"class_resolution_result\", "
          << "\"class_name\": \"" << EscapeJson(target.class_name)
          << "\", \"class_descriptor\": \""
          << EscapeJson(target.class_descriptor)
          << "\", \"dex_entry_path\": \""
          << EscapeJson(target.dex_entry_path)
          << "\", \"resolved_in_dex\": "
          << (target.resolved_in_dex ? "true" : "false")
          << ", \"resolution_reason\": \""
          << EscapeJson(target.resolution_reason) << "\"}\n";
  }
  trace << "{\"event_type\": \"class_resolution_complete\", "
        << "\"offline_resolution_ready\": "
        << (report.offline_resolution_ready ? "true" : "false") << ", "
        << "\"resolved_target_count\": " << report.resolved_target_count
        << ", \"missing_target_count\": " << report.missing_target_count
        << ", \"exit_reason\": \"" << EscapeJson(report.exit_reason)
        << "\"}\n";
  WriteTextFile(report.trace_jsonl_path, trace.str());
}

}  // namespace

NativeArtClassResolutionFixtureReport RunNativeArtClassResolutionFixture(
    const std::string& bootstrap_manifest_path) {
  const auto classloader_report =
      RunNativeArtClassloaderFixture(bootstrap_manifest_path);
  const NativeLifecycleShim lifecycle =
      BuildNativeLifecycleShimFromManifest(bootstrap_manifest_path);

  NativeArtClassResolutionFixtureReport report;
  report.package_name = classloader_report.package_name;
  report.install_id = classloader_report.install_id;
  report.bootstrap_manifest_path = bootstrap_manifest_path;
  report.session_root = classloader_report.session_root;
  report.artifact_root = classloader_report.artifact_root;
  report.dex_inventory_path = classloader_report.dex_inventory_path;
  report.classloader_plan_path = classloader_report.classloader_plan_path;
  report.classloader_trace_jsonl_path = classloader_report.trace_jsonl_path;
  report.resolution_map_path =
      (fs::path(report.artifact_root) / "class-resolution-map.json").string();
  report.trace_jsonl_path =
      (fs::path(report.artifact_root) / "art-class-resolution-trace.jsonl")
          .string();
  report.result_json_path =
      (fs::path(report.artifact_root) / "art-class-resolution-result.json")
          .string();
  report.dex_entries_present = classloader_report.dex_entries_present;
  report.manifest_targets_ready = classloader_report.manifest_targets_ready;
  report.classpath_plan_ready = classloader_report.classpath_plan_ready;
  report.target_class_names = classloader_report.target_class_names;
  report.target_class_descriptors =
      classloader_report.target_class_descriptors;
  for (const auto& dex_entry : classloader_report.dex_entries) {
    report.dex_entry_paths.push_back(dex_entry.entry_path);
  }

  std::unordered_map<std::string, DexDescriptorIndex> dex_indexes;
  OpenedApkArchive archive;
  bool archive_ready = false;
  try {
    archive = OpenApkArchive(lifecycle.bootstrap.plan.bundle_apk_path);
    archive_ready = true;
  } catch (const std::exception& error) {
    report.exit_reason = error.what();
  }
  for (const auto& dex_entry : classloader_report.dex_entries) {
    if (!archive_ready) {
      dex_indexes.emplace(
          dex_entry.entry_path,
          DexDescriptorIndex{.readable = false,
                             .descriptors = {},
                             .failure_reason = "archive_open_failed"});
      continue;
    }
    const auto read_result = ReadApkArchiveEntry(archive, dex_entry.entry_path);
    if (!read_result.readable) {
      dex_indexes.emplace(
          dex_entry.entry_path,
          DexDescriptorIndex{.readable = false,
                             .descriptors = {},
                             .failure_reason = read_result.failure_reason});
      continue;
    }
    dex_indexes.emplace(dex_entry.entry_path,
                        BuildDexDescriptorIndex(read_result.contents));
  }

  for (std::size_t index = 0; index < report.target_class_names.size(); ++index) {
    ResolvedClassTarget target;
    target.class_name = report.target_class_names[index];
    if (index < report.target_class_descriptors.size()) {
      target.class_descriptor = report.target_class_descriptors[index];
    }
    target.resolution_reason = "descriptor_not_found_in_dex";

    for (const auto& dex_entry_path : report.dex_entry_paths) {
      const auto dex_it = dex_indexes.find(dex_entry_path);
      if (dex_it == dex_indexes.end()) {
        continue;
      }
      if (!dex_it->second.readable) {
        target.resolution_reason = dex_it->second.failure_reason;
        continue;
      }
      if (dex_it->second.descriptors.count(target.class_descriptor) != 0u) {
        target.resolved_in_dex = true;
        target.dex_entry_path = dex_entry_path;
        target.resolution_reason = "resolved_in_dex";
        break;
      }
    }

    if (target.resolved_in_dex) {
      ++report.resolved_target_count;
    } else {
      ++report.missing_target_count;
    }
    report.target_results.push_back(std::move(target));
  }

  report.offline_resolution_ready =
      report.classpath_plan_ready && !report.target_results.empty() &&
      report.missing_target_count == 0;

  if (!report.dex_entries_present) {
    report.exit_reason = "no_dex_entries_found";
  } else if (!archive_ready) {
    report.exit_reason = "dex_archive_open_failed";
  } else if (!report.manifest_targets_ready) {
    report.exit_reason = "manifest_targets_missing";
  } else if (report.offline_resolution_ready) {
    report.exit_reason = "manifest_targets_resolved_offline";
  } else {
    report.exit_reason = "manifest_target_resolution_incomplete";
  }

  WriteTextFile(report.resolution_map_path, BuildResolutionMapJson(report));
  WriteTrace(report);
  WriteTextFile(report.result_json_path,
                RenderNativeArtClassResolutionFixtureJson(report));
  return report;
}

std::string RenderNativeArtClassResolutionFixtureJson(
    const NativeArtClassResolutionFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id)
         << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"dex_inventory_path\": \""
         << EscapeJson(report.dex_inventory_path) << "\",\n"
         << "  \"classloader_plan_path\": \""
         << EscapeJson(report.classloader_plan_path) << "\",\n"
         << "  \"classloader_trace_jsonl_path\": \""
         << EscapeJson(report.classloader_trace_jsonl_path) << "\",\n"
         << "  \"resolution_map_path\": \""
         << EscapeJson(report.resolution_map_path) << "\",\n"
         << "  \"trace_jsonl_path\": \""
         << EscapeJson(report.trace_jsonl_path) << "\",\n"
         << "  \"result_json_path\": \""
         << EscapeJson(report.result_json_path) << "\",\n"
         << "  \"dex_entries_present\": "
         << (report.dex_entries_present ? "true" : "false") << ",\n"
         << "  \"manifest_targets_ready\": "
         << (report.manifest_targets_ready ? "true" : "false") << ",\n"
         << "  \"classpath_plan_ready\": "
         << (report.classpath_plan_ready ? "true" : "false") << ",\n"
         << "  \"offline_resolution_ready\": "
         << (report.offline_resolution_ready ? "true" : "false") << ",\n"
         << "  \"resolved_target_count\": " << report.resolved_target_count
         << ",\n"
         << "  \"missing_target_count\": " << report.missing_target_count
         << ",\n"
         << "  \"target_class_names\": "
         << RenderJsonArray(report.target_class_names) << ",\n"
         << "  \"target_class_descriptors\": "
         << RenderJsonArray(report.target_class_descriptors) << ",\n"
         << "  \"dex_entry_paths\": "
         << RenderJsonArray(report.dex_entry_paths) << ",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\"\n"
         << "}\n";
  return output.str();
}

}  // namespace wfa
