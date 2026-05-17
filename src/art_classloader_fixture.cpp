#include "wfa/art_classloader_fixture.hpp"

#include "wfa/apk_archive.hpp"
#include "wfa/apk_loader.hpp"
#include "wfa/native_lifecycle.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

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

bool FileExists(const std::string& path) {
  return !path.empty() && fs::exists(path);
}

bool IsExecutableFile(const std::string& path) {
  if (!FileExists(path)) {
    return false;
  }
  std::error_code error;
  const auto status = fs::status(path, error);
  if (error) {
    return false;
  }
  const auto executable_bits = fs::perms::owner_exec | fs::perms::group_exec |
                               fs::perms::others_exec;
  return (status.permissions() & executable_bits) != fs::perms::none;
}

bool IsDexArchiveEntry(const std::string& path) {
  if (path.size() < 10) {
    return false;
  }
  if (path.rfind("classes", 0) != 0) {
    return false;
  }
  return path.substr(path.size() - 4) == ".dex";
}

std::uint32_t ReadLe32(const std::string& bytes, std::size_t offset) {
  if (offset + 4 > bytes.size()) {
    return 0;
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

std::string NormalizeClassName(const std::string& package_name,
                               const std::string& class_name) {
  if (package_name.empty() || class_name.empty()) {
    return "";
  }
  if (class_name.front() == '.') {
    return package_name + class_name;
  }
  if (class_name.find('.') == std::string::npos) {
    return package_name + "." + class_name;
  }
  return class_name;
}

std::string ToDescriptor(const std::string& class_name) {
  if (class_name.empty()) {
    return "";
  }
  std::string descriptor = "L";
  descriptor.reserve(class_name.size() + 2);
  for (const char character : class_name) {
    descriptor.push_back(character == '.' ? '/' : character);
  }
  descriptor.push_back(';');
  return descriptor;
}

void AppendUnique(std::vector<std::string>& values, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

bool ExecutableExistsInPath(const std::string& executable_name,
                            std::string* resolved_path) {
  const char* path_env = std::getenv("PATH");
  if (path_env == nullptr) {
    return false;
  }

  std::stringstream path_stream(path_env);
  std::string segment;
  while (std::getline(path_stream, segment, ':')) {
    if (segment.empty()) {
      continue;
    }
    const fs::path candidate = fs::path(segment) / executable_name;
    if (!fs::exists(candidate)) {
      continue;
    }
    std::error_code error;
    const auto status = fs::status(candidate, error);
    if (!error && (status.permissions() & fs::perms::owner_exec) !=
                      fs::perms::none) {
      if (resolved_path != nullptr) {
        *resolved_path = candidate.string();
      }
      return true;
    }
  }
  return false;
}

std::string DetectArtRuntimeProbe(bool* detected) {
  const char* override_path = std::getenv("LINUXOID_ART_RUNTIME_PROBE_OVERRIDE");
  if (override_path != nullptr && override_path[0] != '\0' &&
      IsExecutableFile(override_path)) {
    *detected = true;
    return override_path;
  }

  const std::vector<std::string> file_candidates = {
      "/apex/com.android.art/bin/dalvikvm",
      "/apex/com.android.art/lib64/libart.so",
      "/system/bin/dalvikvm",
      "/system/lib64/libart.so",
      "/system/lib/libart.so",
  };
  for (const auto& candidate : file_candidates) {
    if (FileExists(candidate)) {
      *detected = true;
      return candidate;
    }
  }

  std::string resolved_path;
  if (ExecutableExistsInPath("dalvikvm", &resolved_path)) {
    *detected = true;
    return resolved_path;
  }
  if (ExecutableExistsInPath("app_process", &resolved_path)) {
    *detected = true;
    return resolved_path;
  }

  *detected = false;
  return "art_runtime_not_detected";
}

std::string BuildDexInventoryJson(
    const NativeArtClassloaderFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id)
         << "\",\n"
         << "  \"dex_entries_present\": "
         << (report.dex_entries_present ? "true" : "false") << ",\n"
         << "  \"dex_entries\": [\n";
  for (std::size_t index = 0; index < report.dex_entries.size(); ++index) {
    const auto& entry = report.dex_entries[index];
    output << "    {\"entry_path\": \"" << EscapeJson(entry.entry_path)
           << "\", \"compression_method\": " << entry.compression_method
           << ", \"archive_uncompressed_size\": " << entry.archive_uncompressed_size
           << ", \"archive_compressed_size\": " << entry.archive_compressed_size
           << ", \"readable\": " << (entry.readable ? "true" : "false")
           << ", \"valid_dex_magic\": "
           << (entry.valid_dex_magic ? "true" : "false")
           << ", \"dex_version\": \"" << EscapeJson(entry.dex_version)
           << "\", \"dex_file_size\": " << entry.dex_file_size
           << ", \"header_size\": " << entry.header_size
           << ", \"string_ids_size\": " << entry.string_ids_size
           << ", \"type_ids_size\": " << entry.type_ids_size
           << ", \"class_defs_size\": " << entry.class_defs_size
           << ", \"read_failure_reason\": \""
           << EscapeJson(entry.read_failure_reason) << "\"}";
    if (index + 1 != report.dex_entries.size()) {
      output << ",";
    }
    output << "\n";
  }
  output << "  ]\n"
         << "}\n";
  return output.str();
}

std::string BuildClassloaderPlanJson(
    const NativeArtClassloaderFixtureReport& report) {
  std::vector<std::string> dex_paths;
  dex_paths.reserve(report.dex_entries.size());
  for (const auto& entry : report.dex_entries) {
    dex_paths.push_back(entry.entry_path);
  }

  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id)
         << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"dex_entries_present\": "
         << (report.dex_entries_present ? "true" : "false") << ",\n"
         << "  \"manifest_targets_ready\": "
         << (report.manifest_targets_ready ? "true" : "false") << ",\n"
         << "  \"classpath_plan_ready\": "
         << (report.classpath_plan_ready ? "true" : "false") << ",\n"
         << "  \"art_runtime_detected\": "
         << (report.art_runtime_detected ? "true" : "false") << ",\n"
         << "  \"pathclassloader_probe_ready\": "
         << (report.pathclassloader_probe_ready ? "true" : "false") << ",\n"
         << "  \"art_runtime_probe\": \"" << EscapeJson(report.art_runtime_probe)
         << "\",\n"
         << "  \"dex_paths\": " << RenderJsonArray(dex_paths) << ",\n"
         << "  \"target_class_names\": "
         << RenderJsonArray(report.target_class_names) << ",\n"
         << "  \"target_class_descriptors\": "
         << RenderJsonArray(report.target_class_descriptors) << ",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\"\n"
         << "}\n";
  return output.str();
}

void WriteTrace(const NativeArtClassloaderFixtureReport& report) {
  std::ostringstream trace;
  trace << "{\"event_type\": \"dex_inventory_recorded\", "
        << "\"dex_entries_present\": "
        << (report.dex_entries_present ? "true" : "false") << ", "
        << "\"dex_inventory_path\": \"" << EscapeJson(report.dex_inventory_path)
        << "\"}\n";
  trace << "{\"event_type\": \"manifest_targets_recorded\", "
        << "\"manifest_targets_ready\": "
        << (report.manifest_targets_ready ? "true" : "false") << ", "
        << "\"target_class_names\": "
        << RenderJsonArray(report.target_class_names) << "}\n";
  trace << "{\"event_type\": \"art_runtime_probe_recorded\", "
        << "\"art_runtime_detected\": "
        << (report.art_runtime_detected ? "true" : "false") << ", "
        << "\"art_runtime_probe\": \"" << EscapeJson(report.art_runtime_probe)
        << "\"}\n";
  trace << "{\"event_type\": \"classloader_plan_written\", "
        << "\"classpath_plan_ready\": "
        << (report.classpath_plan_ready ? "true" : "false") << ", "
        << "\"pathclassloader_probe_ready\": "
        << (report.pathclassloader_probe_ready ? "true" : "false") << ", "
        << "\"classloader_plan_path\": \""
        << EscapeJson(report.classloader_plan_path) << "\", "
        << "\"exit_reason\": \"" << EscapeJson(report.exit_reason) << "\"}\n";
  WriteTextFile(report.trace_jsonl_path, trace.str());
}

DexEntryMetadata InspectDexArchiveEntry(const OpenedApkArchive& archive,
                                        const ApkArchiveEntry& entry) {
  DexEntryMetadata metadata;
  metadata.entry_path = entry.path;
  metadata.compression_method = entry.compression_method;
  metadata.archive_uncompressed_size = entry.uncompressed_size;
  metadata.archive_compressed_size = entry.compressed_size;

  const auto read_result = ReadApkArchiveEntry(archive, entry.path);
  metadata.readable = read_result.readable;
  if (!read_result.readable) {
    metadata.read_failure_reason = read_result.failure_reason;
    return metadata;
  }

  if (read_result.contents.size() < 0x70) {
    metadata.read_failure_reason = "dex_header_too_small";
    return metadata;
  }

  if (read_result.contents[0] != 'd' || read_result.contents[1] != 'e' ||
      read_result.contents[2] != 'x' || read_result.contents[3] != '\n' ||
      read_result.contents[7] != '\0') {
    metadata.read_failure_reason = "invalid_dex_magic";
    return metadata;
  }

  metadata.valid_dex_magic = true;
  metadata.dex_version.assign(read_result.contents.data() + 4, 3);
  metadata.dex_file_size = ReadLe32(read_result.contents, 32);
  metadata.header_size = ReadLe32(read_result.contents, 36);
  metadata.string_ids_size = ReadLe32(read_result.contents, 56);
  metadata.type_ids_size = ReadLe32(read_result.contents, 64);
  metadata.class_defs_size = ReadLe32(read_result.contents, 96);
  return metadata;
}

}  // namespace

