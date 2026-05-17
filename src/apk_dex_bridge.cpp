#include "wfa/apk_dex_bridge.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

bool IsDexArchiveEntry(const std::string& path) {
  if (path.size() < std::string("classes.dex").size()) {
    return false;
  }
  if (path.rfind("classes", 0) != 0) {
    return false;
  }
  return path.substr(path.size() - 4) == ".dex";
}

bool IsSafeArchivePath(const std::string& archive_path) {
  if (archive_path.empty() || archive_path.front() == '/') {
    return false;
  }

  std::stringstream stream(archive_path);
  std::string segment;
  while (std::getline(stream, segment, '/')) {
    if (segment.empty() || segment == "." || segment == "..") {
      return false;
    }
  }
  return true;
}

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

void AppendUnique(std::vector<std::string>* values, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(value);
  }
}

std::string ComputeFnv1a64Checksum(const std::string& contents) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const unsigned char byte : contents) {
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= 1099511628211ull;
  }

  std::ostringstream output;
  output << "fnv1a64:" << std::hex << std::nouppercase << std::setfill('0')
         << std::setw(16) << hash;
  return output.str();
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

std::string RenderDexProofArtifactJson(const NativeApkDexProofReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id)
         << "\",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir)
         << "\",\n"
         << "  \"dex_root\": \"" << EscapeJson(report.dex_root) << "\",\n"
         << "  \"files_count\": " << report.files_count << ",\n"
         << "  \"total_bytes\": " << report.total_bytes << ",\n"
         << "  \"decode_level\": \"" << EscapeJson(report.decode_level)
         << "\",\n"
         << "  \"class_defs_count\": " << report.class_defs_count << ",\n"
         << "  \"files\": [\n";
  for (std::size_t index = 0; index < report.files.size(); ++index) {
    const auto& file = report.files[index];
    output << "    {\n"
           << "      \"entry_name\": \"" << EscapeJson(file.entry_name)
           << "\",\n"
           << "      \"staged_path\": \"" << EscapeJson(file.staged_path)
           << "\",\n"
           << "      \"size_bytes\": " << file.size_bytes << ",\n"
           << "      \"checksum\": \"" << EscapeJson(file.checksum)
           << "\",\n"
           << "      \"valid_dex_magic\": "
           << (file.valid_dex_magic ? "true" : "false") << ",\n"
           << "      \"dex_version\": \"" << EscapeJson(file.dex_version)
           << "\",\n"
           << "      \"dex_file_size\": " << file.dex_file_size << ",\n"
           << "      \"header_size\": " << file.header_size << ",\n"
           << "      \"string_ids_size\": " << file.string_ids_size << ",\n"
           << "      \"type_ids_size\": " << file.type_ids_size << ",\n"
           << "      \"class_defs_count\": " << file.class_defs_count
           << ",\n"
           << "      \"errors\": " << RenderJsonArray(file.errors) << "\n"
           << "    }";
    if (index + 1 != report.files.size()) {
      output << ",";
    }
    output << "\n";
  }
  output << "  ],\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

std::string RenderArtBootstrapArtifactJson(
    const NativeApkArtBootstrapReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id)
         << "\",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir)
         << "\",\n"
         << "  \"asset_bridge_status\": \""
         << EscapeJson(report.asset_bridge_status) << "\",\n"
         << "  \"lifecycle_status\": \""
         << EscapeJson(report.lifecycle_status) << "\",\n"
         << "  \"binder_service_registry_status\": \""
         << EscapeJson(report.binder_service_registry_status) << "\",\n"
         << "  \"art_runtime_required\": "
         << (report.art_runtime_required ? "true" : "false") << ",\n"
         << "  \"art_runtime_available\": "
         << (report.art_runtime_available ? "true" : "false") << ",\n"
         << "  \"dex_bootstrap_ready\": "
         << (report.dex_bootstrap_ready ? "true" : "false") << ",\n"
         << "  \"class_loader_ready\": "
         << (report.class_loader_ready ? "true" : "false") << ",\n"
         << "  \"java_execution_supported\": "
         << (report.java_execution_supported ? "true" : "false") << ",\n"
         << "  \"limitation\": \"" << EscapeJson(report.limitation)
         << "\",\n"
         << "  \"dex_files\": " << RenderJsonArray(report.dex_files) << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

}  // namespace

NativeApkDexBridgeSession::NativeApkDexBridgeSession(
    NativeApkDexBridgeSessionConfig config)
    : config_(std::move(config)) {}

