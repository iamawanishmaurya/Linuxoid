#include "wfa/apk_loader.hpp"

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
  if (!fs::exists(apk)) {
    throw std::invalid_argument("apk path does not exist: " + apk_path);
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

  const auto metadata = ParseApktoolMetadata(ReadFile(decode_root / "apktool.yml"));
  const auto profile =
      ParseDecodedManifest(ReadFile(decode_root / "AndroidManifest.xml"));
  const auto assessment = AssessRuntimeRequirements(profile);

  PackageInstallRequest install_request{
      .package_name = profile.package_name,
      .install_id = BuildInstallId(metadata),
      .version_code = metadata.version_code,
  };
  const auto layout = BuildPackageLayout(install_request, compat_root);

  fs::create_directories(layout.host_package_root);
  fs::create_directories(layout.host_data_root);
  fs::create_directories(layout.host_external_data_root);
  fs::create_directories(layout.host_obb_root);

  const fs::path installed_apk_path = fs::path(layout.host_package_root) / "base.apk";
  fs::copy_file(apk, installed_apk_path, fs::copy_options::overwrite_existing);
  WriteTextFile(fs::path(layout.host_package_root) / "AndroidManifest.xml",
                ReadFile(decode_root / "AndroidManifest.xml"));
  WriteTextFile(
      fs::path(layout.host_package_root) / "assessment.txt",
      RenderManifestAssessmentReport(assessment));

  std::ostringstream metadata_json;
  metadata_json << "{\n"
                << "  \"package_name\": \"" << EscapeJson(profile.package_name)
                << "\",\n"
                << "  \"version_name\": \"" << EscapeJson(metadata.version_name)
                << "\",\n"
                << "  \"version_code\": " << metadata.version_code << ",\n"
                << "  \"min_sdk\": " << metadata.min_sdk << ",\n"
                << "  \"target_sdk\": " << metadata.target_sdk << ",\n"
                << "  \"install_id\": \"" << EscapeJson(install_request.install_id)
                << "\",\n"
                << "  \"launcher_component\": \""
                << EscapeJson(profile.launcher_activity_name) << "\",\n"
                << "  \"earliest_load_phase\": \""
                << EscapeJson(assessment.earliest_load_phase) << "\",\n"
                << "  \"earliest_ui_phase\": \""
                << EscapeJson(assessment.earliest_ui_phase) << "\",\n"
                << "  \"earliest_full_use_phase\": \""
                << EscapeJson(assessment.earliest_full_use_phase) << "\"\n"
                << "}\n";
  WriteTextFile(layout.host_manifest_path, metadata_json.str());

  return LoadedApkReport{
      .apk_path = apk_path,
      .install_id = install_request.install_id,
      .metadata = metadata,
      .manifest_profile = profile,
      .assessment = assessment,
      .layout = layout,
      .install_root = layout.host_package_root,
  };
}

}  // namespace wfa
