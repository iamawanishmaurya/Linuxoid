#include "wfa/apk_native_launch.hpp"

#include "wfa/apk_activity_launch_bridge.hpp"
#include "wfa/apk_archive.hpp"
#include "wfa/apk_asset_bridge.hpp"
#include "wfa/apk_dex_bridge.hpp"
#include "wfa/apk_loader.hpp"
#include "wfa/apk_permission_bridge.hpp"
#include "wfa/binder_service_manager.hpp"
#include "wfa/manifest_assessment.hpp"
#include "wfa/native_window_surface.hpp"
#include "wfa/package_layout.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

std::string RenderNativeApkLaunchJson(const NativeApkLaunchReport& report);

namespace {

struct ParsedManifestMetadata {
  bool manifest_present = false;
  bool metadata_ready = false;
  std::string manifest_source;
  std::string manifest_contents;
  std::string package_name;
  std::string version_name;
  int version_code = 0;
  int min_sdk = 0;
  int target_sdk = 0;
  std::string launcher_component;
  std::vector<std::string> activity_names;
  std::vector<std::string> errors;
};

struct LibraryArchiveEntry {
  std::string archive_path;
  std::string file_name;
  std::string abi;
};

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

std::string RenderIntentFilterArray(
    const std::vector<NativeApkIntentFilterRecord>& filters) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < filters.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    output << "{"
           << "\"actions\": " << RenderJsonArray(filters[index].actions) << ", "
           << "\"categories\": " << RenderJsonArray(filters[index].categories)
           << "}";
  }
  output << "]";
  return output.str();
}

std::string RenderActivityComponentArray(
    const std::vector<NativeApkActivityComponentRecord>& activities) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < activities.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& activity = activities[index];
    output << "{"
           << "\"component_name\": \"" << EscapeJson(activity.component_name)
           << "\", "
           << "\"class_name\": \"" << EscapeJson(activity.class_name)
           << "\", "
           << "\"component_kind\": \"" << EscapeJson(activity.component_kind)
           << "\", "
           << "\"label\": \"" << EscapeJson(activity.label) << "\", "
           << "\"exported\": " << (activity.exported ? "true" : "false")
           << ", "
           << "\"enabled\": " << (activity.enabled ? "true" : "false")
           << ", "
           << "\"launcher_candidate\": "
           << (activity.launcher_candidate ? "true" : "false") << ", "
           << "\"exported_source\": \""
           << EscapeJson(activity.exported_source) << "\", "
           << "\"target_activity\": \""
           << EscapeJson(activity.target_activity) << "\", "
           << "\"intent_filters\": "
           << RenderIntentFilterArray(activity.intent_filters) << "}";
  }
  output << "]";
  return output.str();
}

std::string RenderSelfHealingActionsArray(
    const std::vector<SelfHealingAndroidDeviceRecoveryAction>& actions) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < actions.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& action = actions[index];
    output << "{"
           << "\"sequence_id\": " << action.sequence_id << ", "
           << "\"subsystem\": \"" << EscapeJson(action.subsystem) << "\", "
           << "\"reason\": \"" << EscapeJson(action.reason) << "\", "
           << "\"action\": \"" << EscapeJson(action.action) << "\", "
           << "\"result\": \"" << EscapeJson(action.result) << "\", "
           << "\"recoverable\": " << (action.recoverable ? "true" : "false")
           << ", "
           << "\"initial_health\": \"" << EscapeJson(action.initial_health)
           << "\", "
           << "\"final_health\": \"" << EscapeJson(action.final_health)
           << "\"}";
  }
  output << "]";
  return output.str();
}

std::string RenderPermissionRecordsArray(
    const std::vector<NativeApkPermissionRecord>& records) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& record = records[index];
    output << "{"
           << "\"permission_name\": \""
           << EscapeJson(record.permission_name) << "\", "
           << "\"grant_state\": \"" << EscapeJson(record.grant_state)
           << "\", "
           << "\"protection_level\": \""
           << EscapeJson(record.protection_level) << "\", "
           << "\"source\": \"" << EscapeJson(record.source) << "\", "
           << "\"rationale\": \"" << EscapeJson(record.rationale) << "\", "
           << "\"updated_at_unix_ms\": " << record.updated_at_unix_ms << "}";
  }
  output << "]";
  return output.str();
}

std::string RenderAppOpRecordsArray(
    const std::vector<NativeApkAppOpRecord>& records) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& record = records[index];
    output << "{"
           << "\"op_name\": \""
           << EscapeJson(record.operation_name) << "\", "
           << "\"mode\": \"" << EscapeJson(record.mode) << "\", "
           << "\"required_permission\": \""
           << EscapeJson(record.required_permission) << "\", "
           << "\"reason\": \"" << EscapeJson(record.reason) << "\", "
           << "\"source\": \"" << EscapeJson(record.source) << "\", "
           << "\"updated_at_unix_ms\": " << record.updated_at_unix_ms << "}";
  }
  output << "]";
  return output.str();
}

std::string HexUint32(std::uint32_t value) {
  std::ostringstream output;
  output << "0x" << std::hex << std::nouppercase << std::setfill('0')
         << std::setw(8) << value;
  return output.str();
}

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

void AppendError(std::vector<std::string>* errors, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(errors->begin(), errors->end(), value) == errors->end()) {
    errors->push_back(value);
  }
}

std::string ExtractFirstMatch(std::string_view text, const std::regex& pattern) {
  std::match_results<std::string_view::const_iterator> match;
  if (!std::regex_search(text.begin(), text.end(), match, pattern) ||
      match.size() < 2) {
    return {};
  }
  return std::string(match[1].first, match[1].second);
}

int ExtractManifestInt(std::string_view xml, const std::string& attribute_name) {
  const std::regex pattern(attribute_name + "=\"([0-9]+)\"");
  const std::string value = ExtractFirstMatch(xml, pattern);
  if (value.empty()) {
    return 0;
  }
  return std::stoi(value);
}

std::string ExtractManifestString(std::string_view xml,
                                  const std::string& attribute_name) {
  return ExtractFirstMatch(xml,
                           std::regex(attribute_name + "=\"([^\"]+)\""));
}

std::string ResolveHostAbi() {
#if defined(__x86_64__)
  return "x86_64";
#elif defined(__i386__)
  return "x86";
#elif defined(__aarch64__)
  return "arm64-v8a";
#elif defined(__arm__)
  return "armeabi-v7a";
#else
  return "unknown";
#endif
}

