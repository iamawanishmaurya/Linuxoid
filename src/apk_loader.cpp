#include "wfa/apk_loader.hpp"
#include "wfa/apk_archive.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace wfa {

namespace fs = std::filesystem;

namespace {

struct DecodedApkInspection {
  ApktoolMetadata metadata;
  ManifestProfile profile;
  ManifestAssessment assessment;
  std::string manifest_xml;
};

std::string ExtractFirstMatch(std::string_view text, const std::regex& pattern);
std::string QuoteForShell(const std::string& value);
std::string RunCommandCapture(const std::string& command);

void CopyDirectoryContents(const fs::path& source, const fs::path& destination) {
  if (!fs::exists(source)) {
    return;
  }
  fs::create_directories(destination);
  for (const auto& entry : fs::recursive_directory_iterator(source)) {
    const fs::path relative = fs::relative(entry.path(), source);
    const fs::path target = destination / relative;
    if (entry.is_directory()) {
      fs::create_directories(target);
      continue;
    }
    if (entry.is_regular_file()) {
      fs::create_directories(target.parent_path());
      fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing);
    }
  }
}

void MaterializeDecodedPayload(const fs::path& apk, const fs::path& install_root) {
  std::string decode_template =
      (fs::temp_directory_path() / "wfa-stage-XXXXXX").string();
  std::unique_ptr<char[]> decode_buffer(new char[decode_template.size() + 1]);
  std::snprintf(decode_buffer.get(), decode_template.size() + 1, "%s",
                decode_template.c_str());
  char* decode_dir = mkdtemp(decode_buffer.get());
  if (decode_dir == nullptr) {
    throw std::runtime_error("failed to create staging decode directory");
  }

  const fs::path decode_root = decode_dir;
  struct DecodeCleanup {
    fs::path path;
    ~DecodeCleanup() {
      std::error_code ignored;
      fs::remove_all(path, ignored);
    }
  } cleanup{decode_root};

  const std::string command = "apktool d -f -s -o " +
                              QuoteForShell(decode_root.string()) + " " +
                              QuoteForShell(apk.string()) + " >/dev/null";
  RunCommandCapture(command);

  std::error_code ignored;
  fs::remove_all(install_root / "lib", ignored);
  fs::remove_all(install_root / "assets", ignored);
  fs::remove_all(install_root / "res", ignored);
  CopyDirectoryContents(decode_root / "lib", install_root / "lib");
  CopyDirectoryContents(decode_root / "assets", install_root / "assets");
  CopyDirectoryContents(decode_root / "res", install_root / "res");
}

std::string QuoteForShell(const std::string& value) {
  std::string quoted = "'";
  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(character);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

std::string RunCommandCapture(const std::string& command) {
  std::array<char, 4096> buffer{};
  std::string output;

  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    throw std::runtime_error("failed to start command: " + command);
  }

  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }

  const int rc = pclose(pipe);
  if (rc != 0) {
    throw std::runtime_error("command failed: " + command + "\n" + output);
  }

  return output;
}