NativeArtClassloaderFixtureReport RunNativeArtClassloaderFixture(
    const std::string& bootstrap_manifest_path) {
  const NativeLifecycleShim lifecycle =
      BuildNativeLifecycleShimFromManifest(bootstrap_manifest_path);
  const fs::path manifest_hint =
      fs::path(lifecycle.bootstrap.plan.package_root) / "bundle" /
      "AndroidManifest.xml";
  const ApkResourceReadinessReport resources = InspectApkResourceReadiness(
      lifecycle.bootstrap.plan.bundle_apk_path,
      lifecycle.bootstrap.plan.resource_root, manifest_hint.string());

  NativeArtClassloaderFixtureReport report;
  report.package_name = lifecycle.bootstrap.plan.assessment.package_name;
  report.install_id = lifecycle.bootstrap.plan.assessment.install_id;
  report.bootstrap_manifest_path = bootstrap_manifest_path;
  report.session_root = lifecycle.session_root;
  report.artifact_root = (fs::path(lifecycle.session_root) / "art").string();
  report.dex_inventory_path =
      (fs::path(report.artifact_root) / "dex-inventory.json").string();
  report.classloader_plan_path =
      (fs::path(report.artifact_root) / "classloader-plan.json").string();
  report.trace_jsonl_path =
      (fs::path(report.artifact_root) / "art-classloader-trace.jsonl").string();

  fs::create_directories(report.artifact_root);

  OpenedApkArchive archive;
  try {
    archive = OpenApkArchive(lifecycle.bootstrap.plan.bundle_apk_path);
  } catch (const std::exception&) {
    archive.entries.clear();
  }

  for (const auto& entry : archive.entries) {
    if (!IsDexArchiveEntry(entry.path)) {
      continue;
    }
    report.dex_entries.push_back(
        InspectDexArchiveEntry(archive, entry));
  }
  report.dex_entries_present = !report.dex_entries.empty();

  std::sort(report.dex_entries.begin(), report.dex_entries.end(),
            [](const DexEntryMetadata& left, const DexEntryMetadata& right) {
              return left.entry_path < right.entry_path;
            });

  AppendUnique(report.target_class_names,
               NormalizeClassName(resources.manifest.package_name,
                                  resources.manifest.application_name));
  for (const auto& activity_name : resources.manifest.activity_names) {
    AppendUnique(report.target_class_names,
                 NormalizeClassName(resources.manifest.package_name,
                                    activity_name));
  }
  for (const auto& class_name : report.target_class_names) {
    AppendUnique(report.target_class_descriptors, ToDescriptor(class_name));
  }
  report.manifest_targets_ready = !report.target_class_names.empty();

  report.classpath_plan_ready =
      report.dex_entries_present && report.manifest_targets_ready;
  report.art_runtime_probe = DetectArtRuntimeProbe(&report.art_runtime_detected);
  report.pathclassloader_probe_ready =
      report.classpath_plan_ready && report.art_runtime_detected;

  if (!report.dex_entries_present) {
    report.exit_reason = "no_dex_entries_found";
  } else if (!report.manifest_targets_ready) {
    report.exit_reason = "manifest_targets_missing";
  } else if (report.pathclassloader_probe_ready) {
    report.exit_reason = "art_classpath_plan_ready";
  } else {
    report.exit_reason = "art_classpath_plan_ready_runtime_missing";
  }

  WriteTextFile(report.dex_inventory_path, BuildDexInventoryJson(report));
  WriteTextFile(report.classloader_plan_path, BuildClassloaderPlanJson(report));
  WriteTrace(report);
  return report;
}

std::string RenderNativeArtClassloaderFixtureJson(
    const NativeArtClassloaderFixtureReport& report) {
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
         << "  \"trace_jsonl_path\": \""
         << EscapeJson(report.trace_jsonl_path) << "\",\n"
         << "  \"dex_entries_present\": "
         << (report.dex_entries_present ? "true" : "false") << ",\n"
         << "  \"manifest_targets_ready\": "
         << (report.manifest_targets_ready ? "true" : "false") << ",\n"
         << "  \"classpath_plan_ready\": "
         << (report.classpath_plan_ready ? "true" : "false") << ",\n"
         << "  \"art_runtime_detected\": "
         << (report.art_runtime_detected ? "true" : "false") << ",\n"
         << "  \"pathclassloader_probe_ready\": "
         << (report.pathclassloader_probe_ready ? "true" : "false") << ",\n"
         << "  \"art_runtime_probe\": \"" << EscapeJson(report.art_runtime_probe)
         << "\",\n"
         << "  \"target_class_names\": "
         << RenderJsonArray(report.target_class_names) << ",\n"
         << "  \"target_class_descriptors\": "
         << RenderJsonArray(report.target_class_descriptors) << ",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\"\n"
         << "}\n";
  return output.str();
}

}  // namespace wfa