bool IsSafeArchivePath(const std::string& archive_path) {
  if (archive_path.empty()) {
    return false;
  }
  if (archive_path.front() == '/') {
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

std::string NormalizeAndroidComponent(const std::string& package_name,
                                      const std::string& class_name) {
  if (package_name.empty() || class_name.empty()) {
    return "";
  }
  if (class_name.front() == '.') {
    return package_name + "/" + class_name;
  }
  if (class_name.rfind(package_name + ".", 0) == 0) {
    return package_name + "/." + class_name.substr(package_name.size() + 1);
  }
  return package_name + "/" + class_name;
}

ParsedManifestMetadata ParseManifestMetadata(
    const OpenedApkArchive& archive, std::vector<std::string>* limitations) {
  ParsedManifestMetadata parsed;
  const auto manifest = ReadApkArchiveEntry(archive, "AndroidManifest.xml");
  parsed.manifest_present = manifest.found;
  if (!manifest.found) {
    parsed.errors.push_back("manifest_missing");
    return parsed;
  }
  if (!manifest.readable) {
    parsed.errors.push_back("manifest_unreadable:" + manifest.failure_reason);
    return parsed;
  }
  if (manifest.contents.find("<manifest") == std::string::npos) {
    parsed.errors.push_back("manifest_plain_xml_required");
    limitations->push_back("binary_xml_manifest_not_supported_yet");
    return parsed;
  }

  parsed.manifest_source = "archive_plain_xml";
  parsed.manifest_contents = manifest.contents;

  try {
    const auto profile = ParseDecodedManifest(manifest.contents);
    parsed.package_name = profile.package_name;
    parsed.activity_names = profile.declared_activity_components;
    if (profile.has_launcher_activity) {
      parsed.launcher_component = NormalizeAndroidComponent(
          profile.package_name, profile.launcher_activity_name);
    } else if (!profile.declared_activity_components.empty()) {
      parsed.launcher_component = NormalizeAndroidComponent(
          profile.package_name, profile.declared_activity_components.front());
    }
  } catch (const std::exception& error) {
    parsed.errors.push_back("manifest_parse_failed:" + std::string(error.what()));
    return parsed;
  }

  parsed.version_code =
      ExtractManifestInt(manifest.contents, "android:versionCode");
  parsed.version_name =
      ExtractManifestString(manifest.contents, "android:versionName");
  parsed.min_sdk =
      ExtractManifestInt(manifest.contents, "android:minSdkVersion");
  parsed.target_sdk =
      ExtractManifestInt(manifest.contents, "android:targetSdkVersion");

  if (parsed.package_name.empty()) {
    parsed.errors.push_back("manifest_package_name_missing");
  }
  if (parsed.version_code <= 0) {
    parsed.errors.push_back("manifest_version_code_missing");
  }
  if (parsed.version_name.empty()) {
    parsed.errors.push_back("manifest_version_name_missing");
  }
  if (parsed.launcher_component.empty()) {
    AppendError(limitations,
                "launcher_component_resolution_deferred_to_activity_contract");
  }

  parsed.metadata_ready = parsed.errors.empty();
  return parsed;
}

bool StageArchiveEntry(const OpenedApkArchive& archive,
                       const std::string& archive_path,
                       const fs::path& destination,
                       std::vector<std::string>* errors) {
  const auto entry = ReadApkArchiveEntry(archive, archive_path);
  if (!entry.found) {
    AppendError(errors, "archive_entry_missing:" + archive_path);
    return false;
  }
  if (!entry.readable) {
    AppendError(errors, "archive_entry_unreadable:" + archive_path + ":" +
                            entry.failure_reason);
    return false;
  }

  fs::create_directories(destination.parent_path());
  std::ofstream output(destination, std::ios::binary);
  if (!output) {
    AppendError(errors, "stage_write_failed:" + destination.string());
    return false;
  }
  output.write(entry.contents.data(),
               static_cast<std::streamsize>(entry.contents.size()));
  return output.good();
}

std::vector<LibraryArchiveEntry> CollectLibraryEntries(
    const OpenedApkArchive& archive) {
  std::vector<LibraryArchiveEntry> libraries;
  for (const auto& entry : ListApkArchiveEntries(archive)) {
    if (entry.is_directory || entry.path.rfind("lib/", 0) != 0 ||
        entry.path.size() < std::string("lib/x/.so").size() ||
        entry.path.substr(entry.path.size() - 3) != ".so") {
      continue;
    }

    const std::size_t abi_start = std::string("lib/").size();
    const std::size_t abi_end = entry.path.find('/', abi_start);
    if (abi_end == std::string::npos || abi_end + 1 >= entry.path.size()) {
      continue;
    }

    libraries.push_back(
        {.archive_path = entry.path,
         .file_name = fs::path(entry.path).filename().string(),
         .abi = entry.path.substr(abi_start, abi_end - abi_start)});
  }

  std::sort(libraries.begin(), libraries.end(),
            [](const LibraryArchiveEntry& left,
               const LibraryArchiveEntry& right) {
              if (left.abi != right.abi) {
                return left.abi < right.abi;
              }
              return left.file_name < right.file_name;
            });
  return libraries;
}

std::string RenderManifestMetadataJson(const ParsedManifestMetadata& parsed) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(parsed.package_name)
         << "\",\n"
         << "  \"version_name\": \"" << EscapeJson(parsed.version_name)
         << "\",\n"
         << "  \"version_code\": " << parsed.version_code << ",\n"
         << "  \"min_sdk\": " << parsed.min_sdk << ",\n"
         << "  \"target_sdk\": " << parsed.target_sdk << ",\n"
         << "  \"launcher_component\": \""
         << EscapeJson(parsed.launcher_component) << "\",\n"
         << "  \"activity_names\": " << RenderJsonArray(parsed.activity_names)
         << "\n"
         << "}\n";
  return output.str();
}

void WriteLaunchBootstrapManifest(const NativeApkLaunchReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"bundle_apk_path\": \"" << EscapeJson(report.bundle_apk_path)
         << "\",\n"
         << "  \"launcher_component\": \""
         << EscapeJson(report.launcher_component) << "\",\n"
         << "  \"sandbox_root\": \"" << EscapeJson(report.sandbox_root)
         << "\",\n"
         << "  \"dex_cache_root\": \"" << EscapeJson(report.dex_cache_root)
         << "\",\n"
         << "  \"resource_root\": \"" << EscapeJson(report.resource_root)
         << "\",\n"
         << "  \"library_root\": \"" << EscapeJson(report.library_root)
         << "\",\n"
         << "  \"selected_abi\": \"" << EscapeJson(report.selected_abi)
         << "\",\n"
         << "  \"staged_native_libraries\": "
         << RenderJsonArray(report.native_libraries) << "\n"
         << "}\n";
  WriteTextFile(report.bootstrap_manifest_path, output.str());
}

NativeWindowMetadata BuildSurfaceMetadata(
    const NativeApkLaunchOptions& options) {
  return {.width = options.surface_width,
          .height = options.surface_height,
          .format = options.surface_format,
          .stride = options.surface_width};
}

std::string DetermineRecommendedRecoveryAction(
    const NativeApkLaunchReport& report) {
  if (report.launch_ready &&
      (!report.surface_proof_requested || report.surface_proof_ready) &&
      (!report.window_proof_requested || report.window_manager.ready) &&
      (!report.asset_proof_requested || report.asset_bridge.ready) &&
      (!report.asset_proof_requested || report.resource_bridge.ready) &&
      (!report.storage_proof_requested || report.storage.ready) &&
      (!report.permissions_proof_requested || report.permissions.ready) &&
      (!report.permissions_proof_requested || report.app_ops.ready) &&
      (!report.lifecycle_proof_requested || report.lifecycle.ready) &&
      (!report.lifecycle_proof_requested || report.looper.ready) &&
      (!report.lifecycle_proof_requested || report.input_queue.ready) &&
      (!report.dex_proof_requested || report.dex.ready) &&
      (!report.dex_proof_requested || report.art_bootstrap.ready) &&
      (!report.activity_proof_requested || report.package_manager.ready) &&
      (!report.activity_proof_requested || report.intent_resolution.ready) &&
      (!report.activity_proof_requested || report.activity_launch.ready) &&
      (!report.process_proof_requested || report.activity_manager.ready) &&
      (!report.process_proof_requested || report.process_manager.ready)) {
    return "none";
  }
  if (std::find(report.errors.begin(), report.errors.end(),
                "apk_path_missing") != report.errors.end() ||
      report.launch_status == "invalid_apk") {
    return "provide_valid_apk_archive";
  }
  if (std::find(report.errors.begin(), report.errors.end(),
                "manifest_missing") != report.errors.end() ||
      report.launch_status == "manifest_metadata_unavailable") {
    return "supply_plain_xml_manifest_metadata";
  }
  if (report.launch_status == "no_native_libraries_found") {
    return "stage_abi_matching_native_library";
  }
  if (report.launch_status == "unsupported_host_abi") {
    return "provide_host_abi_compatible_library";
  }
  if (report.asset_proof_requested && !report.asset_bridge.ready) {
    return "restage_or_repair_assets";
  }
  if (report.asset_proof_requested && !report.resource_bridge.ready) {
    return "restage_or_repair_resource_metadata";
  }
  if (report.storage_proof_requested &&
      (report.storage_health != "ready" || report.sandbox_health != "ready" ||
       !report.storage.ready)) {
    return "repair_app_storage";
  }
  if (report.permissions_proof_requested &&
      (report.permission_health != "ready" || !report.permissions.ready)) {
    return "rebuild_permission_state";
  }
  if (report.permissions_proof_requested &&
      (report.app_ops_health != "ready" || !report.app_ops.ready)) {
    return "rebuild_permission_state";
  }
  if (report.lifecycle_proof_requested && !report.lifecycle.ready) {
    return "rebuild_lifecycle_controller";
  }
  if (report.lifecycle_proof_requested && !report.looper.ready) {
    return "restart_looper_event_pump";
  }
  if (report.lifecycle_proof_requested && !report.input_queue.ready) {
    return "recreate_input_queue";
  }
  if (report.dex_proof_requested && !report.dex.ready) {
    if (std::find(report.errors.begin(), report.errors.end(),
                  "no_dex_entries_found") != report.errors.end()) {
      return "stage_or_restore_dex_payload";
    }
    return "repair_dex_metadata_probe";
  }
  if (report.dex_proof_requested && !report.art_bootstrap.ready) {
    return "rebuild_class_loader_bootstrap_contract";
  }
  if (report.activity_proof_requested && report.binder_health != "ready") {
    return "materialize_local_service_registry";
  }
  if (report.activity_proof_requested && !report.package_manager.ready) {
    return "rebuild_package_record";
  }
  if (report.activity_proof_requested && !report.intent_resolution.ready) {
    if (report.intent_resolution.blocking_reason == "package_not_found") {
      return "select_staged_package_name";
    }
    if (report.intent_resolution.blocking_reason == "ambiguous_launcher_activities") {
      return "select_explicit_component";
    }
    if (report.intent_resolution.blocking_reason == "component_disabled") {
      return "enable_activity_component";
    }
    if (report.intent_resolution.blocking_reason == "component_not_exported") {
      return "use_exported_activity_component";
    }
    if (report.intent_resolution.blocking_reason == "unsupported_component_type") {
      return "select_activity_component";
    }
    if (report.intent_resolution.blocking_reason == "unresolved_activity_class") {
      return "repair_activity_class_name";
    }
    return "repair_launcher_intent_filters";
  }
  if (report.activity_proof_requested && !report.activity_launch.ready) {
    if (report.activity_launch.blocking_reason == "surface_not_ready") {
      return "recreate_native_surface_session";
    }
    if (report.activity_launch.blocking_reason == "lifecycle_not_ready") {
      return "rebuild_lifecycle_controller";
    }
    if (report.activity_launch.blocking_reason == "looper_not_ready") {
      return "restart_looper_event_pump";
    }
    if (report.activity_launch.blocking_reason == "input_not_ready") {
      return "recreate_input_queue";
    }
    if (report.activity_launch.blocking_reason == "dex_not_ready") {
      return "stage_or_restore_dex_payload";
    }
    if (report.activity_launch.blocking_reason == "art_bootstrap_not_ready") {
      return "rebuild_class_loader_bootstrap_contract";
    }
    if (report.activity_launch.blocking_reason == "binder_not_ready") {
      return "materialize_local_service_registry";
    }
    return "rebuild_activity_launch_contract";
  }
  if (report.process_proof_requested &&
      (report.activity_manager_health != "ready" ||
       !report.activity_manager.ready || report.process_health != "ready" ||
       !report.process_manager.ready)) {
    return "rebuild_process_manager_state";
  }
  if (report.window_proof_requested &&
      (report.window_health != "ready" || !report.window_manager.ready)) {
    return report.window_manager.recommended_recovery_action == "none"
               ? "rebuild_window_manager_state"
               : report.window_manager.recommended_recovery_action;
  }
  if (report.surface_proof_requested && !report.surface_proof_ready) {
    return "recreate_native_surface_session";
  }
  return "inspect_native_launch_diagnostics";
}

bool DetermineRecoverable(const NativeApkLaunchReport& report) {
  if (report.launch_ready &&
      (!report.surface_proof_requested || report.surface_proof_ready) &&
      (!report.window_proof_requested || report.window_manager.ready) &&
      (!report.asset_proof_requested || report.asset_bridge.ready) &&
      (!report.asset_proof_requested || report.resource_bridge.ready) &&
      (!report.storage_proof_requested || report.storage.ready) &&
      (!report.permissions_proof_requested || report.permissions.ready) &&
      (!report.permissions_proof_requested || report.app_ops.ready) &&
      (!report.lifecycle_proof_requested || report.lifecycle.ready) &&
      (!report.lifecycle_proof_requested || report.looper.ready) &&
      (!report.lifecycle_proof_requested || report.input_queue.ready) &&
      (!report.dex_proof_requested || report.dex.ready) &&
      (!report.dex_proof_requested || report.art_bootstrap.ready) &&
      (!report.activity_proof_requested || report.package_manager.ready) &&
      (!report.activity_proof_requested || report.intent_resolution.ready) &&
      (!report.activity_proof_requested || report.activity_launch.ready) &&
      (!report.process_proof_requested || report.activity_manager.ready) &&
      (!report.process_proof_requested || report.process_manager.ready)) {
    return false;
  }
  return report.launch_status != "invalid_apk" &&
         report.launch_status != "manifest_metadata_unavailable";
}

NativeApkAssetBridgeSession BuildAssetBridgeSession(
    const NativeApkLaunchReport& report) {
  const std::string selected_library_path =
      !report.native_execute.selected_library_path.empty()
          ? report.native_execute.selected_library_path
          : (report.native_libraries.empty() ? ""
                                             : report.native_libraries.front());
  return NativeApkAssetBridgeSession(
      {.session_id = report.package_name + ":" + report.install_id + ":assets",
       .package_name = report.package_name,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .selected_library_path = selected_library_path,
       .launch_status = report.launch_status,
       .asset_root = report.asset_root,
       .resource_root = report.resource_root,
       .manifest_source = report.manifest_source});
}

NativeApkLifecycleBridgeSession BuildLifecycleBridgeSession(
    const NativeApkLaunchReport& report, const NativeApkLaunchOptions& options) {
  const std::string selected_library_path =
      !report.native_execute.selected_library_path.empty()
          ? report.native_execute.selected_library_path
          : (report.native_libraries.empty() ? ""
                                             : report.native_libraries.front());
  const bool surface_ready =
      report.surface.surface_created || report.surface_proof_ready;
  const int width = surface_ready && report.surface.width > 0
                        ? report.surface.width
                        : options.surface_width;
  const int height = surface_ready && report.surface.height > 0
                         ? report.surface.height
                         : options.surface_height;
  const int format = surface_ready && report.surface.format > 0
                         ? report.surface.format
                         : options.surface_format;
  return NativeApkLifecycleBridgeSession(
      {.session_id = report.package_name + ":" + report.install_id + ":lifecycle",
       .package_name = report.package_name,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .selected_abi = report.selected_abi,
       .selected_library_path = selected_library_path,
       .launch_status = report.launch_status,
       .asset_health = report.asset_health,
       .resource_health = report.resource_health,
       .surface_health = report.surface_health,
       .surface_state = report.surface.state,
       .artifact_root = (fs::path(report.staged_dir) / "lifecycle-proof").string(),
       .width = width,
       .height = height,
       .format = format});
}

std::string DetectBinderServiceRegistryStatus(
    const NativeApkLaunchReport& report) {
  const fs::path service_manager_path =
      fs::path(report.staged_dir) / "binder" / "service-manager.json";
  return fs::exists(service_manager_path) ? "local_foundation_ready"
                                          : "not_present";
}

NativeApkDexBridgeSession BuildDexBridgeSession(
    const NativeApkLaunchReport& report) {
  return NativeApkDexBridgeSession(
      {.session_id = report.package_name + ":" + report.install_id + ":art",
       .package_name = report.package_name,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .dex_root = (fs::path(report.staged_dir) / "dex").string(),
       .artifact_root = (fs::path(report.staged_dir) / "art").string(),
       .asset_bridge_status = report.asset_health,
       .lifecycle_status = report.lifecycle_health,
       .binder_service_registry_status =
           DetectBinderServiceRegistryStatus(report)});
}

NativeApkStorageBridgeSession BuildStorageBridgeSession(
    const NativeApkLaunchReport& report) {
  const fs::path app_data_dir =
      fs::path(report.sandbox_root) / "data" / "data" / report.package_name;
  const std::vector<std::string> permission_metadata = {
      "uid_placeholder=10000",
      "gid_placeholder=10000",
      "app_data_dir_mode=0700",
      "files_dir_mode=0700",
      "cache_dir_mode=0700"};
  return NativeApkStorageBridgeSession(
      {.session_id = report.package_name + ":" + report.install_id + ":storage",
       .package_name = report.package_name,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .app_data_dir = app_data_dir.string(),
       .files_dir = (app_data_dir / "files").string(),
       .cache_dir = (app_data_dir / "cache").string(),
       .native_lib_dir = report.library_root,
       .asset_root = report.asset_root,
       .resource_root = report.resource_root,
       .artifact_root = (fs::path(report.staged_dir) / "storage").string(),
       .uid_placeholder = 10000,
       .gid_placeholder = 10000,
       .isolation_level = "path_sandbox_only",
       .sandbox_state = "path_sandbox_only",
       .permission_metadata = permission_metadata});
}

NativeApkPermissionBridgeSession BuildPermissionBridgeSession(
    const NativeApkLaunchReport& report, const ParsedManifestMetadata& manifest) {
  const fs::path app_data_dir =
      fs::path(report.sandbox_root) / "data" / "data" / report.package_name;
  const fs::path permissions_root = app_data_dir / "permissions";
  return NativeApkPermissionBridgeSession(
      {.session_id =
           report.package_name + ":" + report.install_id + ":permissions",
       .package_name = report.package_name,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .sandbox_root = report.sandbox_root,
       .app_data_dir = app_data_dir.string(),
       .artifact_root = permissions_root.string(),
       .manifest_source = report.manifest_source,
       .manifest_contents = manifest.manifest_contents,
       .user_id = 0,
       .app_id = 10000,
       .storage_health = report.storage_health,
       .binder_health = report.binder_health,
       .dex_health = report.dex_health,
       .art_health = report.art_health,
       .activity_health = report.activity_health,
       .storage_proof_requested = report.storage_proof_requested,
       .activity_proof_requested = report.activity_proof_requested,
       .dex_proof_requested = report.dex_proof_requested});
}

NativeApkProcessManagerSession BuildProcessManagerBridgeSession(
    const NativeApkLaunchReport& report, bool allow_persisted_contract_repair) {
  const fs::path app_data_dir =
      !report.storage.app_data_dir.empty()
          ? fs::path(report.storage.app_data_dir)
          : (fs::path(report.sandbox_root) / "data" / "data" /
             report.package_name);
  const fs::path artifact_root = app_data_dir / "process-manager";
  return NativeApkProcessManagerSession(
      {.session_id =
           report.package_name + ":" + report.install_id + ":process-manager",
       .package_name = report.package_name,
       .requested_package_name = report.requested_package_name,
       .requested_component = report.requested_component,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .sandbox_root = report.sandbox_root,
       .app_data_dir = app_data_dir.string(),
       .artifact_root = artifact_root.string(),
       .install_id = report.install_id,
       .version_name = report.version_name,
       .version_code = report.version_code,
       .user_id = report.permissions.user_id,
       .app_id = report.permissions.app_id,
       .uid_placeholder = report.storage.uid_placeholder,
       .gid_placeholder = report.storage.gid_placeholder,
       .launch_status = report.launch_status,
       .launch_ready = report.launch_ready,
       .recoverable = report.recoverable,
       .launcher_component = report.launcher_component,
       .resolved_component = report.intent_resolution.resolved_component,
       .resolution_status = report.intent_resolution.resolution_status,
       .resolution_mode = report.intent_resolution.resolution_mode,
       .resolution_reason = report.intent_resolution.resolution_reason,
       .resolution_blocking_reason = report.intent_resolution.blocking_reason,
       .resolution_recovery_action =
           report.intent_resolution.recommended_recovery_action,
       .activity_launch_status = report.activity_launch.activity_launch_status,
       .activity_launch_blocking_reason =
           report.activity_launch.blocking_reason,
       .activity_launch_recovery_action =
           report.activity_launch.recommended_recovery_action,
       .intent_action = report.intent_resolution.action.empty()
                            ? "android.intent.action.MAIN"
                            : report.intent_resolution.action,
       .intent_categories = report.intent_resolution.categories.empty()
                                ? std::vector<std::string>{
                                      "android.intent.category.LAUNCHER"}
                                : report.intent_resolution.categories,
       .lifecycle_state = report.lifecycle.current_state,
       .surface_health = report.surface_health,
       .lifecycle_health = report.lifecycle_health,
       .looper_health = report.looper_health,
       .input_health = report.input_health,
       .dex_health = report.dex_health,
       .art_health = report.art_health,
       .binder_health = report.binder_health,
       .activity_health = report.activity_health,
       .storage_health = report.storage_health,
       .sandbox_health = report.sandbox_health,
       .permission_health = report.permission_health,
       .app_ops_health = report.app_ops_health,
       .package_manager_ready = report.package_manager.ready,
       .intent_resolution_ready = report.intent_resolution.ready,
       .activity_launch_ready = report.activity_launch.ready,
       .dex_bootstrap_ready = report.art_bootstrap.dex_bootstrap_ready,
       .art_runtime_available = report.art_bootstrap.art_runtime_available,
       .java_execution_supported =
           report.art_bootstrap.java_execution_supported,
       .persisted_artifact_root_preexisting = fs::exists(artifact_root),
       .allow_persisted_contract_repair = allow_persisted_contract_repair});
}

NativeApkWindowManagerSession BuildWindowManagerBridgeSession(
    const NativeApkLaunchReport& report, bool allow_persisted_contract_repair,
    bool surface_recovered = false) {
  const fs::path app_data_dir =
      !report.storage.app_data_dir.empty()
          ? fs::path(report.storage.app_data_dir)
          : (fs::path(report.sandbox_root) / "data" / "data" /
             report.package_name);
  const fs::path artifact_root = app_data_dir / "window-manager";
  return NativeApkWindowManagerSession(
      {.session_id =
           report.package_name + ":" + report.install_id + ":window-manager",
       .package_name = report.package_name,
       .requested_package_name = report.requested_package_name,
       .requested_component = report.requested_component,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .sandbox_root = report.sandbox_root,
       .app_data_dir = app_data_dir.string(),
       .artifact_root = artifact_root.string(),
       .install_id = report.install_id,
       .version_name = report.version_name,
       .version_code = report.version_code,
       .user_id = report.permissions.user_id,
       .app_id = report.permissions.app_id,
       .uid_placeholder = report.storage.uid_placeholder,
       .gid_placeholder = report.storage.gid_placeholder,
       .launch_status = report.launch_status,
       .launch_ready = report.launch_ready,
       .recoverable = report.recoverable,
       .launcher_component = report.launcher_component,
       .resolved_component = report.intent_resolution.resolved_component,
       .activity_launch_status = report.activity_launch.activity_launch_status,
       .activity_launch_blocking_reason =
           report.activity_launch.blocking_reason,
       .activity_launch_recovery_action =
           report.activity_launch.recommended_recovery_action,
       .lifecycle_state = report.lifecycle.current_state,
       .process_identity = report.process_manager.process_identity,
       .process_name = report.process_manager.process_name,
       .pid_value = report.process_manager.pid_value,
       .pid_source = report.process_manager.pid_source,
       .surface_session_id = report.surface.session_id,
       .surface_session_root = report.surface.session_root,
       .surface_metadata_path = report.surface.metadata_path,
       .surface_event_log_path = report.surface.event_log_path,
       .surface_marker_path = report.surface.marker_path,
       .surface_state = report.surface.state,
       .surface_backend = report.surface.backend,
       .surface_backing_mode = report.surface.backing_mode,
       .surface_width = report.surface.width,
       .surface_height = report.surface.height,
       .surface_format = report.surface.format,
       .surface_first_frame_presented = report.surface.first_frame_presented,
       .surface_created = report.surface.surface_created,
       .wayland_surface_available = report.surface.wayland_surface_available,
       .egl_surface_available = report.surface.egl_surface_available,
       .surface_recovered = surface_recovered,
       .storage_health = report.storage_health,
       .sandbox_health = report.sandbox_health,
       .permission_health = report.permission_health,
       .app_ops_health = report.app_ops_health,
       .binder_health = report.binder_health,
       .surface_health = report.surface_health,
       .lifecycle_health = report.lifecycle_health,
       .looper_health = report.looper_health,
       .input_health = report.input_health,
       .dex_health = report.dex_health,
       .art_health = report.art_health,
       .activity_health = report.activity_health,
       .activity_manager_health = report.activity_manager_health,
       .process_health = report.process_health,
       .activity_manager_ready = report.activity_manager.ready,
       .process_ready = report.process_manager.ready,
       .persisted_artifact_root_preexisting = fs::exists(artifact_root),
       .allow_persisted_contract_repair = allow_persisted_contract_repair});
}

NativeApkActivityLaunchBridgeSession BuildActivityLaunchBridgeSession(
    const NativeApkLaunchReport& report, const ParsedManifestMetadata& manifest) {
  return NativeApkActivityLaunchBridgeSession(
      {.session_id =
           report.package_name + ":" + report.install_id + ":activity-launch",
       .package_name = report.package_name,
       .requested_package_name = report.requested_package_name,
       .requested_component = report.requested_component,
       .apk_path = report.apk_path,
       .staged_dir = report.staged_dir,
       .install_id = report.install_id,
       .version_name = report.version_name,
       .version_code = report.version_code,
       .manifest_source = report.manifest_source,
       .manifest_contents = manifest.manifest_contents,
       .launcher_component = report.launcher_component,
       .declared_activities = report.declared_activities,
       .staged_native_libraries = report.native_libraries,
       .launch_ready = report.launch_ready,
       .recoverable = report.recoverable,
       .art_runtime_available = report.art_bootstrap.art_runtime_available,
       .java_execution_supported = report.art_bootstrap.java_execution_supported,
       .dex_bootstrap_ready = report.art_bootstrap.dex_bootstrap_ready,
       .launch_status = report.launch_status,
       .surface_health = report.surface_health,
       .lifecycle_health = report.lifecycle_health,
       .looper_health = report.looper_health,
       .input_health = report.input_health,
       .dex_health = report.dex_health,
       .art_health = report.art_health,
       .binder_health = report.binder_health,
       .binder_service_registry_status =
           DetectBinderServiceRegistryStatus(report),
       .binder_service_manager_path =
           (fs::path(report.staged_dir) / "binder" / "service-manager.json")
               .string(),
       .lifecycle_states_visited = report.lifecycle.states_visited,
       .lifecycle_current_state = report.lifecycle.current_state,
       .artifact_root = (fs::path(report.staged_dir) / "activity-launch")
                            .string()});
}

BinderServiceManagerFixtureReport MaterializeBinderFoundation(
    const NativeApkLaunchReport& report) {
  return RunBinderServiceManagerFixture(
      {.package_name = report.package_name,
       .launcher_component = report.launcher_component,
       .apk_path = report.bundle_apk_path,
       .artifact_root = report.staged_dir,
       .session_id =
           report.package_name + ":" + report.install_id + ":binder",
       .owner_process_identity =
           "linuxoid-launch-apk:" + report.package_name});
}

void PopulateRequestedProofFailures(NativeApkLaunchReport* report) {
  if (report->surface_proof_requested) {
    report->surface.state = "blocked";
    report->surface.errors = report->errors;
    report->surface_health = "blocked";
  } else {
    report->surface_health = "not_requested";
  }
  if (report->asset_proof_requested) {
    report->asset_bridge.errors = report->errors;
    report->resource_bridge.errors = report->errors;
    report->asset_health = "blocked";
    report->resource_health = "blocked";
  } else {
    report->asset_health = "not_requested";
    report->resource_health = "not_requested";
  }
  if (report->lifecycle_proof_requested) {
    report->lifecycle.errors = report->errors;
    report->looper.errors = report->errors;
    report->input_queue.errors = report->errors;
    report->lifecycle_health = "blocked";
    report->looper_health = "blocked";
    report->input_health = "blocked";
  } else {
    report->lifecycle_health = "not_requested";
    report->looper_health = "not_requested";
    report->input_health = "not_requested";
  }
  if (report->dex_proof_requested) {
    report->dex.errors = report->errors;
    report->art_bootstrap.errors = report->errors;
    report->dex_health = "blocked";
    report->art_health = "blocked";
  } else {
    report->dex_health = "not_requested";
    report->art_health = "not_requested";
  }
  if (report->storage_proof_requested) {
    report->storage.errors = report->errors;
    report->storage_health = "blocked";
    report->sandbox_health = "blocked";
  } else {
    report->storage_health = "not_requested";
    report->sandbox_health = "not_requested";
  }
  if (report->permissions_proof_requested) {
    report->permissions.errors = report->errors;
    report->app_ops.errors = report->errors;
    report->permission_health = "blocked";
    report->app_ops_health = "blocked";
  } else {
    report->permission_health = "not_requested";
    report->app_ops_health = "not_requested";
  }
  if (report->process_proof_requested) {
    report->activity_manager.errors = report->errors;
    report->process_manager.errors = report->errors;
    report->activity_manager_health = "blocked";
    report->process_health = "blocked";
  } else {
    report->activity_manager_health = "not_requested";
    report->process_health = "not_requested";
  }
  if (report->window_proof_requested) {
    report->window_manager.errors = report->errors;
    report->window_health = "blocked";
  } else {
    report->window_health = "not_requested";
  }
  if (report->activity_proof_requested) {
    report->binder_health = "blocked";
    report->activity_health = "blocked";
    report->package_manager.errors = report->errors;
    report->intent_resolution.errors = report->errors;
    report->activity_launch.errors = report->errors;
  } else {
    report->binder_health = "not_requested";
    report->activity_health = "not_requested";
  }
}

void ApplySimulatedSubsystemFaults(NativeApkLaunchReport* report,
                                   const NativeApkLaunchOptions& options) {
  if (options.simulate_missing_asset_bridge && report->asset_proof_requested) {
    report->asset_bridge.ready = false;
    report->asset_bridge.errors = {"simulated_asset_bridge_missing"};
    report->asset_health = "blocked";
    AppendError(&report->errors, "simulated_asset_bridge_missing");
  }

  if (options.simulate_blocked_surface_proof &&
      report->surface_proof_requested) {
    report->surface_proof_ready = false;
    report->surface.surface_created = false;
    report->surface.surface_configured = false;
    report->surface.first_frame_requested = false;
    report->surface.first_frame_presented = false;
    report->surface.cleanup_ready = false;
    report->surface.state = "simulated_surface_blocked";
    report->surface.errors = {"simulated_surface_blocked"};
    report->surface_health = "blocked";
    AppendError(&report->errors, "simulated_surface_blocked");
  }

  if (options.simulate_missing_binder_service &&
      report->activity_proof_requested) {
    std::error_code ignored;
    fs::remove(fs::path(report->staged_dir) / "binder" / "service-manager.json",
               ignored);
    report->binder_health = "blocked";
    report->activity_health = "blocked";
    report->package_manager.binder_service_registry_status = "not_present";
    report->package_manager.binder_service_manager_path =
        (fs::path(report->staged_dir) / "binder" / "service-manager.json")
            .string();
    report->activity_launch.binder_service_registry_status = "not_present";
    report->activity_launch.binder_service_manager_path =
        report->package_manager.binder_service_manager_path;
    AppendError(&report->errors, "simulated_binder_service_missing");
  }

  if (options.simulate_failed_dex_bootstrap && report->dex_proof_requested) {
    report->dex.ready = false;
    report->art_bootstrap.ready = false;
    report->art_bootstrap.dex_bootstrap_ready = false;
    report->art_bootstrap.class_loader_ready = false;
    report->dex_health = "blocked";
    report->art_health = "blocked";
    AppendError(&report->dex.errors, "simulated_dex_bootstrap_failure");
    AppendError(&report->art_bootstrap.errors,
                "simulated_dex_bootstrap_failure");
    AppendError(&report->errors, "simulated_dex_bootstrap_failure");
  }

  if (options.simulate_failed_intent_resolution &&
      report->activity_proof_requested) {
    report->intent_resolution.ready = false;
    report->intent_resolution.resolution_status = "blocked";
    report->intent_resolution.blocking_reason = "simulated_intent_resolution_failure";
    report->intent_resolution.recommended_recovery_action =
        "rerun_intent_resolution";
    report->intent_resolution.errors = {"simulated_intent_resolution_failure"};
    report->activity_launch.ready = false;
    report->activity_launch.activity_launch_status =
        "activity_launch_dependency_blocked";
    report->activity_launch.blocking_reason = "intent_resolution_not_ready";
    report->activity_launch.recommended_recovery_action =
        "rerun_intent_resolution";
    report->activity_launch.dependency_blocked = true;
    report->activity_health = "blocked";
    AppendError(&report->errors, "simulated_intent_resolution_failure");
  }

  if (options.simulate_storage_failure && report->storage_proof_requested) {
    std::error_code ignored;
    fs::remove_all(report->storage.app_data_dir, ignored);
    report->storage.ready = false;
    report->storage.marker_written = false;
    report->storage.marker_read_back = false;
    report->storage.errors = {"simulated_storage_failure"};
    report->storage_health = "blocked";
    report->sandbox_health = "blocked";
    AppendError(&report->errors, "simulated_storage_failure");
  }

  if (options.simulate_permission_mismatch &&
      report->permissions_proof_requested) {
    report->permissions.ready = false;
    report->permissions.errors = {"simulated_permission_mismatch"};
    report->app_ops.ready = false;
    report->app_ops.errors = {"simulated_permission_mismatch"};
    report->permission_health = "blocked";
    report->app_ops_health = "blocked";
    AppendError(&report->errors, "simulated_permission_mismatch");
  }
}

NativeApkLaunchReport FinalizeNativeApkLaunchReport(
    NativeApkLaunchReport report, const NativeApkLaunchOptions& options) {
  if (options.self_heal_proof_requested) {
    report.self_healing_android_device =
        SelfHealingAndroidDeviceWatchdog(report).Run();
  }
  if (!report.report_json_path.empty()) {
    WriteTextFile(report.report_json_path, RenderNativeApkLaunchJson(report));
  }
  return report;
}

void WriteSurfaceSessionMetadata(const NativeApkSurfaceSession& surface) {
  if (surface.metadata_path.empty()) {
    return;
  }
  std::ostringstream output;
  output << "{\n"
         << "  \"session_id\": \"" << EscapeJson(surface.session_id)
         << "\",\n"
         << "  \"package_name\": \"" << EscapeJson(surface.package_name)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(surface.apk_path) << "\",\n"
         << "  \"staged_dir\": \"" << EscapeJson(surface.staged_dir)
         << "\",\n"
         << "  \"selected_library_path\": \""
         << EscapeJson(surface.selected_library_path) << "\",\n"
         << "  \"backend\": \"" << EscapeJson(surface.backend) << "\",\n"
         << "  \"backing_mode\": \"" << EscapeJson(surface.backing_mode)
         << "\",\n"
         << "  \"bridge_metadata_path\": \""
         << EscapeJson(surface.bridge_metadata_path) << "\",\n"
         << "  \"bridge_event_log_path\": \""
         << EscapeJson(surface.bridge_event_log_path) << "\",\n"
         << "  \"state\": \"" << EscapeJson(surface.state) << "\",\n"
         << "  \"width\": " << surface.width << ",\n"
         << "  \"height\": " << surface.height << ",\n"
         << "  \"format\": " << surface.format << ",\n"
         << "  \"surface_created\": "
         << (surface.surface_created ? "true" : "false") << ",\n"
         << "  \"surface_configured\": "
         << (surface.surface_configured ? "true" : "false") << ",\n"
         << "  \"first_frame_requested\": "
         << (surface.first_frame_requested ? "true" : "false") << ",\n"
         << "  \"first_frame_presented\": "
         << (surface.first_frame_presented ? "true" : "false") << ",\n"
         << "  \"cleanup_ready\": "
         << (surface.cleanup_ready ? "true" : "false") << ",\n"
         << "  \"first_pixel_marker\": \""
         << EscapeJson(surface.first_pixel_marker) << "\",\n"
         << "  \"marker_checksum\": \"" << EscapeJson(surface.marker_checksum)
         << "\",\n"
         << "  \"lifecycle_states\": "
         << RenderJsonArray(surface.lifecycle_states) << ",\n"
         << "  \"errors\": " << RenderJsonArray(surface.errors) << "\n"
         << "}\n";
  WriteTextFile(surface.metadata_path, output.str());
}

std::vector<std::string> SplitNonEmptyLines(const std::string& text) {
  std::vector<std::string> lines;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}

NativeExecuteReport ExecuteNativeStubWithCapturedStdio(
    const NativeExecuteRequest& request, const fs::path& log_path,
    std::vector<std::string>* errors, std::vector<std::string>* diagnostics) {
  NativeExecuteReport report;

  std::fflush(stdout);
  std::fflush(stderr);

  const int saved_stdout = ::dup(STDOUT_FILENO);
  const int saved_stderr = ::dup(STDERR_FILENO);
  const int log_fd =
      ::open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (saved_stdout < 0 || saved_stderr < 0 || log_fd < 0) {
    if (saved_stdout >= 0) {
      ::close(saved_stdout);
    }
    if (saved_stderr >= 0) {
      ::close(saved_stderr);
    }
    if (log_fd >= 0) {
      ::close(log_fd);
    }
    AppendError(errors, "native_execute_stdio_capture_unavailable");
    return ExecuteNativeStub(request);
  }

  bool capture_active = false;
  if (::dup2(log_fd, STDOUT_FILENO) >= 0 &&
      ::dup2(log_fd, STDERR_FILENO) >= 0) {
    capture_active = true;
  } else {
    AppendError(errors, "native_execute_stdio_redirect_failed");
  }
  ::close(log_fd);

  if (!capture_active) {
    std::fflush(stdout);
    std::fflush(stderr);
    ::dup2(saved_stdout, STDOUT_FILENO);
    ::dup2(saved_stderr, STDERR_FILENO);
    ::close(saved_stdout);
    ::close(saved_stderr);
    return ExecuteNativeStub(request);
  }

  report = ExecuteNativeStub(request);

  std::fflush(stdout);
  std::fflush(stderr);
  ::dup2(saved_stdout, STDOUT_FILENO);
  ::dup2(saved_stderr, STDERR_FILENO);
  ::close(saved_stdout);
  ::close(saved_stderr);

  std::ifstream input(log_path);
  if (input) {
    std::ostringstream buffer;
    buffer << input.rdbuf();
    *diagnostics = SplitNonEmptyLines(buffer.str());
  }

  return report;
}

NativeApkSurfaceSession RunNativeApkSurfaceProof(
    const NativeApkLaunchReport& launch_report,
    const NativeApkLaunchOptions& options) {
  NativeApkSurfaceSession surface;
  surface.package_name = launch_report.package_name;
  surface.apk_path = launch_report.apk_path;
  surface.staged_dir = launch_report.staged_dir;
  surface.selected_library_path = launch_report.native_execute.selected_library_path;
  surface.session_id =
      launch_report.package_name + ":" + launch_report.install_id + ":surface";
  surface.session_root =
      (fs::path(launch_report.staged_dir) / "surface-session").string();
  surface.metadata_path =
      (fs::path(surface.session_root) / "surface-session.json").string();
  surface.event_log_path =
      (fs::path(surface.session_root) / "surface-events.jsonl").string();
  surface.backend = "headless";
  surface.backing_mode = "headless_fallback";
  surface.bridge_metadata_path =
      (fs::path(surface.session_root) / "bridge" /
       "native-window-bridge-metadata.json")
          .string();
  surface.bridge_event_log_path =
      (fs::path(surface.session_root) / "bridge" /
       "native-window-bridge-events.jsonl")
          .string();

  fs::create_directories(surface.session_root);

  if (!launch_report.launch_ready) {
    surface.state = "blocked";
    surface.errors.push_back("launch_not_ready_for_surface_proof");
    surface.errors.insert(surface.errors.end(), launch_report.errors.begin(),
                          launch_report.errors.end());
    WriteTextFile(surface.event_log_path, "");
    WriteSurfaceSessionMetadata(surface);
    return surface;
  }

  const NativeWindowMetadata metadata = BuildSurfaceMetadata(options);
  const auto bridge = RunNativeWindowBridgeFixture(
      (fs::path(surface.session_root) / "bridge").string(), metadata);
  surface.backing_mode = bridge.backing_mode;
  surface.bridge_metadata_path = bridge.metadata_path;
  surface.bridge_event_log_path = bridge.event_log_path;
  surface.wayland_surface_available = bridge.wayland_surface_created;
  surface.egl_surface_available = bridge.egl_pbuffer_created;
  if (!bridge.native_window_bridge_ready) {
    surface.state = "surface_creation_failed";
    surface.errors.push_back(bridge.exit_reason);
    WriteTextFile(surface.event_log_path, "");
    WriteSurfaceSessionMetadata(surface);
    return surface;
  }

  surface.width = bridge.width;
  surface.height = bridge.height;
  surface.format = bridge.format;
  surface.surface_created = true;
  surface.lifecycle_states.push_back("created");
  surface.surface_configured = true;
  surface.lifecycle_states.push_back("configured");
  surface.first_frame_requested = true;
  surface.lifecycle_states.push_back("first_frame_requested");

  constexpr std::uint32_t kSurfacePixelMarker = 0x1ee7c0de;
  const auto first_frame = RunHeadlessFirstPixelFixture(
      surface.session_root, metadata, kSurfacePixelMarker);
  surface.marker_path = first_frame.surface.marker_path;
  surface.first_frame_presented = first_frame.render_ready;
  if (!first_frame.render_ready) {
    surface.state = "first_frame_failed";
    surface.errors.push_back(first_frame.exit_reason);
  } else {
    surface.first_pixel_marker = HexUint32(first_frame.surface.first_pixel_value);
    surface.marker_checksum = surface.first_pixel_marker;
    surface.lifecycle_states.push_back("first_frame_presented");
    surface.cleanup_ready = true;
    surface.lifecycle_states.push_back("cleanup_ready");
    surface.state = "cleanup_ready";
  }

  std::ostringstream event_log;
  for (const auto& state : surface.lifecycle_states) {
    event_log << "{\"state\":\"" << EscapeJson(state) << "\"}\n";
  }
  WriteTextFile(surface.event_log_path, event_log.str());
  WriteSurfaceSessionMetadata(surface);
  return surface;
}

}  // namespace