std::string ReadFile(const fs::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to open file: " + path.string());
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string ExtractFirstMatch(std::string_view text, const std::regex& pattern) {
  std::match_results<std::string_view::const_iterator> match;
  if (!std::regex_search(text.begin(), text.end(), match, pattern) ||
      match.size() < 2) {
    return {};
  }
  return std::string(match[1].first, match[1].second);
}

std::string ExtractYamlString(std::string_view text, const std::string& key) {
  const std::regex pattern("^\\s*" + key + R"(:\s*(.+)$)",
                           std::regex_constants::multiline);
  std::match_results<std::string_view::const_iterator> match;
  if (!std::regex_search(text.begin(), text.end(), match, pattern) ||
      match.size() < 2) {
    return {};
  }

  std::string value(match[1].first, match[1].second);
  if (!value.empty() && value.front() == '"' && value.back() == '"') {
    value = value.substr(1, value.size() - 2);
  }
  return value;
}

int ExtractYamlInt(std::string_view text, const std::string& key) {
  const std::string value = ExtractYamlString(text, key);
  if (value.empty()) {
    return 0;
  }
  return std::stoi(value);
}

int ExtractManifestSdkInt(std::string_view xml, const std::string& attribute) {
  const std::regex pattern("<uses-sdk[^>]*" + attribute + "=\"([^\"]+)\"");
  const std::string value = ExtractFirstMatch(xml, pattern);
  if (value.empty()) {
    return 0;
  }
  if (!std::regex_match(value, std::regex("[0-9]+"))) {
    return 0;
  }
  return std::stoi(value);
}

std::string ExtractManifestApplicationName(std::string_view xml) {
  return ExtractFirstMatch(
      xml, std::regex("<application[^>]*android:name=\"([^\"]+)\""));
}

std::vector<std::string> ExtractManifestActivityNames(std::string_view xml) {
  std::vector<std::string> activity_names;
  const std::regex pattern(
      "<(activity|activity-alias)[^>]*android:name=\"([^\"]+)\"");
  const char* begin = xml.data();
  const char* end = xml.data() + xml.size();
  for (std::cregex_iterator it(begin, end, pattern), last; it != last; ++it) {
    const std::string activity_name = (*it)[2].str();
    if (activity_name.empty()) {
      continue;
    }
    if (std::find(activity_names.begin(), activity_names.end(), activity_name) ==
        activity_names.end()) {
      activity_names.push_back(activity_name);
    }
  }
  return activity_names;
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

std::string SanitizeInstallSegment(const std::string& value) {
  std::string sanitized;
  sanitized.reserve(value.size());

  for (const char character : value) {
    const bool safe = (character >= 'A' && character <= 'Z') ||
                      (character >= 'a' && character <= 'z') ||
                      (character >= '0' && character <= '9') ||
                      character == '.' || character == '_' || character == '-';
    sanitized.push_back(safe ? character : '_');
  }

  while (sanitized.find("__") != std::string::npos) {
    sanitized = std::regex_replace(sanitized, std::regex("__"), "_");
  }

  if (sanitized.empty()) {
    sanitized = "unknown";
  }

  return sanitized;
}

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

void AppendUnique(std::vector<std::string>& values, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

std::vector<std::string> CollectFilesystemAssets(const fs::path& asset_root) {
  std::vector<std::string> assets;
  if (!fs::exists(asset_root)) {
    return assets;
  }
  for (const auto& entry : fs::recursive_directory_iterator(asset_root)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    assets.push_back(fs::relative(entry.path(), asset_root).generic_string());
  }
  std::sort(assets.begin(), assets.end());
  return assets;
}

std::vector<std::string> CollectArchiveAssets(
    const std::vector<ApkArchiveEntry>& entries) {
  std::vector<std::string> assets;
  for (const auto& entry : entries) {
    if (entry.is_directory || entry.path.rfind("assets/", 0) != 0) {
      continue;
    }
    assets.push_back(entry.path.substr(std::string("assets/").size()));
  }
  std::sort(assets.begin(), assets.end());
  return assets;
}

void PopulateManifestFromXml(ApkResourceReadinessReport* report,
                             const std::string& manifest_xml,
                             const std::string& manifest_source) {
  const auto profile = ParseDecodedManifest(manifest_xml);
  report->manifest.manifest_present = true;
  report->manifest.manifest_ready = true;
  report->manifest.manifest_source = manifest_source;
  report->manifest.package_name = profile.package_name;
  report->manifest.min_sdk =
      ExtractManifestSdkInt(manifest_xml, "android:minSdkVersion");
  report->manifest.target_sdk =
      ExtractManifestSdkInt(manifest_xml, "android:targetSdkVersion");
  report->manifest.application_name =
      ExtractManifestApplicationName(manifest_xml);
  report->manifest.activity_names =
      ExtractManifestActivityNames(manifest_xml);
}

std::vector<fs::path> BuildManifestCandidates(const std::string& resource_root,
                                              const std::string& manifest_hint) {
  std::vector<fs::path> candidates;
  auto append_unique = [&](const fs::path& candidate) {
    if (candidate.empty()) {
      return;
    }
    if (std::find(candidates.begin(), candidates.end(), candidate) ==
        candidates.end()) {
      candidates.push_back(candidate);
    }
  };

  append_unique(fs::path(manifest_hint));
  if (!resource_root.empty()) {
    const fs::path resource_root_path(resource_root);
    append_unique(resource_root_path / "AndroidManifest.xml");
    append_unique(resource_root_path.parent_path() / "bundle" /
                  "AndroidManifest.xml");
    append_unique(resource_root_path.parent_path() / "AndroidManifest.xml");
  }
  return candidates;
}

bool TryPopulateManifestFromFilesystem(ApkResourceReadinessReport* report,
                                       const std::string& resource_root,
                                       const std::string& manifest_hint) {
  for (const auto& candidate : BuildManifestCandidates(resource_root,
                                                       manifest_hint)) {
    if (candidate.empty() || !fs::exists(candidate) || fs::is_directory(candidate)) {
      continue;
    }
    const std::string manifest_xml = ReadFile(candidate);
    if (manifest_xml.find("<manifest") == std::string::npos) {
      continue;
    }
    PopulateManifestFromXml(report, manifest_xml, "staged_bundle_manifest");
    return true;
  }
  return false;
}

DecodedApkInspection InspectDecodedApk(const fs::path& apk) {
  if (!fs::exists(apk)) {
    throw std::invalid_argument("apk path does not exist: " + apk.string());
  }

  std::string decode_template =
      (fs::temp_directory_path() / "wfa-decode-XXXXXX").string();
  std::unique_ptr<char[]> decode_buffer(new char[decode_template.size() + 1]);
  std::snprintf(decode_buffer.get(), decode_template.size() + 1, "%s",
                decode_template.c_str());
  char* decode_dir = mkdtemp(decode_buffer.get());
  if (decode_dir == nullptr) {
    throw std::runtime_error("failed to create unique decode directory");
  }

  const fs::path decode_root = decode_dir;
  struct DecodeCleanup {
    fs::path path;
    ~DecodeCleanup() {
      std::error_code ignored;
      fs::remove_all(path, ignored);
    }
  } cleanup{decode_root};

  const std::string command = "apktool d -f -s -o " +
                              QuoteForShell(decode_root.string()) + " " +
                              QuoteForShell(apk.string()) + " >/dev/null";
  RunCommandCapture(command);

  const auto manifest_xml = ReadFile(decode_root / "AndroidManifest.xml");
  const auto profile = ParseDecodedManifest(manifest_xml);
  return DecodedApkInspection{
      .metadata = ParseApktoolMetadata(ReadFile(decode_root / "apktool.yml")),
      .profile = profile,
      .assessment = AssessRuntimeRequirements(profile),
      .manifest_xml = manifest_xml,
  };
}

}  // namespace

ApktoolMetadata ParseApktoolMetadata(std::string_view yaml) {
  ApktoolMetadata metadata;
  metadata.apk_file_name = ExtractYamlString(yaml, "apkFileName");
  metadata.min_sdk = ExtractYamlInt(yaml, "minSdkVersion");
  metadata.target_sdk = ExtractYamlInt(yaml, "targetSdkVersion");
  metadata.version_code = ExtractYamlInt(yaml, "versionCode");
  metadata.version_name = ExtractYamlString(yaml, "versionName");

  if (metadata.apk_file_name.empty() || metadata.version_code <= 0 ||
      metadata.version_name.empty()) {
    throw std::invalid_argument(
        "apktool metadata is missing required version fields");
  }

  return metadata;
}

std::string BuildInstallId(const ApktoolMetadata& metadata) {
  return "vc" + std::to_string(metadata.version_code) + "-" +
         SanitizeInstallSegment(metadata.version_name);
}

std::string InspectApkPackageName(const std::string& apk_path) {
  return InspectDecodedApk(apk_path).profile.package_name;
}

ApkResourceReadinessReport InspectApkResourceReadiness(
    const std::string& apk_path, const std::string& resource_root,
    const std::string& manifest_hint_path) {
  const fs::path apk = apk_path;
  if (!fs::exists(apk)) {
    throw std::invalid_argument("apk path does not exist: " + apk.string());
  }

  ApkResourceReadinessReport report;
  report.apk_path = apk_path;
  report.resource_root_path = resource_root;

  OpenedApkArchive archive;
  bool archive_ready = false;
  try {
    archive = OpenApkArchive(apk_path);
    archive_ready = true;
  } catch (const std::exception& error) {
    report.errors.push_back("archive_open_failed: " + std::string(error.what()));
  }

  const auto& archive_entries = archive.entries;
  const auto manifest_entry = std::find_if(
      archive_entries.begin(), archive_entries.end(),
      [](const ApkArchiveEntry& entry) {
        return entry.path == "AndroidManifest.xml";
      });
  report.manifest.manifest_present = manifest_entry != archive_entries.end();

  if (report.manifest.manifest_present) {
    const auto manifest_read = ReadApkArchiveEntry(archive, "AndroidManifest.xml");
    if (manifest_read.found && manifest_read.readable &&
        manifest_read.contents.find("<manifest") != std::string::npos) {
      PopulateManifestFromXml(&report, manifest_read.contents,
                              "archive_plain_xml");
    }
  }

  if (!report.manifest.manifest_ready &&
      TryPopulateManifestFromFilesystem(&report, resource_root,
                                        manifest_hint_path)) {
    report.manifest.manifest_present = true;
  }

  if (!report.manifest.manifest_ready) {
    try {
      const auto decoded = InspectDecodedApk(apk);
      report.manifest.manifest_present = true;
      report.manifest.manifest_ready = true;
      report.manifest.manifest_source = "apktool_decoded_manifest";
      report.manifest.package_name = decoded.profile.package_name;
      report.manifest.min_sdk = decoded.metadata.min_sdk;
      report.manifest.target_sdk = decoded.metadata.target_sdk;
      report.manifest.application_name =
          ExtractManifestApplicationName(decoded.manifest_xml);
      report.manifest.activity_names =
          ExtractManifestActivityNames(decoded.manifest_xml);
    } catch (const std::exception& error) {
      if (!report.manifest.manifest_present) {
        report.errors.push_back("manifest_missing");
      } else {
        report.errors.push_back("manifest_unavailable: " +
                                std::string(error.what()));
      }
    }
  }

  const fs::path resource_root_path = resource_root;
  const fs::path asset_root = resource_root.empty()
                                  ? fs::path()
                                  : (resource_root_path / "assets");
  if (!resource_root.empty() &&
      (fs::exists(asset_root) || fs::exists(resource_root_path))) {
    report.asset_source = "staged_resource_root";
    report.asset_root_path =
        fs::exists(asset_root) ? asset_root.string() : resource_root;
    report.asset_paths = CollectFilesystemAssets(
        fs::exists(asset_root) ? asset_root : resource_root_path);
    report.asset_listing_ready = true;
    report.asset_read_ready = true;
    report.resources_table_present =
        fs::exists(resource_root_path / "resources.arsc") ||
        fs::exists(resource_root_path / "res");
  } else if (archive_ready && !archive_entries.empty()) {
    report.asset_source = "archive_entries";
    report.asset_root_path = "zip:" + apk_path + "!/assets";
    report.asset_paths = CollectArchiveAssets(archive_entries);
    report.asset_listing_ready = true;
    report.asset_read_ready =
        std::any_of(archive_entries.begin(), archive_entries.end(),
                    [](const ApkArchiveEntry& entry) {
                      return !entry.is_directory &&
                             entry.path.rfind("assets/", 0) == 0 &&
                             entry.compression_method == 0;
                    }) ||
        report.asset_paths.empty();
    report.resources_table_present = std::any_of(
        archive_entries.begin(), archive_entries.end(),
        [](const ApkArchiveEntry& entry) { return entry.path == "resources.arsc"; });
  }

  if (report.asset_listing_ready && report.asset_paths.empty()) {
    AppendUnique(report.errors, "asset_root_present_but_empty");
  }
  if (!report.asset_listing_ready) {
    AppendUnique(report.errors, "asset_listing_unavailable");
  }
  if (!report.asset_read_ready) {
    AppendUnique(report.errors, "asset_read_requires_stored_assets_or_stage_root");
  }
  if (!report.resources_table_present) {
    AppendUnique(report.errors, "resources_table_missing_or_not_staged");
  }

  return report;
}

std::string RenderApkResourceReadinessJson(
    const ApkResourceReadinessReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"manifest_ready\": "
         << (report.manifest.manifest_ready ? "true" : "false") << ",\n"
         << "  \"manifest_present\": "
         << (report.manifest.manifest_present ? "true" : "false") << ",\n"
         << "  \"manifest_source\": \""
         << EscapeJson(report.manifest.manifest_source) << "\",\n"
         << "  \"package_name\": \""
         << EscapeJson(report.manifest.package_name) << "\",\n"
         << "  \"min_sdk\": " << report.manifest.min_sdk << ",\n"
         << "  \"target_sdk\": " << report.manifest.target_sdk << ",\n"
         << "  \"application_name\": \""
         << EscapeJson(report.manifest.application_name) << "\",\n"
         << "  \"activity_names\": "
         << RenderJsonArray(report.manifest.activity_names) << ",\n"
         << "  \"asset_listing_ready\": "
         << (report.asset_listing_ready ? "true" : "false") << ",\n"
         << "  \"asset_read_ready\": "
         << (report.asset_read_ready ? "true" : "false") << ",\n"
         << "  \"asset_source\": \"" << EscapeJson(report.asset_source)
         << "\",\n"
         << "  \"asset_root_path\": \""
         << EscapeJson(report.asset_root_path) << "\",\n"
         << "  \"resource_root_path\": \""
         << EscapeJson(report.resource_root_path) << "\",\n"
         << "  \"asset_paths\": " << RenderJsonArray(report.asset_paths)
         << ",\n"
         << "  \"resources_table_present\": "
         << (report.resources_table_present ? "true" : "false") << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

std::string RenderLoadedApkReport(const LoadedApkReport& report) {
  std::ostringstream output;
  output << "APK Path: " << report.apk_path << '\n';
  output << "Package: " << report.manifest_profile.package_name << '\n';
  output << "Version: " << report.metadata.version_name << " ("
         << report.metadata.version_code << ")\n";
  output << "Min SDK: " << report.metadata.min_sdk << '\n';
  output << "Target SDK: " << report.metadata.target_sdk << '\n';
  output << "Install ID: " << report.install_id << '\n';
  output << "Install Root: " << report.install_root << '\n';
  output << "Earliest package load phase: " << report.assessment.earliest_load_phase
         << '\n';
  output << "Earliest settings/UI phase: " << report.assessment.earliest_ui_phase
         << '\n';
  output << "Earliest full-use phase: "
         << report.assessment.earliest_full_use_phase << '\n';
  return output.str();
}

LoadedApkReport LoadApkToCompatRoot(const std::string& apk_path,
                                    const std::string& compat_root) {
  const fs::path apk = apk_path;
  const auto inspection = InspectDecodedApk(apk);

  PackageInstallRequest install_request{
      .package_name = inspection.profile.package_name,
      .install_id = BuildInstallId(inspection.metadata),
      .version_code = inspection.metadata.version_code,
  };
  const auto layout = BuildPackageLayout(install_request, compat_root);

  fs::create_directories(layout.host_package_root);
  fs::create_directories(layout.host_data_root);
  fs::create_directories(layout.host_external_data_root);
  fs::create_directories(layout.host_obb_root);

  const fs::path installed_apk_path = fs::path(layout.host_package_root) / "base.apk";
  fs::copy_file(apk, installed_apk_path, fs::copy_options::overwrite_existing);
  WriteTextFile(fs::path(layout.host_package_root) / "AndroidManifest.xml",
                inspection.manifest_xml);
  WriteTextFile(
      fs::path(layout.host_package_root) / "assessment.txt",
      RenderManifestAssessmentReport(inspection.assessment));
  MaterializeDecodedPayload(apk, layout.host_package_root);

  std::ostringstream metadata_json;
  metadata_json << "{\n"
                << "  \"package_name\": \""
                << EscapeJson(inspection.profile.package_name)
                << "\",\n"
                << "  \"version_name\": \""
                << EscapeJson(inspection.metadata.version_name)
                << "\",\n"
                << "  \"version_code\": " << inspection.metadata.version_code
                << ",\n"
                << "  \"min_sdk\": " << inspection.metadata.min_sdk << ",\n"
                << "  \"target_sdk\": " << inspection.metadata.target_sdk
                << ",\n"
                << "  \"install_id\": \"" << EscapeJson(install_request.install_id)
                << "\",\n"
                << "  \"launcher_component\": \""
                << EscapeJson(inspection.profile.launcher_activity_name)
                << "\",\n"
                << "  \"earliest_load_phase\": \""
                << EscapeJson(inspection.assessment.earliest_load_phase)
                << "\",\n"
                << "  \"earliest_ui_phase\": \""
                << EscapeJson(inspection.assessment.earliest_ui_phase)
                << "\",\n"
                << "  \"earliest_full_use_phase\": \""
                << EscapeJson(inspection.assessment.earliest_full_use_phase)
                << "\"\n"
                << "}\n";
  WriteTextFile(layout.host_manifest_path, metadata_json.str());

  return LoadedApkReport{
      .apk_path = apk_path,
      .install_id = install_request.install_id,
      .metadata = inspection.metadata,
      .manifest_profile = inspection.profile,
      .assessment = inspection.assessment,
      .layout = layout,
      .install_root = layout.host_package_root,
  };
}

}  // namespace wfa
