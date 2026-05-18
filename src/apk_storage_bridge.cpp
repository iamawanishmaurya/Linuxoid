#include "wfa/apk_storage_bridge.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace wfa {

namespace fs = std::filesystem;

namespace {

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

void AppendUnique(std::vector<std::string>* values, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(value);
  }
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

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

std::string ReadTextFile(const fs::path& path) {
  std::ifstream input(path);
  if (!input) {
    return "";
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string ComputeFnv1a64(const std::string& contents) {
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

bool IsValidPackageName(const std::string& package_name) {
  static const std::regex pattern("^[A-Za-z0-9_]+(\\.[A-Za-z0-9_]+)+$");
  return std::regex_match(package_name, pattern);
}

bool PathHasPrefix(const fs::path& path, const fs::path& prefix) {
  auto path_it = path.begin();
  auto prefix_it = prefix.begin();
  for (; prefix_it != prefix.end(); ++prefix_it, ++path_it) {
    if (path_it == path.end() || *path_it != *prefix_it) {
      return false;
    }
  }
  return true;
}

std::string NormalizeRelativePath(const std::string& relative_path,
                                  std::vector<std::string>* errors) {
  if (relative_path.empty()) {
    AppendUnique(errors, "empty_relative_path");
    return "";
  }

  const fs::path requested(relative_path);
  if (requested.is_absolute()) {
    AppendUnique(errors, "absolute_path_rejected");
    return "";
  }

  fs::path normalized;
  for (const auto& part : requested) {
    const std::string segment = part.string();
    if (segment.empty() || segment == ".") {
      continue;
    }
    if (segment == "..") {
      AppendUnique(errors, "path_traversal_rejected");
      return "";
    }
    normalized /= part;
  }

  if (normalized.empty()) {
    AppendUnique(errors, "empty_relative_path");
    return "";
  }
  return normalized.generic_string();
}

bool DetectSymlinkEscape(const fs::path& root, const fs::path& relative_path,
                         std::vector<std::string>* errors) {
  std::error_code error;
  const fs::path canonical_root = fs::weakly_canonical(root, error);
  if (error) {
    return false;
  }

  fs::path cursor = root;
  for (const auto& part : relative_path) {
    cursor /= part;
    error.clear();
    if (!fs::exists(cursor, error) && error) {
      return false;
    }
    error.clear();
    if (!fs::is_symlink(cursor, error) || error) {
      continue;
    }
    error.clear();
    const fs::path canonical_cursor = fs::weakly_canonical(cursor, error);
    if (!error && !PathHasPrefix(canonical_cursor, canonical_root)) {
      AppendUnique(errors, "symlink_escape_rejected");
      return true;
    }
  }

  return false;
}

std::string BuildStorageProofJson(const NativeApkStorageProof& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id)
         << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"report_json_path\": \"" << EscapeJson(report.report_json_path)
         << "\",\n"
         << "  \"persisted_state_preexisting\": "
         << (report.persisted_state_preexisting ? "true" : "false") << ",\n"
         << "  \"continuity_validated\": "
         << (report.continuity_validated ? "true" : "false") << ",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir)
         << "\",\n"
         << "  \"app_data_dir\": \"" << EscapeJson(report.app_data_dir)
         << "\",\n"
         << "  \"files_dir\": \"" << EscapeJson(report.files_dir) << "\",\n"
         << "  \"cache_dir\": \"" << EscapeJson(report.cache_dir) << "\",\n"
         << "  \"native_lib_dir\": \"" << EscapeJson(report.native_lib_dir)
         << "\",\n"
         << "  \"asset_root\": \"" << EscapeJson(report.asset_root)
         << "\",\n"
         << "  \"resource_root\": \"" << EscapeJson(report.resource_root)
         << "\",\n"
         << "  \"marker_path\": \"" << EscapeJson(report.marker_path)
         << "\",\n"
         << "  \"marker_size\": " << report.marker_size << ",\n"
         << "  \"marker_preexisting\": "
         << (report.marker_preexisting ? "true" : "false") << ",\n"
         << "  \"marker_reused\": "
         << (report.marker_reused ? "true" : "false") << ",\n"
         << "  \"marker_checksum\": \"" << EscapeJson(report.marker_checksum)
         << "\",\n"
         << "  \"continuity_state\": \""
         << EscapeJson(report.continuity_state) << "\",\n"
         << "  \"uid_placeholder\": " << report.uid_placeholder << ",\n"
         << "  \"gid_placeholder\": " << report.gid_placeholder << ",\n"
         << "  \"isolation_level\": \""
         << EscapeJson(report.isolation_level) << "\",\n"
         << "  \"sandbox_state\": \"" << EscapeJson(report.sandbox_state)
         << "\",\n"
         << "  \"permission_metadata_ready\": "
         << (report.permission_metadata_ready ? "true" : "false") << ",\n"
         << "  \"marker_written\": "
         << (report.marker_written ? "true" : "false") << ",\n"
         << "  \"marker_read_back\": "
         << (report.marker_read_back ? "true" : "false") << ",\n"
         << "  \"rejected_escape_paths\": " << report.rejected_escape_paths
         << ",\n"
         << "  \"accepted_paths\": " << RenderJsonArray(report.accepted_paths)
         << ",\n"
         << "  \"rejected_paths\": " << RenderJsonArray(report.rejected_paths)
         << ",\n"
         << "  \"continuity_diagnostics\": "
         << RenderJsonArray(report.continuity_diagnostics) << ",\n"
         << "  \"permission_metadata\": "
         << RenderJsonArray(report.permission_metadata) << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

}  // namespace

NativeApkStorageBridgeSession::NativeApkStorageBridgeSession(
    NativeApkStorageBridgeContext context)
    : context_(std::move(context)) {}

const NativeApkStorageBridgeContext& NativeApkStorageBridgeSession::context()
    const {
  return context_;
}

NativeApkStoragePathResolution
NativeApkStorageBridgeSession::ResolveAppRelativePath(
    const std::string& relative_path) const {
  NativeApkStoragePathResolution resolution;
  resolution.requested_path = relative_path;

  if (!IsValidPackageName(context_.package_name)) {
    resolution.rejected = true;
    AppendUnique(&resolution.errors, "unsafe_package_name");
    return resolution;
  }

  const std::string normalized =
      NormalizeRelativePath(relative_path, &resolution.errors);
  if (normalized.empty()) {
    resolution.rejected = true;
    return resolution;
  }

  resolution.normalized_relative_path = normalized;
  const fs::path root = fs::path(context_.app_data_dir).lexically_normal();
  const fs::path candidate = (root / normalized).lexically_normal();
  if (!PathHasPrefix(candidate, root)) {
    resolution.rejected = true;
    AppendUnique(&resolution.errors, "path_escape_rejected");
    return resolution;
  }

  if (DetectSymlinkEscape(root, fs::path(normalized), &resolution.errors)) {
    resolution.rejected = true;
    return resolution;
  }

  resolution.accepted = true;
  resolution.resolved_path = candidate.string();
  return resolution;
}

NativeApkStorageProof NativeApkStorageBridgeSession::RunStorageProof() const {
  NativeApkStorageProof report;
  report.session_id = context_.session_id;
  report.artifact_root = context_.artifact_root;
  report.report_json_path =
      (fs::path(context_.artifact_root) / "storage-proof.json").string();
  report.package_name = context_.package_name;
  report.apk_path = context_.apk_path;
  report.staged_dir = context_.staged_dir;
  report.app_data_dir = context_.app_data_dir;
  report.files_dir = context_.files_dir;
  report.cache_dir = context_.cache_dir;
  report.native_lib_dir = context_.native_lib_dir;
  report.asset_root = context_.asset_root;
  report.resource_root = context_.resource_root;
  report.uid_placeholder = context_.uid_placeholder;
  report.gid_placeholder = context_.gid_placeholder;
  report.isolation_level = context_.isolation_level;
  report.sandbox_state = context_.sandbox_state;
  report.permission_metadata = context_.permission_metadata;
  report.permission_metadata_ready = !context_.permission_metadata.empty();
  report.persisted_state_preexisting =
      fs::exists(context_.app_data_dir) || fs::exists(context_.files_dir) ||
      fs::exists(context_.cache_dir);

  if (!IsValidPackageName(context_.package_name)) {
    AppendUnique(&report.errors, "unsafe_package_name");
  }

  try {
    fs::create_directories(context_.artifact_root);
    fs::create_directories(context_.app_data_dir);
    fs::create_directories(context_.files_dir);
    fs::create_directories(context_.cache_dir);
  } catch (const std::exception& error) {
    AppendUnique(&report.errors,
                 "storage_directory_creation_failed:" +
                     std::string(error.what()));
  }

  const auto marker_resolution =
      ResolveAppRelativePath("files/linuxoid-session.marker");
  if (!marker_resolution.accepted) {
    report.errors.insert(report.errors.end(), marker_resolution.errors.begin(),
                         marker_resolution.errors.end());
  } else {
    report.accepted_paths.push_back(marker_resolution.normalized_relative_path);
    report.marker_path = marker_resolution.resolved_path;
    const std::string marker_contents =
        "package=" + context_.package_name + "\n" + "session=" +
        context_.session_id + "\n" + "isolation_level=" +
        context_.isolation_level + "\n";
    report.marker_preexisting = fs::exists(marker_resolution.resolved_path);
    try {
      const std::string existing_contents =
          report.marker_preexisting
              ? ReadTextFile(marker_resolution.resolved_path)
              : std::string();
      if (report.marker_preexisting && existing_contents == marker_contents) {
        report.marker_reused = true;
        report.continuity_validated = true;
        report.continuity_state = "validated_existing_state";
        AppendUnique(&report.continuity_diagnostics,
                     "storage_marker_reused_without_rewrite");
      } else {
        WriteTextFile(marker_resolution.resolved_path, marker_contents);
        report.marker_written = true;
        report.continuity_validated = true;
        if (!report.persisted_state_preexisting && !report.marker_preexisting) {
          report.continuity_state = "initialized_new_state";
          AppendUnique(&report.continuity_diagnostics,
                       "storage_state_initialized_for_first_launch");
        } else if (report.marker_preexisting) {
          report.continuity_state = "repaired_stale_state";
          AppendUnique(&report.continuity_diagnostics,
                       "storage_marker_mismatch_rebuilt");
        } else {
          report.continuity_state = "healed_missing_marker";
          AppendUnique(&report.continuity_diagnostics,
                       "storage_marker_missing_rebuilt");
        }
      }
      const std::string read_back = ReadTextFile(marker_resolution.resolved_path);
      report.marker_read_back = read_back == marker_contents;
      report.marker_size = read_back.size();
      report.marker_checksum = ComputeFnv1a64(read_back);
    } catch (const std::exception& error) {
      AppendUnique(&report.errors,
                   "storage_marker_write_failed:" +
                       std::string(error.what()));
    }
  }

  if (report.continuity_state == "not_checked") {
    if (report.persisted_state_preexisting) {
      report.continuity_state = "existing_state_not_validated";
    } else {
      report.continuity_state = "initialized_new_state";
    }
  }

  for (const std::string unsafe_path : {"../escape.txt", "/tmp/escape.txt"}) {
    const auto resolution = ResolveAppRelativePath(unsafe_path);
    if (resolution.rejected) {
      ++report.rejected_escape_paths;
      report.rejected_paths.push_back(unsafe_path);
    } else {
      AppendUnique(&report.errors,
                   "unsafe_storage_path_not_rejected:" + unsafe_path);
    }
  }

  report.ready = report.errors.empty() &&
                 (report.marker_written || report.marker_reused) &&
                 report.marker_read_back &&
                 fs::exists(report.app_data_dir) && fs::exists(report.files_dir) &&
                 fs::exists(report.cache_dir);

  WriteTextFile(report.report_json_path, BuildStorageProofJson(report));
  return report;
}

}  // namespace wfa