NativeApkLaunchReport LaunchNativeApk(const std::string& apk_path,
                                      const NativeApkLaunchOptions& options) {
  NativeApkLaunchReport report;
  report.apk_path = apk_path;
  report.requested_package_name = options.requested_package_name;
  report.requested_component = options.requested_component;
  report.self_heal_proof_requested = options.self_heal_proof_requested;
  report.window_proof_requested =
      options.window_proof_requested || options.self_heal_proof_requested;
  report.process_proof_requested =
      options.process_proof_requested || report.window_proof_requested ||
      options.self_heal_proof_requested;
  report.activity_proof_requested =
      options.activity_proof_requested || report.process_proof_requested ||
      options.self_heal_proof_requested;
  report.storage_proof_requested =
      options.storage_proof_requested || report.process_proof_requested ||
      options.self_heal_proof_requested;
  report.permissions_proof_requested =
      options.permissions_proof_requested || report.process_proof_requested ||
      options.self_heal_proof_requested;
  report.surface_proof_requested =
      options.surface_proof_requested || report.activity_proof_requested ||
      report.window_proof_requested || options.self_heal_proof_requested;
  report.asset_proof_requested =
      options.asset_proof_requested || options.self_heal_proof_requested;
  report.lifecycle_proof_requested =
      options.lifecycle_proof_requested || report.activity_proof_requested ||
      options.self_heal_proof_requested;
  report.dex_proof_requested =
      options.dex_proof_requested || report.activity_proof_requested ||
      options.self_heal_proof_requested;
  report.surface.state = "not_requested";
  report.limitations = {
      "plain_xml_manifest_parser_only",
      "stored_zip_entries_only",
      "native_only_no_art_execution_yet",
  };
  if (report.dex_proof_requested) {
    AppendError(&report.limitations, "dex_header_and_counts_only");
    AppendError(&report.limitations, "full_art_execution_not_supported_yet");
  }
  if (report.activity_proof_requested) {
    AppendError(&report.limitations, "local_package_manager_contract_only");
    AppendError(&report.limitations, "local_intent_resolution_only");
    AppendError(&report.limitations, "local_activity_launch_contract_only");
  }
  if (report.storage_proof_requested) {
    AppendError(&report.limitations, "android_app_storage_contract_only");
    AppendError(&report.limitations, "path_sandbox_only");
  }
  if (report.permissions_proof_requested) {
    AppendError(&report.limitations, "permissions_and_appops_contract_only");
    AppendError(&report.limitations,
                "binary_manifest_permission_decode_not_supported_yet");
  }
  if (report.process_proof_requested) {
    AppendError(&report.limitations,
                "local_activity_manager_contract_only");
    AppendError(&report.limitations,
                "local_process_manager_contract_only");
  }
  if (report.window_proof_requested) {
    AppendError(&report.limitations,
                "local_window_manager_contract_only");
    AppendError(&report.limitations,
                "wayland_egl_best_effort_probe_only");
  }
  if (report.self_heal_proof_requested) {
    AppendError(&report.limitations,
                "self_healing_android_device_local_watchdog_only");
  }
  report.launch_status = "launch_not_started";

  if (apk_path.empty()) {
    report.errors.push_back("apk_path_missing");
    report.launch_status = "invalid_request";
    PopulateRequestedProofFailures(&report);
    report.launch_health = "blocked";
    report.recoverable = DetermineRecoverable(report);
    report.recommended_recovery_action =
        DetermineRecommendedRecoveryAction(report);
    return FinalizeNativeApkLaunchReport(std::move(report), options);
  }

  OpenedApkArchive archive;
  try {
    archive = OpenApkArchive(apk_path);
  } catch (const std::exception& error) {
    report.errors.push_back("invalid_apk:" + std::string(error.what()));
    report.launch_status = "invalid_apk";
    PopulateRequestedProofFailures(&report);
    report.launch_health = "blocked";
    report.recoverable = DetermineRecoverable(report);
    report.recommended_recovery_action =
        DetermineRecommendedRecoveryAction(report);
    return FinalizeNativeApkLaunchReport(std::move(report), options);
  }

  for (const auto& entry : ListApkArchiveEntries(archive)) {
    if (!IsSafeArchivePath(entry.path)) {
      report.errors.push_back("unsafe_archive_entry:" + entry.path);
      report.launch_status = "unsafe_archive_entry";
      PopulateRequestedProofFailures(&report);
      report.launch_health = "blocked";
      report.recoverable = DetermineRecoverable(report);
      report.recommended_recovery_action =
          DetermineRecommendedRecoveryAction(report);
      return FinalizeNativeApkLaunchReport(std::move(report), options);
    }
  }

  const auto manifest = ParseManifestMetadata(archive, &report.limitations);
  report.manifest_present = manifest.manifest_present;
  report.manifest_metadata_ready = manifest.metadata_ready;
  report.manifest_source = manifest.manifest_source;
  report.package_name = manifest.package_name;
  report.version_name = manifest.version_name;
  report.version_code = manifest.version_code;
  report.launcher_component = manifest.launcher_component;
  report.declared_activities = manifest.activity_names;
  report.errors.insert(report.errors.end(), manifest.errors.begin(),
                       manifest.errors.end());
  if (!manifest.metadata_ready) {
    report.launch_status = "manifest_metadata_unavailable";
    PopulateRequestedProofFailures(&report);
    report.launch_health = "blocked";
    report.recoverable = DetermineRecoverable(report);
    report.recommended_recovery_action =
        DetermineRecommendedRecoveryAction(report);
    return FinalizeNativeApkLaunchReport(std::move(report), options);
  }

  ApktoolMetadata apk_metadata{
      .apk_file_name = fs::path(apk_path).filename().string(),
      .min_sdk = manifest.min_sdk,
      .target_sdk = manifest.target_sdk,
      .version_code = manifest.version_code,
      .version_name = manifest.version_name,
  };
  report.install_id = BuildInstallId(apk_metadata);

  const auto layout = BuildPackageLayout(
      {.package_name = report.package_name,
       .install_id = report.install_id,
       .version_code = report.version_code},
      options.staging_root);
  report.package_root = layout.host_package_root;
  report.staged_dir = (fs::path(layout.host_package_root) / "launch-apk" / "default").string();
  report.bundle_apk_path = (fs::path(report.staged_dir) / "base.apk").string();
  report.manifest_path = (fs::path(report.staged_dir) / "AndroidManifest.xml").string();
  report.metadata_json_path =
      (fs::path(report.staged_dir) / "manifest-metadata.json").string();
  report.bootstrap_manifest_path =
      (fs::path(report.staged_dir) / "launch-bootstrap.json").string();
  report.report_json_path =
      (fs::path(report.staged_dir) / "launch-apk-report.json").string();
  report.native_execute_log_path =
      (fs::path(report.staged_dir) / "native-execute.log").string();
  report.library_root = (fs::path(report.staged_dir) / "lib").string();
  report.resource_root = (fs::path(report.staged_dir) / "resources").string();
  report.asset_root = (fs::path(report.resource_root) / "assets").string();
  report.sandbox_root = (fs::path(report.staged_dir) / "sandbox").string();
  report.dex_cache_root = (fs::path(report.staged_dir) / "dex-cache").string();

  std::error_code ignored;
  fs::create_directories(report.staged_dir, ignored);
  for (const auto& entry : fs::directory_iterator(report.staged_dir, ignored)) {
    if (entry.path().filename() == "sandbox") {
      continue;
    }
    fs::remove_all(entry.path(), ignored);
  }
  fs::create_directories(report.library_root);
  fs::create_directories(report.asset_root);
  fs::create_directories(fs::path(report.resource_root) / "res");
  fs::create_directories(report.sandbox_root);
  fs::create_directories(report.dex_cache_root);

  fs::copy_file(apk_path, report.bundle_apk_path, fs::copy_options::overwrite_existing);
  WriteTextFile(report.manifest_path, manifest.manifest_contents);
  WriteTextFile(report.metadata_json_path, RenderManifestMetadataJson(manifest));

  const std::string host_abi = ResolveHostAbi();
  const auto libraries = CollectLibraryEntries(archive);
  for (const auto& library : libraries) {
    if (library.abi == host_abi) {
      report.selected_abi = host_abi;
      report.host_abi_supported = true;
      report.native_libraries_present = true;
      const fs::path staged_library = fs::path(report.library_root) / library.file_name;
      if (StageArchiveEntry(archive, library.archive_path, staged_library,
                            &report.errors)) {
        report.native_libraries.push_back(staged_library.string());
      }
    } else {
      report.native_libraries_present = true;
      report.unsupported_native_libraries.push_back(library.archive_path);
    }
  }

  for (const auto& entry : ListApkArchiveEntries(archive)) {
    if (entry.is_directory) {
      continue;
    }
    if (entry.path.rfind("assets/", 0) == 0) {
      const std::string relative = entry.path.substr(std::string("assets/").size());
      if (StageArchiveEntry(archive, entry.path, fs::path(report.asset_root) / relative,
                            &report.errors)) {
        report.asset_entries.push_back(relative);
      }
      continue;
    }
    if (entry.path.rfind("res/", 0) == 0) {
      const std::string relative = entry.path.substr(std::string("res/").size());
      StageArchiveEntry(archive, entry.path,
                        fs::path(report.resource_root) / "res" / relative,
                        &report.errors);
      continue;
    }
    if (entry.path == "resources.arsc") {
      StageArchiveEntry(archive, entry.path,
                        fs::path(report.resource_root) / "resources.arsc",
                        &report.errors);
    }
  }
  report.assets_count = static_cast<int>(report.asset_entries.size());
  std::sort(report.asset_entries.begin(), report.asset_entries.end());

  if (report.storage_proof_requested) {
    report.storage = BuildStorageBridgeSession(report).RunStorageProof();
    report.storage_health = report.storage.ready ? "ready" : "blocked";
    report.sandbox_health =
        (report.storage.ready &&
         report.storage.isolation_level == "path_sandbox_only")
            ? "ready"
            : "blocked";
    for (const auto& error : report.storage.errors) {
      AppendError(&report.errors, error);
    }
  } else {
    report.storage_health = "not_requested";
    report.sandbox_health = "not_requested";
  }

  if (!report.native_libraries_present) {
    report.errors.push_back("no_native_libraries_found");
    report.launch_status = "no_native_libraries_found";
  } else if (!report.host_abi_supported) {
    report.errors.push_back("unsupported_host_abi:" + host_abi);
    report.launch_status = "unsupported_host_abi";
  } else if (report.native_libraries.empty()) {
    report.errors.push_back("native_library_staging_failed");
    report.launch_status = "native_library_staging_failed";
  }

  if (report.asset_proof_requested) {
    const auto asset_session = BuildAssetBridgeSession(report);
    report.asset_bridge =
        ProveNativeApkAssetBridge(asset_session, options.preferred_asset_path);
    report.resource_bridge = asset_session.InspectResources();
    if (!report.asset_bridge.ready) {
      AppendError(&report.errors, "asset_bridge_unavailable");
    }
    if (!report.resource_bridge.ready) {
      AppendError(&report.errors, "resource_bridge_unavailable");
    }
    report.asset_health = report.asset_bridge.ready ? "ready" : "blocked";
    report.resource_health =
        report.resource_bridge.ready ? "ready" : "blocked";
  } else {
    report.asset_health = "not_requested";
    report.resource_health = "not_requested";
  }

  if (report.errors.empty()) {
    report.native_execute.package_name = report.package_name;
    report.native_execute.launcher_component = manifest.launcher_component;
    report.native_execute.bundle_apk_path = report.bundle_apk_path;
    report.native_execute.sandbox_root = report.sandbox_root;
    report.native_execute.dex_cache_root = report.dex_cache_root;
    report.native_execute.resource_root = report.resource_root;
    report.native_execute.library_root = report.library_root;
    report.native_execute.bootstrap_manifest_path = report.bootstrap_manifest_path;

    WriteLaunchBootstrapManifest(report);
    report.native_execute = ExecuteNativeStubWithCapturedStdio(
        {.package_name = report.package_name,
         .launcher_component = manifest.launcher_component,
         .bundle_apk_path = report.bundle_apk_path,
         .sandbox_root = report.sandbox_root,
         .dex_cache_root = report.dex_cache_root,
         .resource_root = report.resource_root,
         .library_root = report.library_root,
         .bootstrap_manifest_path = report.bootstrap_manifest_path,
         .watchdog_seconds = options.watchdog_seconds},
        fs::path(report.native_execute_log_path), &report.errors,
        &report.diagnostics);

    for (const auto& result : report.native_execute.jni_onload_results) {
      if (result.call_succeeded) {
        report.jni_onload_called = true;
        report.jni_onload_result = result.return_code;
        break;
      }
    }

    report.launch_ready =
        report.native_execute.dlopen_ok &&
        report.native_execute.entrypoint_found &&
        report.native_execute.activity_called &&
        report.native_execute.exit_code == 0;
    report.launch_status = report.launch_ready
                               ? "native_apk_launch_succeeded"
                               : report.native_execute.exit_reason;
    if (!report.launch_ready) {
      AppendError(&report.errors,
                  "native_launch_failed:" + report.native_execute.exit_reason);
    }
  }

  if (report.surface_proof_requested) {
    report.surface = RunNativeApkSurfaceProof(report, options);
    report.surface_proof_ready = report.surface.first_frame_presented;
    report.surface_health = report.surface_proof_ready ? "ready" : "blocked";
  } else {
    report.surface_health = "not_requested";
  }

  if (report.lifecycle_proof_requested) {
    if (report.launch_ready) {
      const auto lifecycle_report =
          BuildLifecycleBridgeSession(report, options).RunDeterministicProof();
      report.lifecycle = lifecycle_report.lifecycle;
      report.looper = lifecycle_report.looper;
      report.input_queue = lifecycle_report.input_queue;
      report.lifecycle_health = report.lifecycle.ready ? "ready" : "blocked";
      report.looper_health = report.looper.ready ? "ready" : "blocked";
      report.input_health = report.input_queue.ready ? "ready" : "blocked";
      if (!lifecycle_report.ready) {
        for (const auto& error : lifecycle_report.errors) {
          AppendError(&report.errors, error);
        }
      }
    } else {
      report.lifecycle.errors = report.errors;
      report.looper.errors = report.errors;
      report.input_queue.errors = report.errors;
      report.lifecycle_health = "blocked";
      report.looper_health = "blocked";
      report.input_health = "blocked";
    }
  } else {
    report.lifecycle_health = "not_requested";
    report.looper_health = "not_requested";
    report.input_health = "not_requested";
  }

  if (report.dex_proof_requested) {
    report.dex = BuildDexBridgeSession(report).RunDexProof(archive);
    report.art_bootstrap = BuildDexBridgeSession(report).BuildArtBootstrap(report.dex);
    report.dex_health = report.dex.ready ? "ready" : "blocked";
    report.art_health = report.art_bootstrap.ready ? "ready" : "blocked";
    for (const auto& error : report.dex.errors) {
      AppendError(&report.errors, error);
    }
    for (const auto& error : report.art_bootstrap.errors) {
      AppendError(&report.errors, error);
    }
  } else {
    report.dex_health = "not_requested";
    report.art_health = "not_requested";
  }

  if (report.activity_proof_requested) {
    const auto binder_report = MaterializeBinderFoundation(report);
    report.binder_health = binder_report.manager_ready ? "ready" : "blocked";
    if (!binder_report.manager_ready) {
      AppendError(&report.errors, "binder_service_manager_unavailable");
    }

    const auto activity_session = BuildActivityLaunchBridgeSession(report, manifest);
    report.package_manager = activity_session.BuildPackageManagerRecord();
    report.intent_resolution =
        activity_session.ResolveActivityIntent(report.package_manager);
    report.activity_launch = activity_session.BuildActivityLaunchRecord(
        report.package_manager, report.intent_resolution);
    report.activity_health = report.activity_launch.ready ? "ready" : "blocked";
    for (const auto& error : report.package_manager.errors) {
      AppendError(&report.errors, error);
    }
    for (const auto& error : report.intent_resolution.errors) {
      AppendError(&report.errors, error);
    }
    for (const auto& error : report.activity_launch.errors) {
      AppendError(&report.errors, error);
    }
  } else {
    report.binder_health = "not_requested";
    report.activity_health = "not_requested";
  }

  if (report.permissions_proof_requested) {
    const auto permission_session =
        BuildPermissionBridgeSession(report, manifest);
    report.permissions = permission_session.BuildPermissionsReport();
    report.app_ops = permission_session.BuildAppOpsReport(report.permissions);
    report.permission_health = report.permissions.ready ? "ready" : "blocked";
    report.app_ops_health = report.app_ops.ready ? "ready" : "blocked";
    for (const auto& error : report.permissions.errors) {
      AppendError(&report.errors, error);
    }
    for (const auto& error : report.app_ops.errors) {
      AppendError(&report.errors, error);
    }
  } else {
    report.permission_health = "not_requested";
    report.app_ops_health = "not_requested";
  }

  ApplySimulatedSubsystemFaults(&report, options);

  if (report.process_proof_requested) {
    const auto process_session = BuildProcessManagerBridgeSession(
        report, !report.self_heal_proof_requested);
    report.activity_manager = process_session.BuildActivityManagerReport();
    report.process_manager =
        process_session.BuildProcessManagerReport(report.activity_manager);
    report.activity_manager_health =
        report.activity_manager.ready ? "ready" : "blocked";
    report.process_health =
        report.process_manager.ready ? "ready" : "blocked";
    for (const auto& error : report.activity_manager.errors) {
      AppendError(&report.errors, error);
    }
    for (const auto& error : report.process_manager.errors) {
      AppendError(&report.errors, error);
    }
  } else {
    report.activity_manager_health = "not_requested";
    report.process_health = "not_requested";
  }

  if (report.window_proof_requested) {
    report.window_manager = BuildWindowManagerBridgeSession(
        report, !report.self_heal_proof_requested)
                                .BuildReport();
    report.window_health = report.window_manager.ready ? "ready" : "blocked";
    for (const auto& error : report.window_manager.errors) {
      AppendError(&report.errors, error);
    }
  } else {
    report.window_health = "not_requested";
  }

  const bool asset_contract_ready =
      !report.asset_proof_requested ||
      (report.asset_bridge.ready && report.resource_bridge.ready);
  const bool storage_contract_ready =
      !report.storage_proof_requested || report.storage.ready;
  const bool permissions_contract_ready =
      !report.permissions_proof_requested ||
      (report.permissions.ready && report.app_ops.ready);
  const bool lifecycle_contract_ready =
      !report.lifecycle_proof_requested ||
      (report.lifecycle.ready && report.looper.ready && report.input_queue.ready);
  const bool dex_contract_ready =
      !report.dex_proof_requested ||
      (report.dex.ready && report.art_bootstrap.ready);
  const bool activity_contract_ready =
      !report.activity_proof_requested ||
      (report.package_manager.ready && report.intent_resolution.ready &&
       report.activity_launch.ready);
  const bool process_contract_ready =
      !report.process_proof_requested ||
      (report.activity_manager.ready && report.process_manager.ready);
  const bool window_contract_ready =
      !report.window_proof_requested || report.window_manager.ready;
  report.launch_health =
      report.launch_ready &&
              (!report.surface_proof_requested || report.surface_proof_ready) &&
              window_contract_ready &&
              asset_contract_ready &&
              storage_contract_ready &&
              permissions_contract_ready &&
              lifecycle_contract_ready &&
              dex_contract_ready &&
              activity_contract_ready &&
              process_contract_ready
          ? "ready"
          : "blocked";
  report.recoverable = DetermineRecoverable(report);
  report.recommended_recovery_action =
      DetermineRecommendedRecoveryAction(report);

  return FinalizeNativeApkLaunchReport(std::move(report), options);
}