NativeApkDexProofReport NativeApkDexBridgeSession::RunDexProof(
    const OpenedApkArchive& archive) const {
  NativeApkDexProofReport report;
  report.session_id = config_.session_id;
  report.package_name = config_.package_name;
  report.apk_path = config_.apk_path;
  report.staged_dir = config_.staged_dir;
  report.dex_root = config_.dex_root;
  report.artifact_root = config_.artifact_root;
  report.inventory_json_path =
      (fs::path(config_.artifact_root) / "dex-proof.json").string();
  report.decode_level = "header_and_counts";

  fs::create_directories(config_.dex_root);
  fs::create_directories(config_.artifact_root);

  std::vector<ApkArchiveEntry> dex_entries;
  for (const auto& entry : ListApkArchiveEntries(archive)) {
    if (!entry.is_directory && IsDexArchiveEntry(entry.path)) {
      dex_entries.push_back(entry);
    }
  }

  std::sort(dex_entries.begin(), dex_entries.end(),
            [](const ApkArchiveEntry& left, const ApkArchiveEntry& right) {
              return left.path < right.path;
            });

  if (dex_entries.empty()) {
    report.errors.push_back("no_dex_entries_found");
    WriteTextFile(report.inventory_json_path, RenderDexProofArtifactJson(report));
    return report;
  }

  for (const auto& entry : dex_entries) {
    NativeApkDexFileReport file;
    file.entry_name = entry.path;
    if (!IsSafeArchivePath(entry.path)) {
      file.errors.push_back("unsafe_dex_entry");
      AppendUnique(&report.errors, "unsafe_dex_entry:" + entry.path);
      report.files.push_back(file);
      continue;
    }

    const auto read_result = ReadApkArchiveEntry(archive, entry.path);
    if (!read_result.found) {
      file.errors.push_back("dex_entry_missing");
      AppendUnique(&report.errors, "dex_entry_missing:" + entry.path);
      report.files.push_back(file);
      continue;
    }
    if (!read_result.readable) {
      file.errors.push_back("dex_entry_unreadable:" + read_result.failure_reason);
      AppendUnique(&report.errors,
                   "dex_entry_unreadable:" + entry.path + ":" +
                       read_result.failure_reason);
      report.files.push_back(file);
      continue;
    }

    const fs::path staged_path =
        fs::path(config_.dex_root) / fs::path(entry.path).filename();
    fs::create_directories(staged_path.parent_path());
    {
      std::ofstream output(staged_path, std::ios::binary);
      if (!output) {
        file.errors.push_back("dex_stage_write_failed");
        AppendUnique(&report.errors, "dex_stage_write_failed:" + entry.path);
        report.files.push_back(file);
        continue;
      }
      output.write(read_result.contents.data(),
                   static_cast<std::streamsize>(read_result.contents.size()));
      if (!output.good()) {
        file.errors.push_back("dex_stage_write_failed");
        AppendUnique(&report.errors, "dex_stage_write_failed:" + entry.path);
        report.files.push_back(file);
        continue;
      }
    }

    file.staged_path = staged_path.string();
    file.size_bytes = read_result.contents.size();
    file.checksum = ComputeFnv1a64Checksum(read_result.contents);
    report.total_bytes += file.size_bytes;

    if (read_result.contents.size() < 0x70) {
      file.errors.push_back("dex_header_truncated");
      AppendUnique(&report.errors, "dex_header_truncated:" + entry.path);
      report.files.push_back(file);
      continue;
    }

    if (read_result.contents[0] != 'd' || read_result.contents[1] != 'e' ||
        read_result.contents[2] != 'x' || read_result.contents[3] != '\n' ||
        read_result.contents[7] != '\0') {
      file.errors.push_back("dex_magic_invalid");
      AppendUnique(&report.errors, "dex_magic_invalid:" + entry.path);
      report.files.push_back(file);
      continue;
    }

    file.valid_dex_magic = true;
    file.dex_version.assign(read_result.contents.data() + 4, 3);
    file.dex_file_size = ReadLe32(read_result.contents, 32);
    file.header_size = ReadLe32(read_result.contents, 36);
    file.string_ids_size = ReadLe32(read_result.contents, 56);
    file.type_ids_size = ReadLe32(read_result.contents, 64);
    file.class_defs_count = ReadLe32(read_result.contents, 96);
    report.class_defs_count += file.class_defs_count;
    report.files.push_back(file);
  }

  report.files_count = static_cast<int>(report.files.size());
  report.ready = report.files_count > 0 && report.errors.empty();
  WriteTextFile(report.inventory_json_path, RenderDexProofArtifactJson(report));
  return report;
}

NativeApkArtBootstrapReport NativeApkDexBridgeSession::BuildArtBootstrap(
    const NativeApkDexProofReport& dex_report) const {
  NativeApkArtBootstrapReport report;
  report.session_id = config_.session_id;
  report.package_name = config_.package_name;
  report.apk_path = config_.apk_path;
  report.staged_dir = config_.staged_dir;
  report.artifact_root = config_.artifact_root;
  report.bootstrap_json_path =
      (fs::path(config_.artifact_root) / "art-bootstrap.json").string();
  report.asset_bridge_status = config_.asset_bridge_status;
  report.lifecycle_status = config_.lifecycle_status;
  report.binder_service_registry_status =
      config_.binder_service_registry_status;
  report.dex_bootstrap_ready = dex_report.ready;
  report.class_loader_ready = dex_report.ready;
  report.ready = dex_report.ready;
  for (const auto& file : dex_report.files) {
    if (!file.staged_path.empty()) {
      report.dex_files.push_back(file.staged_path);
    }
  }
  report.errors = dex_report.errors;
  fs::create_directories(config_.artifact_root);
  WriteTextFile(report.bootstrap_json_path,
                RenderArtBootstrapArtifactJson(report));
  return report;
}

}  // namespace wfa