std::string RenderNativeApkLaunchJson(const NativeApkLaunchReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"requested_package_name\": \""
         << EscapeJson(report.requested_package_name) << "\",\n"
         << "  \"requested_component\": \""
         << EscapeJson(report.requested_component) << "\",\n"
         << "  \"version_name\": \"" << EscapeJson(report.version_name)
         << "\",\n"
         << "  \"version_code\": " << report.version_code << ",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id)
         << "\",\n"
         << "  \"manifest_present\": "
         << (report.manifest_present ? "true" : "false") << ",\n"
         << "  \"manifest_metadata_ready\": "
         << (report.manifest_metadata_ready ? "true" : "false") << ",\n"
         << "  \"manifest_source\": \"" << EscapeJson(report.manifest_source)
         << "\",\n"
         << "  \"surface_proof_requested\": "
         << (report.surface_proof_requested ? "true" : "false") << ",\n"
         << "  \"surface_proof_ready\": "
         << (report.surface_proof_ready ? "true" : "false") << ",\n"
         << "  \"asset_proof_requested\": "
         << (report.asset_proof_requested ? "true" : "false") << ",\n"
         << "  \"lifecycle_proof_requested\": "
         << (report.lifecycle_proof_requested ? "true" : "false") << ",\n"
         << "  \"dex_proof_requested\": "
         << (report.dex_proof_requested ? "true" : "false") << ",\n"
         << "  \"activity_proof_requested\": "
         << (report.activity_proof_requested ? "true" : "false") << ",\n"
         << "  \"process_proof_requested\": "
         << (report.process_proof_requested ? "true" : "false") << ",\n"
         << "  \"window_proof_requested\": "
         << (report.window_proof_requested ? "true" : "false") << ",\n"
         << "  \"storage_proof_requested\": "
         << (report.storage_proof_requested ? "true" : "false") << ",\n"
         << "  \"permissions_proof_requested\": "
         << (report.permissions_proof_requested ? "true" : "false") << ",\n"
         << "  \"self_heal_proof_requested\": "
         << (report.self_heal_proof_requested ? "true" : "false") << ",\n"
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir)
         << "\",\n"
         << "  \"bundle_apk_path\": \"" << EscapeJson(report.bundle_apk_path)
         << "\",\n"
         << "  \"manifest_path\": \"" << EscapeJson(report.manifest_path)
         << "\",\n"
         << "  \"metadata_json_path\": \"" << EscapeJson(report.metadata_json_path)
         << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"native_execute_log_path\": \""
         << EscapeJson(report.native_execute_log_path) << "\",\n"
         << "  \"report_json_path\": \"" << EscapeJson(report.report_json_path)
         << "\",\n"
         << "  \"launcher_component\": \""
         << EscapeJson(report.launcher_component) << "\",\n"
         << "  \"native_libraries\": " << RenderJsonArray(report.native_libraries)
         << ",\n"
         << "  \"unsupported_native_libraries\": "
         << RenderJsonArray(report.unsupported_native_libraries) << ",\n"
         << "  \"assets_count\": " << report.assets_count << ",\n"
         << "  \"asset_entries\": " << RenderJsonArray(report.asset_entries)
         << ",\n"
         << "  \"declared_activities\": "
         << RenderJsonArray(report.declared_activities) << ",\n"
         << "  \"diagnostics\": " << RenderJsonArray(report.diagnostics)
         << ",\n"
         << "  \"selected_abi\": \"" << EscapeJson(report.selected_abi)
         << "\",\n"
         << "  \"jni_onload_called\": "
         << (report.jni_onload_called ? "true" : "false") << ",\n"
         << "  \"jni_onload_result\": " << report.jni_onload_result << ",\n"
         << "  \"launch_status\": \"" << EscapeJson(report.launch_status)
         << "\",\n"
         << "  \"launch_ready\": "
         << (report.launch_ready ? "true" : "false") << ",\n"
         << "  \"surface_health\": \"" << EscapeJson(report.surface_health)
         << "\",\n"
         << "  \"window_health\": \"" << EscapeJson(report.window_health)
         << "\",\n"
         << "  \"asset_health\": \"" << EscapeJson(report.asset_health)
         << "\",\n"
         << "  \"resource_health\": \"" << EscapeJson(report.resource_health)
         << "\",\n"
         << "  \"lifecycle_health\": \""
         << EscapeJson(report.lifecycle_health) << "\",\n"
         << "  \"looper_health\": \"" << EscapeJson(report.looper_health)
         << "\",\n"
         << "  \"input_health\": \"" << EscapeJson(report.input_health)
         << "\",\n"
         << "  \"dex_health\": \"" << EscapeJson(report.dex_health)
         << "\",\n"
         << "  \"art_health\": \"" << EscapeJson(report.art_health)
         << "\",\n"
         << "  \"binder_health\": \"" << EscapeJson(report.binder_health)
         << "\",\n"
         << "  \"activity_health\": \"" << EscapeJson(report.activity_health)
         << "\",\n"
         << "  \"activity_manager_health\": \""
         << EscapeJson(report.activity_manager_health) << "\",\n"
         << "  \"process_health\": \"" << EscapeJson(report.process_health)
         << "\",\n"
         << "  \"storage_health\": \"" << EscapeJson(report.storage_health)
         << "\",\n"
         << "  \"sandbox_health\": \"" << EscapeJson(report.sandbox_health)
         << "\",\n"
         << "  \"permission_health\": \""
         << EscapeJson(report.permission_health) << "\",\n"
         << "  \"app_ops_health\": \"" << EscapeJson(report.app_ops_health)
         << "\",\n"
         << "  \"launch_health\": \"" << EscapeJson(report.launch_health)
         << "\",\n"
         << "  \"recoverable\": "
         << (report.recoverable ? "true" : "false") << ",\n"
         << "  \"recommended_recovery_action\": \""
         << EscapeJson(report.recommended_recovery_action) << "\",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << ",\n"
         << "  \"limitations\": " << RenderJsonArray(report.limitations) << ",\n"
         << "  \"surface\": {\n"
         << "    \"session_id\": \"" << EscapeJson(report.surface.session_id)
         << "\",\n"
         << "    \"package_name\": \""
         << EscapeJson(report.surface.package_name) << "\",\n"
         << "    \"apk_path\": \"" << EscapeJson(report.surface.apk_path)
         << "\",\n"
         << "    \"staged_dir\": \"" << EscapeJson(report.surface.staged_dir)
         << "\",\n"
         << "    \"selected_library_path\": \""
         << EscapeJson(report.surface.selected_library_path) << "\",\n"
         << "    \"session_root\": \""
         << EscapeJson(report.surface.session_root) << "\",\n"
         << "    \"metadata_path\": \""
         << EscapeJson(report.surface.metadata_path) << "\",\n"
         << "    \"event_log_path\": \""
         << EscapeJson(report.surface.event_log_path) << "\",\n"
         << "    \"marker_path\": \"" << EscapeJson(report.surface.marker_path)
         << "\",\n"
         << "    \"backend\": \"" << EscapeJson(report.surface.backend)
         << "\",\n"
         << "    \"backing_mode\": \""
         << EscapeJson(report.surface.backing_mode) << "\",\n"
         << "    \"bridge_metadata_path\": \""
         << EscapeJson(report.surface.bridge_metadata_path) << "\",\n"
         << "    \"bridge_event_log_path\": \""
         << EscapeJson(report.surface.bridge_event_log_path) << "\",\n"
         << "    \"state\": \"" << EscapeJson(report.surface.state)
         << "\",\n"
         << "    \"width\": " << report.surface.width << ",\n"
         << "    \"height\": " << report.surface.height << ",\n"
         << "    \"format\": " << report.surface.format << ",\n"
         << "    \"surface_created\": "
         << (report.surface.surface_created ? "true" : "false") << ",\n"
         << "    \"surface_configured\": "
         << (report.surface.surface_configured ? "true" : "false") << ",\n"
         << "    \"first_frame_requested\": "
         << (report.surface.first_frame_requested ? "true" : "false")
         << ",\n"
         << "    \"first_frame_presented\": "
         << (report.surface.first_frame_presented ? "true" : "false")
         << ",\n"
         << "    \"cleanup_ready\": "
         << (report.surface.cleanup_ready ? "true" : "false") << ",\n"
         << "    \"wayland_surface_available\": "
         << (report.surface.wayland_surface_available ? "true" : "false")
         << ",\n"
         << "    \"egl_surface_available\": "
         << (report.surface.egl_surface_available ? "true" : "false")
         << ",\n"
         << "    \"first_pixel_marker\": \""
         << EscapeJson(report.surface.first_pixel_marker) << "\",\n"
         << "    \"marker_checksum\": \""
         << EscapeJson(report.surface.marker_checksum) << "\",\n"
         << "    \"lifecycle_states\": "
         << RenderJsonArray(report.surface.lifecycle_states) << ",\n"
         << "    \"errors\": " << RenderJsonArray(report.surface.errors)
         << "\n"
         << "  },\n"
         << "  \"asset_bridge\": {\n"
         << "    \"ready\": "
         << (report.asset_bridge.ready ? "true" : "false") << ",\n"
         << "    \"assets_count\": " << report.asset_bridge.assets_count
         << ",\n"
         << "    \"asset_paths\": "
         << RenderJsonArray(report.asset_bridge.asset_paths) << ",\n"
         << "    \"opened_asset\": \""
         << EscapeJson(report.asset_bridge.opened_asset) << "\",\n"
         << "    \"opened_asset_size\": "
         << report.asset_bridge.opened_asset_size << ",\n"
         << "    \"opened_asset_checksum\": \""
         << EscapeJson(report.asset_bridge.opened_asset_checksum) << "\",\n"
         << "    \"rejected_unsafe_path\": "
         << (report.asset_bridge.rejected_unsafe_path ? "true" : "false")
         << ",\n"
         << "    \"errors\": " << RenderJsonArray(report.asset_bridge.errors)
         << "\n"
         << "  },\n"
         << "  \"resource_bridge\": {\n"
         << "    \"ready\": "
         << (report.resource_bridge.ready ? "true" : "false") << ",\n"
         << "    \"resource_table_present\": "
         << (report.resource_bridge.resource_table_present ? "true" : "false")
         << ",\n"
         << "    \"res_entries_count\": "
         << report.resource_bridge.res_entries_count << ",\n"
         << "    \"decode_level\": \""
         << EscapeJson(report.resource_bridge.decode_level) << "\",\n"
         << "    \"res_entries\": "
         << RenderJsonArray(report.resource_bridge.res_entries) << ",\n"
         << "    \"errors\": "
         << RenderJsonArray(report.resource_bridge.errors) << "\n"
         << "  },\n"
         << "  \"lifecycle\": {\n"
         << "    \"ready\": " << (report.lifecycle.ready ? "true" : "false")
         << ",\n"
         << "    \"session_id\": \"" << EscapeJson(report.lifecycle.session_id)
         << "\",\n"
         << "    \"metadata_path\": \""
         << EscapeJson(report.lifecycle.metadata_path) << "\",\n"
         << "    \"event_log_path\": \""
         << EscapeJson(report.lifecycle.event_log_path) << "\",\n"
         << "    \"states_visited\": "
         << RenderJsonArray(report.lifecycle.states_visited) << ",\n"
         << "    \"current_state\": \""
         << EscapeJson(report.lifecycle.current_state) << "\",\n"
         << "    \"events_dispatched\": "
         << report.lifecycle.events_dispatched << ",\n"
         << "    \"errors\": " << RenderJsonArray(report.lifecycle.errors)
         << "\n"
         << "  },\n"
         << "  \"looper\": {\n"
         << "    \"ready\": " << (report.looper.ready ? "true" : "false")
         << ",\n"
         << "    \"metadata_path\": \""
         << EscapeJson(report.looper.metadata_path) << "\",\n"
         << "    \"event_log_path\": \""
         << EscapeJson(report.looper.event_log_path) << "\",\n"
         << "    \"posted_events\": " << report.looper.posted_events << ",\n"
         << "    \"dispatched_events\": " << report.looper.dispatched_events
         << ",\n"
         << "    \"shutdown_clean\": "
         << (report.looper.shutdown_clean ? "true" : "false") << ",\n"
         << "    \"errors\": " << RenderJsonArray(report.looper.errors)
         << "\n"
         << "  },\n"
         << "  \"input_queue\": {\n"
         << "    \"ready\": " << (report.input_queue.ready ? "true" : "false")
         << ",\n"
         << "    \"metadata_path\": \""
         << EscapeJson(report.input_queue.metadata_path) << "\",\n"
         << "    \"event_log_path\": \""
         << EscapeJson(report.input_queue.event_log_path) << "\",\n"
         << "    \"queued_events\": " << report.input_queue.queued_events
         << ",\n"
         << "    \"dispatched_events\": "
         << report.input_queue.dispatched_events << ",\n"
         << "    \"handled_events\": " << report.input_queue.handled_events
         << ",\n"
         << "    \"rejected_events\": " << report.input_queue.rejected_events
         << ",\n"
         << "    \"errors\": "
         << RenderJsonArray(report.input_queue.errors) << "\n"
         << "  },\n"
         << "  \"dex\": {\n"
         << "    \"ready\": " << (report.dex.ready ? "true" : "false")
         << ",\n"
         << "    \"session_id\": \"" << EscapeJson(report.dex.session_id)
         << "\",\n"
         << "    \"artifact_root\": \""
         << EscapeJson(report.dex.artifact_root) << "\",\n"
         << "    \"inventory_json_path\": \""
         << EscapeJson(report.dex.inventory_json_path) << "\",\n"
         << "    \"files_count\": " << report.dex.files_count << ",\n"
         << "    \"total_bytes\": " << report.dex.total_bytes << ",\n"
         << "    \"decode_level\": \"" << EscapeJson(report.dex.decode_level)
         << "\",\n"
         << "    \"class_defs_count\": " << report.dex.class_defs_count
         << ",\n"
         << "    \"files\": [\n";
  for (std::size_t index = 0; index < report.dex.files.size(); ++index) {
    const auto& file = report.dex.files[index];
    output << "      {\n"
           << "        \"entry_name\": \"" << EscapeJson(file.entry_name)
           << "\",\n"
           << "        \"staged_path\": \"" << EscapeJson(file.staged_path)
           << "\",\n"
           << "        \"size_bytes\": " << file.size_bytes << ",\n"
           << "        \"checksum\": \"" << EscapeJson(file.checksum)
           << "\",\n"
           << "        \"valid_dex_magic\": "
           << (file.valid_dex_magic ? "true" : "false") << ",\n"
           << "        \"dex_version\": \"" << EscapeJson(file.dex_version)
           << "\",\n"
           << "        \"dex_file_size\": " << file.dex_file_size << ",\n"
           << "        \"header_size\": " << file.header_size << ",\n"
           << "        \"string_ids_size\": " << file.string_ids_size
           << ",\n"
           << "        \"type_ids_size\": " << file.type_ids_size << ",\n"
           << "        \"class_defs_count\": " << file.class_defs_count
           << ",\n"
           << "        \"errors\": " << RenderJsonArray(file.errors) << "\n"
           << "      }";
    if (index + 1 != report.dex.files.size()) {
      output << ",";
    }
    output << "\n";
  }
  output << "    ],\n"
         << "    \"errors\": " << RenderJsonArray(report.dex.errors) << "\n"
         << "  },\n"
         << "  \"art_bootstrap\": {\n"
         << "    \"ready\": "
         << (report.art_bootstrap.ready ? "true" : "false") << ",\n"
         << "    \"session_id\": \""
         << EscapeJson(report.art_bootstrap.session_id) << "\",\n"
         << "    \"artifact_root\": \""
         << EscapeJson(report.art_bootstrap.artifact_root) << "\",\n"
         << "    \"bootstrap_json_path\": \""
         << EscapeJson(report.art_bootstrap.bootstrap_json_path)
         << "\",\n"
         << "    \"asset_bridge_status\": \""
         << EscapeJson(report.art_bootstrap.asset_bridge_status)
         << "\",\n"
         << "    \"lifecycle_status\": \""
         << EscapeJson(report.art_bootstrap.lifecycle_status) << "\",\n"
         << "    \"binder_service_registry_status\": \""
         << EscapeJson(report.art_bootstrap.binder_service_registry_status)
         << "\",\n"
         << "    \"art_runtime_required\": "
         << (report.art_bootstrap.art_runtime_required ? "true" : "false")
         << ",\n"
         << "    \"art_runtime_available\": "
         << (report.art_bootstrap.art_runtime_available ? "true" : "false")
         << ",\n"
         << "    \"dex_bootstrap_ready\": "
         << (report.art_bootstrap.dex_bootstrap_ready ? "true" : "false")
         << ",\n"
         << "    \"class_loader_ready\": "
         << (report.art_bootstrap.class_loader_ready ? "true" : "false")
         << ",\n"
         << "    \"java_execution_supported\": "
         << (report.art_bootstrap.java_execution_supported ? "true"
                                                           : "false")
         << ",\n"
         << "    \"limitation\": \""
         << EscapeJson(report.art_bootstrap.limitation) << "\",\n"
         << "    \"dex_files\": "
         << RenderJsonArray(report.art_bootstrap.dex_files) << ",\n"
         << "    \"errors\": "
         << RenderJsonArray(report.art_bootstrap.errors) << "\n"
         << "  },\n"
         << "  \"package_manager\": {\n"
         << "    \"ready\": "
         << (report.package_manager.ready ? "true" : "false") << ",\n"
         << "    \"session_id\": \""
         << EscapeJson(report.package_manager.session_id) << "\",\n"
         << "    \"artifact_root\": \""
         << EscapeJson(report.package_manager.artifact_root) << "\",\n"
         << "    \"package_record_path\": \""
         << EscapeJson(report.package_manager.package_record_path)
         << "\",\n"
         << "    \"package_name\": \""
         << EscapeJson(report.package_manager.package_name) << "\",\n"
         << "    \"requested_package_name\": \""
         << EscapeJson(report.package_manager.requested_package_name)
         << "\",\n"
         << "    \"apk_path\": \"" << EscapeJson(report.package_manager.apk_path)
         << "\",\n"
         << "    \"staged_dir\": \""
         << EscapeJson(report.package_manager.staged_dir) << "\",\n"
         << "    \"install_id\": \""
         << EscapeJson(report.package_manager.install_id) << "\",\n"
         << "    \"version_name\": \""
         << EscapeJson(report.package_manager.version_name) << "\",\n"
         << "    \"version_code\": " << report.package_manager.version_code
         << ",\n"
         << "    \"manifest_source\": \""
         << EscapeJson(report.package_manager.manifest_source) << "\",\n"
         << "    \"package_label\": \""
         << EscapeJson(report.package_manager.package_label) << "\",\n"
         << "    \"launcher_component\": \""
         << EscapeJson(report.package_manager.launcher_component) << "\",\n"
         << "    \"declared_components\": "
         << RenderJsonArray(report.package_manager.declared_components)
         << ",\n"
         << "    \"declared_activities\": "
         << RenderJsonArray(report.package_manager.declared_activities)
         << ",\n"
         << "    \"activities\": "
         << RenderActivityComponentArray(report.package_manager.activities)
         << ",\n"
         << "    \"staged_native_libraries\": "
         << RenderJsonArray(report.package_manager.staged_native_libraries)
         << ",\n"
         << "    \"binder_service_registry_status\": \""
         << EscapeJson(report.package_manager.binder_service_registry_status)
         << "\",\n"
         << "    \"binder_service_manager_path\": \""
         << EscapeJson(report.package_manager.binder_service_manager_path)
         << "\",\n"
         << "    \"errors\": "
         << RenderJsonArray(report.package_manager.errors) << "\n"
         << "  },\n"
         << "  \"intent_resolution\": {\n"
         << "    \"ready\": "
         << (report.intent_resolution.ready ? "true" : "false") << ",\n"
         << "    \"session_id\": \""
         << EscapeJson(report.intent_resolution.session_id) << "\",\n"
         << "    \"artifact_root\": \""
         << EscapeJson(report.intent_resolution.artifact_root) << "\",\n"
         << "    \"resolution_json_path\": \""
         << EscapeJson(report.intent_resolution.resolution_json_path)
         << "\",\n"
         << "    \"package_name\": \""
         << EscapeJson(report.intent_resolution.package_name) << "\",\n"
         << "    \"requested_package_name\": \""
         << EscapeJson(report.intent_resolution.requested_package_name)
         << "\",\n"
         << "    \"requested_component\": \""
         << EscapeJson(report.intent_resolution.requested_component)
         << "\",\n"
         << "    \"resolution_mode\": \""
         << EscapeJson(report.intent_resolution.resolution_mode) << "\",\n"
         << "    \"action\": \""
         << EscapeJson(report.intent_resolution.action) << "\",\n"
         << "    \"categories\": "
         << RenderJsonArray(report.intent_resolution.categories) << ",\n"
         << "    \"candidate_components\": "
         << RenderJsonArray(report.intent_resolution.candidate_components)
         << ",\n"
         << "    \"matched_components\": "
         << RenderJsonArray(report.intent_resolution.matched_components)
         << ",\n"
         << "    \"resolved_component\": \""
         << EscapeJson(report.intent_resolution.resolved_component)
         << "\",\n"
         << "    \"resolved_activity_class\": \""
         << EscapeJson(report.intent_resolution.resolved_activity_class)
         << "\",\n"
         << "    \"launcher_match\": "
         << (report.intent_resolution.launcher_match ? "true" : "false")
         << ",\n"
         << "    \"resolution_status\": \""
         << EscapeJson(report.intent_resolution.resolution_status)
         << "\",\n"
         << "    \"resolution_reason\": \""
         << EscapeJson(report.intent_resolution.resolution_reason)
         << "\",\n"
         << "    \"blocking_reason\": \""
         << EscapeJson(report.intent_resolution.blocking_reason) << "\",\n"
         << "    \"recommended_recovery_action\": \""
         << EscapeJson(report.intent_resolution.recommended_recovery_action)
         << "\",\n"
         << "    \"errors\": "
         << RenderJsonArray(report.intent_resolution.errors) << "\n"
         << "  },\n"
         << "  \"activity_launch\": {\n"
         << "    \"ready\": "
         << (report.activity_launch.ready ? "true" : "false") << ",\n"
         << "    \"session_id\": \""
         << EscapeJson(report.activity_launch.session_id) << "\",\n"
         << "    \"artifact_root\": \""
         << EscapeJson(report.activity_launch.artifact_root) << "\",\n"
         << "    \"launch_record_path\": \""
         << EscapeJson(report.activity_launch.launch_record_path)
         << "\",\n"
         << "    \"package_name\": \""
         << EscapeJson(report.activity_launch.package_name) << "\",\n"
         << "    \"resolved_component\": \""
         << EscapeJson(report.activity_launch.resolved_component)
         << "\",\n"
         << "    \"activity_launch_status\": \""
         << EscapeJson(report.activity_launch.activity_launch_status)
         << "\",\n"
         << "    \"current_state\": \""
         << EscapeJson(report.activity_launch.current_state) << "\",\n"
         << "    \"blocking_reason\": \""
         << EscapeJson(report.activity_launch.blocking_reason) << "\",\n"
         << "    \"recommended_recovery_action\": \""
         << EscapeJson(report.activity_launch.recommended_recovery_action)
         << "\",\n"
         << "    \"dependency_blocked\": "
         << (report.activity_launch.dependency_blocked ? "true" : "false")
         << ",\n"
         << "    \"art_runtime_available\": "
         << (report.activity_launch.art_runtime_available ? "true" : "false")
         << ",\n"
         << "    \"java_execution_supported\": "
         << (report.activity_launch.java_execution_supported ? "true"
                                                             : "false")
         << ",\n"
         << "    \"dex_bootstrap_ready\": "
         << (report.activity_launch.dex_bootstrap_ready ? "true" : "false")
         << ",\n"
         << "    \"surface_health\": \""
         << EscapeJson(report.activity_launch.surface_health) << "\",\n"
         << "    \"lifecycle_health\": \""
         << EscapeJson(report.activity_launch.lifecycle_health) << "\",\n"
         << "    \"looper_health\": \""
         << EscapeJson(report.activity_launch.looper_health) << "\",\n"
         << "    \"input_health\": \""
         << EscapeJson(report.activity_launch.input_health) << "\",\n"
         << "    \"dex_health\": \""
         << EscapeJson(report.activity_launch.dex_health) << "\",\n"
         << "    \"art_health\": \""
         << EscapeJson(report.activity_launch.art_health) << "\",\n"
         << "    \"binder_health\": \""
         << EscapeJson(report.activity_launch.binder_health) << "\",\n"
         << "    \"binder_service_registry_status\": \""
         << EscapeJson(report.activity_launch.binder_service_registry_status)
         << "\",\n"
         << "    \"binder_service_manager_path\": \""
         << EscapeJson(report.activity_launch.binder_service_manager_path)
         << "\",\n"
         << "    \"states_visited\": "
         << RenderJsonArray(report.activity_launch.states_visited) << ",\n"
         << "    \"dependency_details\": "
         << RenderJsonArray(report.activity_launch.dependency_details)
         << ",\n"
         << "    \"errors\": "
         << RenderJsonArray(report.activity_launch.errors) << "\n"
         << "  },\n"
         << "  \"activity_manager\": {\n"
         << "    \"schema_version\": \""
         << EscapeJson(report.activity_manager.schema_version) << "\",\n"
         << "    \"ready\": "
         << (report.activity_manager.ready ? "true" : "false") << ",\n"
         << "    \"contract_ready\": "
         << (report.activity_manager.contract_ready ? "true" : "false")
         << ",\n"
         << "    \"session_id\": \""
         << EscapeJson(report.activity_manager.session_id) << "\",\n"
         << "    \"artifact_root\": \""
         << EscapeJson(report.activity_manager.artifact_root) << "\",\n"
         << "    \"report_json_path\": \""
         << EscapeJson(report.activity_manager.report_json_path)
         << "\",\n"
         << "    \"package_name\": \""
         << EscapeJson(report.activity_manager.package_name) << "\",\n"
         << "    \"user_id\": " << report.activity_manager.user_id << ",\n"
         << "    \"app_id\": " << report.activity_manager.app_id << ",\n"
         << "    \"sandbox_root\": \""
         << EscapeJson(report.activity_manager.sandbox_root) << "\",\n"
         << "    \"app_data_dir\": \""
         << EscapeJson(report.activity_manager.app_data_dir) << "\",\n"
         << "    \"apk_path\": \""
         << EscapeJson(report.activity_manager.apk_path) << "\",\n"
         << "    \"staged_dir\": \""
         << EscapeJson(report.activity_manager.staged_dir) << "\",\n"
         << "    \"updated_at_unix_ms\": "
         << report.activity_manager.updated_at_unix_ms << ",\n"
         << "    \"requested_package_name\": \""
         << EscapeJson(report.activity_manager.requested_package_name)
         << "\",\n"
         << "    \"requested_component\": \""
         << EscapeJson(report.activity_manager.requested_component)
         << "\",\n"
         << "    \"resolved_component\": \""
         << EscapeJson(report.activity_manager.resolved_component)
         << "\",\n"
         << "    \"launch_component\": \""
         << EscapeJson(report.activity_manager.launch_component)
         << "\",\n"
         << "    \"process_name\": \""
         << EscapeJson(report.activity_manager.process_name) << "\",\n"
         << "    \"start_reason\": \""
         << EscapeJson(report.activity_manager.start_reason) << "\",\n"
         << "    \"resolution_mode\": \""
         << EscapeJson(report.activity_manager.resolution_mode) << "\",\n"
         << "    \"intent_action\": \""
         << EscapeJson(report.activity_manager.intent_action) << "\",\n"
         << "    \"intent_categories\": "
         << RenderJsonArray(report.activity_manager.intent_categories)
         << ",\n"
         << "    \"resolution_status\": \""
         << EscapeJson(report.activity_manager.resolution_status)
         << "\",\n"
         << "    \"resolution_reason\": \""
         << EscapeJson(report.activity_manager.resolution_reason)
         << "\",\n"
         << "    \"launch_state\": \""
         << EscapeJson(report.activity_manager.launch_state) << "\",\n"
         << "    \"lifecycle_state\": \""
         << EscapeJson(report.activity_manager.lifecycle_state) << "\",\n"
         << "    \"blocking_reason\": \""
         << EscapeJson(report.activity_manager.blocking_reason) << "\",\n"
         << "    \"recommended_recovery_action\": \""
         << EscapeJson(report.activity_manager.recommended_recovery_action)
         << "\",\n"
         << "    \"restart_policy\": \""
         << EscapeJson(report.activity_manager.restart_policy) << "\",\n"
         << "    \"termination_policy\": \""
         << EscapeJson(report.activity_manager.termination_policy)
         << "\",\n"
         << "    \"storage_health\": \""
         << EscapeJson(report.activity_manager.storage_health) << "\",\n"
         << "    \"sandbox_health\": \""
         << EscapeJson(report.activity_manager.sandbox_health) << "\",\n"
         << "    \"permission_health\": \""
         << EscapeJson(report.activity_manager.permission_health)
         << "\",\n"
         << "    \"app_ops_health\": \""
         << EscapeJson(report.activity_manager.app_ops_health) << "\",\n"
         << "    \"binder_health\": \""
         << EscapeJson(report.activity_manager.binder_health) << "\",\n"
         << "    \"surface_health\": \""
         << EscapeJson(report.activity_manager.surface_health) << "\",\n"
         << "    \"lifecycle_health\": \""
         << EscapeJson(report.activity_manager.lifecycle_health) << "\",\n"
         << "    \"looper_health\": \""
         << EscapeJson(report.activity_manager.looper_health) << "\",\n"
         << "    \"input_health\": \""
         << EscapeJson(report.activity_manager.input_health) << "\",\n"
         << "    \"dex_health\": \""
         << EscapeJson(report.activity_manager.dex_health) << "\",\n"
         << "    \"art_health\": \""
         << EscapeJson(report.activity_manager.art_health) << "\",\n"
         << "    \"healing_actions\": "
         << RenderJsonArray(report.activity_manager.healing_actions)
         << ",\n"
         << "    \"diagnostics\": "
         << RenderJsonArray(report.activity_manager.diagnostics) << ",\n"
         << "    \"errors\": "
         << RenderJsonArray(report.activity_manager.errors) << "\n"
         << "  },\n"
         << "  \"process_manager\": {\n"
         << "    \"schema_version\": \""
         << EscapeJson(report.process_manager.schema_version) << "\",\n"
         << "    \"ready\": "
         << (report.process_manager.ready ? "true" : "false") << ",\n"
         << "    \"contract_ready\": "
         << (report.process_manager.contract_ready ? "true" : "false")
         << ",\n"
         << "    \"session_id\": \""
         << EscapeJson(report.process_manager.session_id) << "\",\n"
         << "    \"artifact_root\": \""
         << EscapeJson(report.process_manager.artifact_root) << "\",\n"
         << "    \"report_json_path\": \""
         << EscapeJson(report.process_manager.report_json_path)
         << "\",\n"
         << "    \"package_name\": \""
         << EscapeJson(report.process_manager.package_name) << "\",\n"
         << "    \"user_id\": " << report.process_manager.user_id << ",\n"
         << "    \"app_id\": " << report.process_manager.app_id << ",\n"
         << "    \"uid_placeholder\": "
         << report.process_manager.uid_placeholder << ",\n"
         << "    \"gid_placeholder\": "
         << report.process_manager.gid_placeholder << ",\n"
         << "    \"sandbox_root\": \""
         << EscapeJson(report.process_manager.sandbox_root) << "\",\n"
         << "    \"app_data_dir\": \""
         << EscapeJson(report.process_manager.app_data_dir) << "\",\n"
         << "    \"apk_path\": \""
         << EscapeJson(report.process_manager.apk_path) << "\",\n"
         << "    \"staged_dir\": \""
         << EscapeJson(report.process_manager.staged_dir) << "\",\n"
         << "    \"updated_at_unix_ms\": "
         << report.process_manager.updated_at_unix_ms << ",\n"
         << "    \"process_identity\": \""
         << EscapeJson(report.process_manager.process_identity)
         << "\",\n"
         << "    \"process_name\": \""
         << EscapeJson(report.process_manager.process_name) << "\",\n"
         << "    \"pid_value\": " << report.process_manager.pid_value
         << ",\n"
         << "    \"pid_source\": \""
         << EscapeJson(report.process_manager.pid_source) << "\",\n"
         << "    \"launch_component\": \""
         << EscapeJson(report.process_manager.launch_component)
         << "\",\n"
         << "    \"start_reason\": \""
         << EscapeJson(report.process_manager.start_reason) << "\",\n"
         << "    \"process_state\": \""
         << EscapeJson(report.process_manager.process_state) << "\",\n"
         << "    \"lifecycle_state\": \""
         << EscapeJson(report.process_manager.lifecycle_state) << "\",\n"
         << "    \"restart_policy\": \""
         << EscapeJson(report.process_manager.restart_policy) << "\",\n"
         << "    \"termination_policy\": \""
         << EscapeJson(report.process_manager.termination_policy)
         << "\",\n"
         << "    \"intent_action\": \""
         << EscapeJson(report.process_manager.intent_action) << "\",\n"
         << "    \"intent_categories\": "
         << RenderJsonArray(report.process_manager.intent_categories)
         << ",\n"
         << "    \"blocking_reason\": \""
         << EscapeJson(report.process_manager.blocking_reason) << "\",\n"
         << "    \"recommended_recovery_action\": \""
         << EscapeJson(report.process_manager.recommended_recovery_action)
         << "\",\n"
         << "    \"dependency_details\": "
         << RenderJsonArray(report.process_manager.dependency_details)
         << ",\n"
         << "    \"healing_actions\": "
         << RenderJsonArray(report.process_manager.healing_actions)
         << ",\n"
         << "    \"diagnostics\": "
         << RenderJsonArray(report.process_manager.diagnostics) << ",\n"
         << "    \"errors\": "
         << RenderJsonArray(report.process_manager.errors) << "\n"
         << "  },\n"
         << "  \"window_manager\": {\n"
         << "    \"schema_version\": \""
         << EscapeJson(report.window_manager.schema_version) << "\",\n"
         << "    \"ready\": "
         << (report.window_manager.ready ? "true" : "false") << ",\n"
         << "    \"contract_ready\": "
         << (report.window_manager.contract_ready ? "true" : "false")
         << ",\n"
         << "    \"session_id\": \""
         << EscapeJson(report.window_manager.session_id) << "\",\n"
         << "    \"artifact_root\": \""
         << EscapeJson(report.window_manager.artifact_root) << "\",\n"
         << "    \"report_json_path\": \""
         << EscapeJson(report.window_manager.report_json_path) << "\",\n"
         << "    \"session_map_path\": \""
         << EscapeJson(report.window_manager.session_map_path) << "\",\n"
         << "    \"event_log_path\": \""
         << EscapeJson(report.window_manager.event_log_path) << "\",\n"
         << "    \"package_name\": \""
         << EscapeJson(report.window_manager.package_name) << "\",\n"
         << "    \"user_id\": " << report.window_manager.user_id << ",\n"
         << "    \"app_id\": " << report.window_manager.app_id << ",\n"
         << "    \"uid_placeholder\": "
         << report.window_manager.uid_placeholder << ",\n"
         << "    \"gid_placeholder\": "
         << report.window_manager.gid_placeholder << ",\n"
         << "    \"sandbox_root\": \""
         << EscapeJson(report.window_manager.sandbox_root) << "\",\n"
         << "    \"app_data_dir\": \""
         << EscapeJson(report.window_manager.app_data_dir) << "\",\n"
         << "    \"apk_path\": \""
         << EscapeJson(report.window_manager.apk_path) << "\",\n"
         << "    \"staged_dir\": \""
         << EscapeJson(report.window_manager.staged_dir) << "\",\n"
         << "    \"updated_at_unix_ms\": "
         << report.window_manager.updated_at_unix_ms << ",\n"
         << "    \"window_id\": \""
         << EscapeJson(report.window_manager.window_id) << "\",\n"
         << "    \"process_identity\": \""
         << EscapeJson(report.window_manager.process_identity) << "\",\n"
         << "    \"process_name\": \""
         << EscapeJson(report.window_manager.process_name) << "\",\n"
         << "    \"pid_value\": " << report.window_manager.pid_value << ",\n"
         << "    \"pid_source\": \""
         << EscapeJson(report.window_manager.pid_source) << "\",\n"
         << "    \"launch_component\": \""
         << EscapeJson(report.window_manager.launch_component) << "\",\n"
         << "    \"activity_component\": \""
         << EscapeJson(report.window_manager.activity_component) << "\",\n"
         << "    \"lifecycle_state\": \""
         << EscapeJson(report.window_manager.lifecycle_state) << "\",\n"
         << "    \"surface_state\": \""
         << EscapeJson(report.window_manager.surface_state) << "\",\n"
         << "    \"backend\": \""
         << EscapeJson(report.window_manager.backend) << "\",\n"
         << "    \"backing_mode\": \""
         << EscapeJson(report.window_manager.backing_mode) << "\",\n"
         << "    \"headless_safe\": "
         << (report.window_manager.headless_safe ? "true" : "false")
         << ",\n"
         << "    \"wayland_surface_available\": "
         << (report.window_manager.wayland_surface_available ? "true"
                                                             : "false")
         << ",\n"
         << "    \"egl_surface_available\": "
         << (report.window_manager.egl_surface_available ? "true"
                                                         : "false")
         << ",\n"
         << "    \"width\": " << report.window_manager.width << ",\n"
         << "    \"height\": " << report.window_manager.height << ",\n"
         << "    \"format\": " << report.window_manager.format << ",\n"
         << "    \"surface_session_id\": \""
         << EscapeJson(report.window_manager.surface_session_id) << "\",\n"
         << "    \"surface_session_root\": \""
         << EscapeJson(report.window_manager.surface_session_root)
         << "\",\n"
         << "    \"surface_metadata_path\": \""
         << EscapeJson(report.window_manager.surface_metadata_path)
         << "\",\n"
         << "    \"surface_event_log_path\": \""
         << EscapeJson(report.window_manager.surface_event_log_path)
         << "\",\n"
         << "    \"marker_path\": \""
         << EscapeJson(report.window_manager.marker_path) << "\",\n"
         << "    \"surface_created\": "
         << (report.window_manager.surface_created ? "true" : "false")
         << ",\n"
         << "    \"attached\": "
         << (report.window_manager.attached ? "true" : "false") << ",\n"
         << "    \"visible\": "
         << (report.window_manager.visible ? "true" : "false") << ",\n"
         << "    \"hidden\": "
         << (report.window_manager.hidden ? "true" : "false") << ",\n"
         << "    \"resized\": "
         << (report.window_manager.resized ? "true" : "false") << ",\n"
         << "    \"destroyed\": "
         << (report.window_manager.destroyed ? "true" : "false") << ",\n"
         << "    \"failed\": "
         << (report.window_manager.failed ? "true" : "false") << ",\n"
         << "    \"recovered\": "
         << (report.window_manager.recovered ? "true" : "false") << ",\n"
         << "    \"window_state\": \""
         << EscapeJson(report.window_manager.window_state) << "\",\n"
         << "    \"blocking_reason\": \""
         << EscapeJson(report.window_manager.blocking_reason) << "\",\n"
         << "    \"recommended_recovery_action\": \""
         << EscapeJson(report.window_manager.recommended_recovery_action)
         << "\",\n"
         << "    \"states_visited\": "
         << RenderJsonArray(report.window_manager.states_visited) << ",\n"
         << "    \"healing_actions\": "
         << RenderJsonArray(report.window_manager.healing_actions)
         << ",\n"
         << "    \"diagnostics\": "
         << RenderJsonArray(report.window_manager.diagnostics) << ",\n"
         << "    \"errors\": "
         << RenderJsonArray(report.window_manager.errors) << "\n"
         << "  },\n"
         << "  \"storage\": {\n"
         << "    \"ready\": " << (report.storage.ready ? "true" : "false")
         << ",\n"
         << "    \"session_id\": \"" << EscapeJson(report.storage.session_id)
         << "\",\n"
         << "    \"artifact_root\": \""
         << EscapeJson(report.storage.artifact_root) << "\",\n"
         << "    \"report_json_path\": \""
         << EscapeJson(report.storage.report_json_path) << "\",\n"
         << "    \"package_name\": \""
         << EscapeJson(report.storage.package_name) << "\",\n"
         << "    \"apk_path\": \"" << EscapeJson(report.storage.apk_path)
         << "\",\n"
         << "    \"staged_dir\": \"" << EscapeJson(report.storage.staged_dir)
         << "\",\n"
         << "    \"app_data_dir\": \""
         << EscapeJson(report.storage.app_data_dir) << "\",\n"
         << "    \"files_dir\": \"" << EscapeJson(report.storage.files_dir)
         << "\",\n"
         << "    \"cache_dir\": \"" << EscapeJson(report.storage.cache_dir)
         << "\",\n"
         << "    \"native_lib_dir\": \""
         << EscapeJson(report.storage.native_lib_dir) << "\",\n"
         << "    \"asset_root\": \"" << EscapeJson(report.storage.asset_root)
         << "\",\n"
         << "    \"resource_root\": \""
         << EscapeJson(report.storage.resource_root) << "\",\n"
         << "    \"marker_path\": \"" << EscapeJson(report.storage.marker_path)
         << "\",\n"
         << "    \"marker_size\": " << report.storage.marker_size << ",\n"
         << "    \"marker_written\": "
         << (report.storage.marker_written ? "true" : "false") << ",\n"
         << "    \"marker_read_back\": "
         << (report.storage.marker_read_back ? "true" : "false") << ",\n"
         << "    \"marker_checksum\": \""
         << EscapeJson(report.storage.marker_checksum) << "\",\n"
         << "    \"uid_placeholder\": " << report.storage.uid_placeholder
         << ",\n"
         << "    \"gid_placeholder\": " << report.storage.gid_placeholder
         << ",\n"
         << "    \"isolation_level\": \""
         << EscapeJson(report.storage.isolation_level) << "\",\n"
         << "    \"sandbox_state\": \""
         << EscapeJson(report.storage.sandbox_state) << "\",\n"
         << "    \"permission_metadata_ready\": "
         << (report.storage.permission_metadata_ready ? "true" : "false")
         << ",\n"
         << "    \"permission_metadata\": "
         << RenderJsonArray(report.storage.permission_metadata) << ",\n"
         << "    \"rejected_escape_paths\": "
         << report.storage.rejected_escape_paths << ",\n"
         << "    \"accepted_paths\": "
         << RenderJsonArray(report.storage.accepted_paths) << ",\n"
         << "    \"rejected_paths\": "
         << RenderJsonArray(report.storage.rejected_paths) << ",\n"
         << "    \"errors\": " << RenderJsonArray(report.storage.errors)
         << "\n"
         << "  },\n"
         << "  \"permissions\": {\n"
         << "    \"schema_version\": \""
         << EscapeJson(report.permissions.schema_version) << "\",\n"
         << "    \"ready\": "
         << (report.permissions.ready ? "true" : "false") << ",\n"
         << "    \"contract_ready\": "
         << (report.permissions.contract_ready ? "true" : "false")
         << ",\n"
         << "    \"session_id\": \""
         << EscapeJson(report.permissions.session_id) << "\",\n"
         << "    \"artifact_root\": \""
         << EscapeJson(report.permissions.artifact_root) << "\",\n"
         << "    \"report_json_path\": \""
         << EscapeJson(report.permissions.report_json_path) << "\",\n"
         << "    \"package_name\": \""
         << EscapeJson(report.permissions.package_name) << "\",\n"
         << "    \"user_id\": " << report.permissions.user_id << ",\n"
         << "    \"app_id\": " << report.permissions.app_id << ",\n"
         << "    \"sandbox_root\": \""
         << EscapeJson(report.permissions.sandbox_root) << "\",\n"
         << "    \"app_data_dir\": \""
         << EscapeJson(report.permissions.app_data_dir) << "\",\n"
         << "    \"apk_path\": \""
         << EscapeJson(report.permissions.apk_path) << "\",\n"
         << "    \"staged_dir\": \""
         << EscapeJson(report.permissions.staged_dir) << "\",\n"
         << "    \"updated_at_unix_ms\": "
         << report.permissions.updated_at_unix_ms << ",\n"
         << "    \"requested_permissions\": "
         << RenderJsonArray(report.permissions.requested_permissions) << ",\n"
         << "    \"granted_permissions\": "
         << RenderJsonArray(report.permissions.granted_permissions) << ",\n"
         << "    \"denied_permissions\": "
         << RenderJsonArray(report.permissions.denied_permissions) << ",\n"
         << "    \"decode_level\": \""
         << EscapeJson(report.permissions.decode_level) << "\",\n"
         << "    \"permission_records\": "
         << RenderPermissionRecordsArray(report.permissions.permission_records)
         << ",\n"
         << "    \"healing_actions\": "
         << RenderJsonArray(report.permissions.healing_actions) << ",\n"
         << "    \"diagnostics\": "
         << RenderJsonArray(report.permissions.diagnostics) << ",\n"
         << "    \"errors\": "
         << RenderJsonArray(report.permissions.errors) << "\n"
         << "  },\n"
         << "  \"app_ops\": {\n"
         << "    \"schema_version\": \""
         << EscapeJson(report.app_ops.schema_version) << "\",\n"
         << "    \"ready\": "
         << (report.app_ops.ready ? "true" : "false") << ",\n"
         << "    \"contract_ready\": "
         << (report.app_ops.contract_ready ? "true" : "false") << ",\n"
         << "    \"session_id\": \""
         << EscapeJson(report.app_ops.session_id) << "\",\n"
         << "    \"artifact_root\": \""
         << EscapeJson(report.app_ops.artifact_root) << "\",\n"
         << "    \"report_json_path\": \""
         << EscapeJson(report.app_ops.report_json_path) << "\",\n"
         << "    \"package_name\": \""
         << EscapeJson(report.app_ops.package_name) << "\",\n"
         << "    \"user_id\": " << report.app_ops.user_id << ",\n"
         << "    \"app_id\": " << report.app_ops.app_id << ",\n"
         << "    \"sandbox_root\": \""
         << EscapeJson(report.app_ops.sandbox_root) << "\",\n"
         << "    \"app_data_dir\": \""
         << EscapeJson(report.app_ops.app_data_dir) << "\",\n"
         << "    \"apk_path\": \"" << EscapeJson(report.app_ops.apk_path)
         << "\",\n"
         << "    \"staged_dir\": \""
         << EscapeJson(report.app_ops.staged_dir) << "\",\n"
         << "    \"updated_at_unix_ms\": "
         << report.app_ops.updated_at_unix_ms << ",\n"
         << "    \"operations_count\": " << report.app_ops.operations_count
         << ",\n"
         << "    \"allowed_operations\": "
         << RenderJsonArray(report.app_ops.allowed_operations) << ",\n"
         << "    \"denied_operations\": "
         << RenderJsonArray(report.app_ops.denied_operations) << ",\n"
         << "    \"default_operations\": "
         << RenderJsonArray(report.app_ops.default_operations) << ",\n"
         << "    \"ignored_placeholder_operations\": "
         << RenderJsonArray(report.app_ops.ignored_placeholder_operations)
         << ",\n"
         << "    \"app_ops\": "
         << RenderAppOpRecordsArray(report.app_ops.operation_records)
         << ",\n"
         << "    \"healing_actions\": "
         << RenderJsonArray(report.app_ops.healing_actions) << ",\n"
         << "    \"diagnostics\": "
         << RenderJsonArray(report.app_ops.diagnostics) << ",\n"
         << "    \"errors\": " << RenderJsonArray(report.app_ops.errors)
         << "\n"
         << "  },\n"
         << "  \"self_healing_android_device\": {\n"
         << "    \"ready\": "
         << (report.self_healing_android_device.ready ? "true" : "false")
         << ",\n"
         << "    \"phase_name\": \""
         << EscapeJson(report.self_healing_android_device.phase_name)
         << "\",\n"
         << "    \"session_id\": \""
         << EscapeJson(report.self_healing_android_device.session_id)
         << "\",\n"
         << "    \"artifact_root\": \""
         << EscapeJson(report.self_healing_android_device.artifact_root)
         << "\",\n"
         << "    \"report_json_path\": \""
         << EscapeJson(report.self_healing_android_device.report_json_path)
         << "\",\n"
         << "    \"journal_path\": \""
         << EscapeJson(report.self_healing_android_device.journal_path)
         << "\",\n"
         << "    \"initial_health\": \""
         << EscapeJson(report.self_healing_android_device.initial_health)
         << "\",\n"
         << "    \"final_health\": \""
         << EscapeJson(report.self_healing_android_device.final_health)
         << "\",\n"
         << "    \"storage_health\": \""
         << EscapeJson(report.self_healing_android_device.storage_health)
         << "\",\n"
         << "    \"sandbox_health\": \""
         << EscapeJson(report.self_healing_android_device.sandbox_health)
         << "\",\n"
         << "    \"permission_health\": \""
         << EscapeJson(report.self_healing_android_device.permission_health)
         << "\",\n"
         << "    \"app_ops_health\": \""
         << EscapeJson(report.self_healing_android_device.app_ops_health)
         << "\",\n"
         << "    \"activity_manager_health\": \""
         << EscapeJson(
                report.self_healing_android_device.activity_manager_health)
         << "\",\n"
         << "    \"process_health\": \""
         << EscapeJson(report.self_healing_android_device.process_health)
         << "\",\n"
         << "    \"window_health\": \""
         << EscapeJson(report.self_healing_android_device.window_health)
         << "\",\n"
         << "    \"recoverable\": "
         << (report.self_healing_android_device.recoverable ? "true"
                                                            : "false")
         << ",\n"
         << "    \"actions_attempted\": "
         << report.self_healing_android_device.actions_attempted << ",\n"
         << "    \"actions_succeeded\": "
         << report.self_healing_android_device.actions_succeeded << ",\n"
         << "    \"actions_failed\": "
         << report.self_healing_android_device.actions_failed << ",\n"
         << "    \"recommended_next_action\": \""
         << EscapeJson(
                report.self_healing_android_device.recommended_next_action)
         << "\",\n"
         << "    \"actions\": "
         << RenderSelfHealingActionsArray(
                report.self_healing_android_device.actions)
         << ",\n"
         << "    \"limitation_flags\": "
         << RenderJsonArray(report.self_healing_android_device.limitation_flags)
         << ",\n"
         << "    \"errors\": "
         << RenderJsonArray(report.self_healing_android_device.errors) << "\n"
         << "  },\n"
         << "  \"native_execute\": {\n"
         << "    \"selected_library_path\": \""
         << EscapeJson(report.native_execute.selected_library_path) << "\",\n"
         << "    \"entrypoint_found\": "
         << (report.native_execute.entrypoint_found ? "true" : "false")
         << ",\n"
         << "    \"activity_called\": "
         << (report.native_execute.activity_called ? "true" : "false")
         << ",\n"
         << "    \"execution_engine_ready\": "
         << (report.native_execute.execution_engine_ready ? "true" : "false")
         << ",\n"
         << "    \"exit_reason\": \""
         << EscapeJson(report.native_execute.exit_reason) << "\",\n"
         << "    \"exit_code\": " << report.native_execute.exit_code << "\n"
         << "  }\n"
         << "}\n";
  return output.str();
}

}  // namespace wfa
