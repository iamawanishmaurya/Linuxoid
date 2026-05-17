#include "wfa/apk_host_integration.hpp"
#include "wfa/apk_archive.hpp"
#include "wfa/art_activity_bootstrap_fixture.hpp"
#include "wfa/art_bootstrap_execution_fixture.hpp"
#include "wfa/art_classloader_fixture.hpp"
#include "wfa/art_class_resolution_fixture.hpp"
#include "wfa/art_runtime_smoke.hpp"
#include "wfa/apk_loader.hpp"
#include "wfa/asset_manager_stub.hpp"
#include "wfa/binder_service_manager.hpp"
#include "wfa/checkpoint.hpp"
#include "wfa/desktop_integration.hpp"
#include "wfa/egl_smoke_fixture.hpp"
#include "wfa/manifest_assessment.hpp"
#include "wfa/native_input_queue_fixture.hpp"
#include "wfa/native_lifecycle.hpp"
#include "wfa/native_spike.hpp"
#include "wfa/native_window_surface.hpp"
#include "wfa/package_layout.hpp"
#include "wfa/project_status.hpp"
#include "wfa/runtime_bridge.hpp"
#include "wfa/runtime_health.hpp"
#include "wfa/wayland_surface_fixture.hpp"
#include "wfa/waydroid_integration.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::string ReadCommandOutput(const std::string& command, int* exit_code) {
  const std::string wrapped = command + " 2>&1";
  FILE* pipe = popen(wrapped.c_str(), "r");
  if (pipe == nullptr) {
    throw std::runtime_error("unable to open pipe for command: " + command);
  }

  std::string output;
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }

  const int status = pclose(pipe);
  if (exit_code != nullptr) {
    *exit_code = status;
  }
  return output;
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to open file: " + path.string());
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

class ScopedEnvironmentVariable {
 public:
  ScopedEnvironmentVariable(const std::string& name,
                            const std::string& value)
      : name_(name) {
    const char* existing = std::getenv(name.c_str());
    if (existing != nullptr) {
      had_original_value_ = true;
      original_value_ = existing;
    }
    setenv(name_.c_str(), value.c_str(), 1);
  }

  ~ScopedEnvironmentVariable() {
    if (had_original_value_) {
      setenv(name_.c_str(), original_value_.c_str(), 1);
    } else {
      unsetenv(name_.c_str());
    }
  }

 private:
  std::string name_;
  std::string original_value_;
  bool had_original_value_ = false;
};

std::filesystem::path ResolveBuildDirFromTestBinary() {
  namespace fs = std::filesystem;
  std::error_code error;
  const fs::path self = fs::read_symlink("/proc/self/exe", error);
  if (error || self.empty()) {
    throw std::runtime_error("unable to resolve test binary path");
  }
  return self.parent_path();
}

std::uint32_t ComputeCrc32(const std::string& contents);
void WriteLe16(std::ofstream& output, std::uint16_t value);
void WriteLe32(std::ofstream& output, std::uint32_t value);
void WriteStoredZipFixture(
    const std::filesystem::path& zip_path,
    const std::vector<std::pair<std::string, std::string>>& entries);

std::string BuildStubDexPayload() {
  std::string payload(112, '\0');
  payload[0] = 'd';
  payload[1] = 'e';
  payload[2] = 'x';
  payload[3] = '\n';
  payload[4] = '0';
  payload[5] = '3';
  payload[6] = '5';
  payload[7] = '\0';

  auto write_le32 = [&](std::size_t offset, std::uint32_t value) {
    payload[offset + 0] = static_cast<char>(value & 0xFFu);
    payload[offset + 1] = static_cast<char>((value >> 8) & 0xFFu);
    payload[offset + 2] = static_cast<char>((value >> 16) & 0xFFu);
    payload[offset + 3] = static_cast<char>((value >> 24) & 0xFFu);
  };

  write_le32(32, static_cast<std::uint32_t>(payload.size()));
  write_le32(36, 0x70u);
  write_le32(40, 0x12345678u);
  write_le32(56, 0u);
  write_le32(60, 0u);
  write_le32(88, 0u);
  write_le32(92, 0u);
  write_le32(104, 0u);
  write_le32(108, 0u);
  return payload;
}

std::string EncodeUleb128(std::uint32_t value) {
  std::string encoded;
  do {
    unsigned char byte = static_cast<unsigned char>(value & 0x7Fu);
    value >>= 7u;
    if (value != 0) {
      byte |= 0x80u;
    }
    encoded.push_back(static_cast<char>(byte));
  } while (value != 0);
  return encoded;
}

std::string BuildResolvableDexPayload(
    const std::vector<std::string>& class_descriptors) {
  std::vector<std::string> strings = class_descriptors;
  std::sort(strings.begin(), strings.end());
  strings.erase(std::unique(strings.begin(), strings.end()), strings.end());

  const std::uint32_t string_ids_size =
      static_cast<std::uint32_t>(strings.size());
  const std::uint32_t type_ids_size = string_ids_size;
  const std::uint32_t class_defs_size = string_ids_size;

  const std::uint32_t header_size = 0x70u;
  const std::uint32_t string_ids_off = header_size;
  const std::uint32_t type_ids_off = string_ids_off + string_ids_size * 4u;
  const std::uint32_t class_defs_off = type_ids_off + type_ids_size * 4u;
  const std::uint32_t data_off = class_defs_off + class_defs_size * 32u;

  std::string payload(data_off, '\0');

  auto write_le32 = [&](std::size_t offset, std::uint32_t value) {
    payload[offset + 0] = static_cast<char>(value & 0xFFu);
    payload[offset + 1] = static_cast<char>((value >> 8) & 0xFFu);
    payload[offset + 2] = static_cast<char>((value >> 16) & 0xFFu);
    payload[offset + 3] = static_cast<char>((value >> 24) & 0xFFu);
  };

  payload[0] = 'd';
  payload[1] = 'e';
  payload[2] = 'x';
  payload[3] = '\n';
  payload[4] = '0';
  payload[5] = '3';
  payload[6] = '5';
  payload[7] = '\0';

  std::uint32_t cursor = data_off;
  for (std::size_t index = 0; index < strings.size(); ++index) {
    write_le32(string_ids_off + index * 4u, cursor);
    payload += EncodeUleb128(
        static_cast<std::uint32_t>(strings[index].size()));
    payload += strings[index];
    payload.push_back('\0');
    cursor = static_cast<std::uint32_t>(payload.size());
  }

  for (std::size_t index = 0; index < strings.size(); ++index) {
    write_le32(type_ids_off + index * 4u,
               static_cast<std::uint32_t>(index));
    write_le32(class_defs_off + index * 32u,
               static_cast<std::uint32_t>(index));
  }

  write_le32(32, static_cast<std::uint32_t>(payload.size()));
  write_le32(36, header_size);
  write_le32(40, 0x12345678u);
  write_le32(56, string_ids_size);
  write_le32(60, string_ids_off);
  write_le32(64, type_ids_size);
  write_le32(68, type_ids_off);
  write_le32(96, class_defs_size);
  write_le32(100, class_defs_off);
  write_le32(104, static_cast<std::uint32_t>(payload.size() - data_off));
  write_le32(108, data_off);
  return payload;
}

struct RuntimeHealthBootstrapFixture {
  std::filesystem::path root;
  wfa::NativeActivityBootstrap bootstrap;
};

struct NativeRuntimePackageFixture {
  std::filesystem::path root;
  std::filesystem::path compat_root;
  std::string package_name;
  std::string install_id;
  std::string launcher_component;
  std::string install_root;
  std::string apk_path;
};

NativeRuntimePackageFixture CreateNativeRuntimePackageFixture(
    const std::string& fixture_name, bool include_classes_dex = true,
    bool include_input_method_service = false) {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / fixture_name;
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path compat_root = root / "compat";

  const auto layout = wfa::BuildPackageLayout(
      {.package_name = "com.example.nativebridge",
       .install_id = "vc9-2.0.0",
       .version_code = 9},
      compat_root.string());
  fs::create_directories(layout.host_package_root);
  fs::create_directories(fs::path(layout.host_package_root) / "assets" /
                         "config");
  fs::create_directories(fs::path(layout.host_package_root) / "res" / "raw");

  std::vector<std::pair<std::string, std::string>> archive_entries = {
      {"AndroidManifest.xml",
       include_input_method_service
           ? R"(<manifest package="com.example.nativebridge">
  <uses-sdk android:minSdkVersion="24" android:targetSdkVersion="35"/>
  <application android:name="com.example.nativebridge.App">
    <activity android:name="com.example.nativebridge.MainActivity"/>
    <service android:name="com.example.nativebridge.ImeService" android:permission="android.permission.BIND_INPUT_METHOD"/>
  </application>
</manifest>
)"
           : R"(<manifest package="com.example.nativebridge">
  <uses-sdk android:minSdkVersion="24" android:targetSdkVersion="35"/>
  <application android:name="com.example.nativebridge.App">
    <activity android:name="com.example.nativebridge.MainActivity"/>
  </application>
</manifest>
)"},
      {"assets/config/hello.txt", "hello native runtime bridge\n"},
      {"resources.arsc", "arsc"},
  };
  if (include_classes_dex) {
    archive_entries.push_back(
        {"classes.dex",
         BuildResolvableDexPayload({"Lcom/example/nativebridge/App;",
                                    "Lcom/example/nativebridge/MainActivity;"})});
  }
  WriteStoredZipFixture(fs::path(layout.host_package_root) / "base.apk",
                        archive_entries);

  {
    std::ofstream manifest(fs::path(layout.host_package_root) /
                           "AndroidManifest.xml");
    manifest << (include_input_method_service
                     ? R"(<manifest package="com.example.nativebridge">
  <application android:name="com.example.nativebridge.App">
    <activity android:name="com.example.nativebridge.MainActivity"/>
    <service android:name="com.example.nativebridge.ImeService" android:permission="android.permission.BIND_INPUT_METHOD"/>
  </application>
</manifest>
)"
                     : R"(<manifest package="com.example.nativebridge">
  <application android:name="com.example.nativebridge.App">
    <activity android:name="com.example.nativebridge.MainActivity"/>
  </application>
</manifest>
)");
  }
  {
    std::ofstream assessment(fs::path(layout.host_package_root) /
                             "assessment.txt");
    assessment << "native runtime package fixture\n";
  }
  {
    std::ofstream asset(fs::path(layout.host_package_root) / "assets" /
                        "config" / "hello.txt");
    asset << "hello native runtime bridge\n";
  }
  {
    std::ofstream metadata(fs::path(layout.host_package_root) / "manifest.json");
    metadata << "{\n"
             << "  \"package_name\": \"com.example.nativebridge\",\n"
             << "  \"version_name\": \"2.0.0\",\n"
             << "  \"version_code\": 9,\n"
             << "  \"min_sdk\": 24,\n"
             << "  \"target_sdk\": 35,\n"
             << "  \"install_id\": \"vc9-2.0.0\",\n"
             << "  \"launcher_component\": "
                "\"com.example.nativebridge/.MainActivity\",\n"
             << "  \"earliest_load_phase\": \"P4\",\n"
             << "  \"earliest_ui_phase\": \"P6\",\n"
             << "  \"earliest_full_use_phase\": \"P6\"\n"
             << "}\n";
  }

  return {.root = root,
          .compat_root = compat_root,
          .package_name = "com.example.nativebridge",
          .install_id = "vc9-2.0.0",
          .launcher_component = "com.example.nativebridge/.MainActivity",
          .install_root = layout.host_package_root,
          .apk_path =
              (fs::path(layout.host_package_root) / "base.apk").string()};
}

RuntimeHealthBootstrapFixture CreateRuntimeHealthBootstrapFixture(
    const std::string& fixture_name, bool include_classes_dex,
    bool include_native_library,
    bool include_unsupported_native_library = false) {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / fixture_name;
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path compat_root = root / "compat";
  const fs::path native_root = root / "native";
  const fs::path compatctl_path = root / "compatctl";

  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }

  const auto layout = wfa::BuildPackageLayout(
      {.package_name = "com.example.runtimehealth",
       .install_id = "vc8-1.2.3",
       .version_code = 8},
      compat_root.string());
  fs::create_directories(layout.host_package_root);
  fs::create_directories(fs::path(layout.host_package_root) / "assets" /
                         "config");
  fs::create_directories(fs::path(layout.host_package_root) / "res" / "raw");
  if (include_native_library) {
    fs::create_directories(fs::path(layout.host_package_root) / "lib" /
                           "x86_64");
  }
  if (include_unsupported_native_library) {
    fs::create_directories(fs::path(layout.host_package_root) / "lib" /
                           "arm64-v8a");
  }

  std::vector<std::pair<std::string, std::string>> archive_entries = {
      {"AndroidManifest.xml",
       R"(<manifest package="com.example.runtimehealth">
  <uses-sdk android:minSdkVersion="24" android:targetSdkVersion="35"/>
  <application android:name="com.example.runtimehealth.App">
    <activity android:name="com.example.runtimehealth.MainActivity"/>
  </application>
</manifest>
)"},
      {"assets/config/hello.txt", "hello runtime health\n"},
      {"resources.arsc", "arsc"},
  };
  if (include_classes_dex) {
    archive_entries.push_back(
        {"classes.dex",
         BuildResolvableDexPayload({"Lcom/example/runtimehealth/App;",
                                    "Lcom/example/runtimehealth/MainActivity;"})});
  }
  WriteStoredZipFixture(fs::path(layout.host_package_root) / "base.apk",
                        archive_entries);

  {
    std::ofstream manifest(fs::path(layout.host_package_root) /
                           "AndroidManifest.xml");
    manifest << R"(<manifest package="com.example.runtimehealth">
  <application android:name="com.example.runtimehealth.App">
    <activity android:name="com.example.runtimehealth.MainActivity"/>
  </application>
</manifest>
)";
  }
  {
    std::ofstream assessment(fs::path(layout.host_package_root) /
                             "assessment.txt");
    assessment << "runtime health fixture\n";
  }
  {
    std::ofstream asset(fs::path(layout.host_package_root) / "assets" /
                        "config" / "hello.txt");
    asset << "hello runtime health\n";
  }
  {
    std::ofstream res(fs::path(layout.host_package_root) / "res" / "raw" /
                      "note.txt");
    res << "note\n";
  }
  if (include_native_library) {
    std::ofstream library(fs::path(layout.host_package_root) / "lib" / "x86_64" /
                          "libcalculator.so");
    library << "runtime health native lib\n";
  }
  if (include_unsupported_native_library) {
    std::ofstream library(fs::path(layout.host_package_root) / "lib" /
                              "arm64-v8a" / "libcalculator.so");
    library << "runtime health unsupported native lib\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = (fs::path(layout.host_package_root) / "base.apk").string(),
      .install_id = "vc8-1.2.3",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "runtimehealth.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 8,
          .version_name = "1.2.3",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.runtimehealth",
          .launcher_activity_name = "com.example.runtimehealth.MainActivity",
          .declared_components = {"com.example.runtimehealth.MainActivity"},
          .declared_activity_components = {"com.example.runtimehealth.MainActivity"},
          .has_launcher_activity = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.runtimehealth",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "P6",
          .has_launcher_activity = true,
      },
      .layout = layout,
      .install_root = layout.host_package_root,
  };

  const auto plan = wfa::BuildNativeLaunchPlan(report, native_root.string());
  return {.root = root,
          .bootstrap =
              wfa::BuildNativeActivityBootstrap(plan, compatctl_path.string())};
}

std::uint32_t ComputeCrc32(const std::string& contents) {
  std::uint32_t crc = 0xFFFFFFFFu;
  for (const unsigned char byte : contents) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      const bool carry = (crc & 1u) != 0;
      crc >>= 1u;
      if (carry) {
        crc ^= 0xEDB88320u;
      }
    }
  }
  return crc ^ 0xFFFFFFFFu;
}

void WriteLe16(std::ofstream& output, std::uint16_t value) {
  output.put(static_cast<char>(value & 0xFF));
  output.put(static_cast<char>((value >> 8) & 0xFF));
}

void WriteLe32(std::ofstream& output, std::uint32_t value) {
  output.put(static_cast<char>(value & 0xFF));
  output.put(static_cast<char>((value >> 8) & 0xFF));
  output.put(static_cast<char>((value >> 16) & 0xFF));
  output.put(static_cast<char>((value >> 24) & 0xFF));
}

void WriteStoredZipFixture(
    const std::filesystem::path& zip_path,
    const std::vector<std::pair<std::string, std::string>>& entries) {
  struct CentralDirectoryEntry {
    std::string path;
    std::uint32_t crc32 = 0;
    std::uint32_t size = 0;
    std::uint32_t local_header_offset = 0;
  };

  std::ofstream output(zip_path, std::ios::binary);
  if (!output) {
    throw std::runtime_error("unable to create zip fixture: " + zip_path.string());
  }

  std::vector<CentralDirectoryEntry> central_entries;
  for (const auto& [path, contents] : entries) {
    const std::uint32_t local_header_offset =
        static_cast<std::uint32_t>(output.tellp());
    const std::uint32_t crc32 = ComputeCrc32(contents);
    const std::uint32_t size = static_cast<std::uint32_t>(contents.size());

    WriteLe32(output, 0x04034B50u);
    WriteLe16(output, 20);
    WriteLe16(output, 0);
    WriteLe16(output, 0);
    WriteLe16(output, 0);
    WriteLe16(output, 0);
    WriteLe32(output, crc32);
    WriteLe32(output, size);
    WriteLe32(output, size);
    WriteLe16(output, static_cast<std::uint16_t>(path.size()));
    WriteLe16(output, 0);
    output.write(path.data(), static_cast<std::streamsize>(path.size()));
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));

    central_entries.push_back(CentralDirectoryEntry{
        .path = path,
        .crc32 = crc32,
        .size = size,
        .local_header_offset = local_header_offset,
    });
  }

  const std::uint32_t central_directory_offset =
      static_cast<std::uint32_t>(output.tellp());
  for (const auto& entry : central_entries) {
    WriteLe32(output, 0x02014B50u);
    WriteLe16(output, 20);
    WriteLe16(output, 20);
    WriteLe16(output, 0);
    WriteLe16(output, 0);
    WriteLe16(output, 0);
    WriteLe16(output, 0);
    WriteLe32(output, entry.crc32);
    WriteLe32(output, entry.size);
    WriteLe32(output, entry.size);
    WriteLe16(output, static_cast<std::uint16_t>(entry.path.size()));
    WriteLe16(output, 0);
    WriteLe16(output, 0);
    WriteLe16(output, 0);
    WriteLe16(output, 0);
    WriteLe32(output, 0);
    WriteLe32(output, entry.local_header_offset);
    output.write(entry.path.data(),
                 static_cast<std::streamsize>(entry.path.size()));
  }

  const std::uint32_t central_directory_size =
      static_cast<std::uint32_t>(output.tellp()) - central_directory_offset;
  WriteLe32(output, 0x06054B50u);
  WriteLe16(output, 0);
  WriteLe16(output, 0);
  WriteLe16(output, static_cast<std::uint16_t>(central_entries.size()));
  WriteLe16(output, static_cast<std::uint16_t>(central_entries.size()));
  WriteLe32(output, central_directory_size);
  WriteLe32(output, central_directory_offset);
  WriteLe16(output, 0);
}

void TestWeightedCheckpointProgress() {
  const auto checkpoints = wfa::BuildDefaultCheckpoints();

  Expect(checkpoints.size() == 5, "expected five runtime checkpoints");
  Expect(wfa::CalculateWeightedCheckpointProgress(checkpoints) == 70,
         "expected weighted checkpoint progress to round to 70");
  Expect(wfa::CountCompletedCheckpoints(checkpoints) == 2,
         "expected two completed runtime checkpoints");
}

void TestPhaseProgressAverage() {
  const auto phases = wfa::BuildDefaultPhases();

  Expect(phases.size() == 6, "expected six implementation phases");
  Expect(wfa::CalculateAveragePhaseProgress(phases) == 96,
         "expected average phase progress to equal 96");
}

void TestPackageLayoutBuildsExpectedPaths() {
  const wfa::PackageInstallRequest request{
      .package_name = "com.example.demo",
      .install_id = "alpha01",
      .version_code = 42,
  };

  const auto layout = wfa::BuildPackageLayout(request, "/var/lib/wfa");

  Expect(layout.guest_base_apk == "/data/app/com.example.demo/base.apk",
         "unexpected guest base APK path");
  Expect(layout.host_package_root ==
             "/var/lib/wfa/users/0/packages/com.example.demo/alpha01",
         "unexpected host package root");
  Expect(layout.host_manifest_path ==
             "/var/lib/wfa/users/0/packages/com.example.demo/alpha01/manifest.json",
         "unexpected manifest path");
  Expect(layout.guest_obb_root ==
             "/storage/emulated/0/Android/obb/com.example.demo",
         "unexpected guest OBB root");
}

void TestPackageValidationRejectsInvalidName() {
  const wfa::PackageInstallRequest request{
      .package_name = "invalid-package",
      .install_id = "alpha01",
      .version_code = 1,
  };

  bool threw = false;
  try {
    (void)wfa::BuildPackageLayout(request, "/var/lib/wfa");
  } catch (const std::invalid_argument&) {
    threw = true;
  }

  Expect(threw, "expected invalid package names to be rejected");
}

void TestStatusRenderingContainsLoadingBars() {
  const auto report = wfa::RenderProjectStatusReport();

  Expect(report.find("Scaffold Readiness") != std::string::npos,
         "expected scaffold readiness heading");
  Expect(report.find("Native Execution Readiness") != std::string::npos,
         "expected native execution readiness heading");
  Expect(report.find("96/100") != std::string::npos,
         "expected average phase progress in report");
  Expect(report.find("70/100") != std::string::npos,
         "expected weighted checkpoint progress in report");
}

void TestSimpleLauncherAssessment() {
  const std::string manifest = R"(
<manifest package="com.example.simple">
  <application>
    <activity android:name="com.example.simple.MainActivity">
      <intent-filter>
        <action android:name="android.intent.action.MAIN"/>
        <category android:name="android.intent.category.LAUNCHER"/>
      </intent-filter>
    </activity>
  </application>
</manifest>
)";

  const auto profile = wfa::ParseDecodedManifest(manifest);
  const auto assessment = wfa::AssessRuntimeRequirements(profile);

  Expect(profile.package_name == "com.example.simple",
         "expected simple package name");
  Expect(profile.has_launcher_activity,
         "expected launcher activity in simple manifest");
  Expect(!profile.has_input_method_service,
         "did not expect input method service in simple manifest");
  Expect(assessment.earliest_load_phase == "P4",
         "expected simple app load phase to be P4");
  Expect(assessment.earliest_ui_phase == "P6",
         "expected simple app UI phase to be P6");
  Expect(assessment.earliest_full_use_phase == "P6",
         "expected simple app full-use phase to be P6");
}

void TestKeyboardAssessmentRequiresPostP6ImeIntegration() {
  const std::string manifest = R"(
<manifest package="org.futo.inputmethod.latin">
  <uses-permission android:name="android.permission.RECEIVE_BOOT_COMPLETED"/>
  <uses-permission android:name="android.permission.RECORD_AUDIO"/>
  <application>
    <service android:name="org.futo.inputmethod.latin.LatinIME"
             android:permission="android.permission.BIND_INPUT_METHOD">
      <intent-filter>
        <action android:name="android.view.InputMethod"/>
      </intent-filter>
    </service>
    <activity android:name="org.futo.inputmethod.latin.uix.settings.SettingsActivity">
      <intent-filter>
        <action android:name="android.intent.action.MAIN"/>
        <category android:name="android.intent.category.LAUNCHER"/>
      </intent-filter>
    </activity>
    <receiver android:name="org.futo.inputmethod.latin.SystemBroadcastReceiver">
      <intent-filter>
        <action android:name="android.intent.action.BOOT_COMPLETED"/>
      </intent-filter>
    </receiver>
  </application>
</manifest>
)";

  const auto profile = wfa::ParseDecodedManifest(manifest);
  const auto assessment = wfa::AssessRuntimeRequirements(profile);
  const auto report = wfa::RenderManifestAssessmentReport(assessment);

  Expect(profile.package_name == "org.futo.inputmethod.latin",
         "expected keyboard package name");
  Expect(profile.has_input_method_service,
         "expected keyboard input method service");
  Expect(profile.requests_record_audio,
         "expected keyboard audio permission");
  Expect(assessment.earliest_ui_phase == "P6",
         "expected keyboard settings UI phase to be P6");
  Expect(assessment.earliest_full_use_phase == "POST_P6_IME",
         "expected full IME use to require post-P6 integration");
  Expect(report.find("Input method service: yes") != std::string::npos,
         "expected keyboard report to mention IME service");
}

void TestActivityAliasLauncherAssessment() {
  const std::string manifest = R"(
<manifest package="com.example.alias">
  <application>
    <activity android:name="com.example.alias.HomeActivity"/>
    <activity-alias android:name="com.example.alias.LauncherAlias"
                    android:targetActivity="com.example.alias.HomeActivity">
      <intent-filter>
        <action android:name="android.intent.action.MAIN"/>
        <category android:name="android.intent.category.LAUNCHER"/>
      </intent-filter>
    </activity-alias>
  </application>
</manifest>
)";

  const auto profile = wfa::ParseDecodedManifest(manifest);
  const auto assessment = wfa::AssessRuntimeRequirements(profile);

  Expect(profile.has_launcher_activity,
         "expected launcher activity through activity-alias");
  Expect(profile.launcher_activity_name == "com.example.alias.LauncherAlias",
         "expected alias name to be used for launcher component");
  Expect(assessment.earliest_ui_phase == "P6",
         "expected alias launcher UI phase to remain P6");
}

void TestDisabledLauncherCandidateFallsThroughToEnabledActivity() {
  const std::string manifest = R"(
<manifest package="org.fdroid.fdroid">
  <application>
    <activity android:name="org.fdroid.fdroid.panic.CalculatorActivity"
              android:enabled="false">
      <intent-filter>
        <action android:name="android.intent.action.MAIN"/>
        <category android:name="android.intent.category.LAUNCHER"/>
      </intent-filter>
    </activity>
    <activity android:name="org.fdroid.fdroid.views.main.MainActivity">
      <intent-filter>
        <action android:name="android.intent.action.MAIN"/>
        <category android:name="android.intent.category.LAUNCHER"/>
      </intent-filter>
    </activity>
  </application>
</manifest>
)";

  const auto profile = wfa::ParseDecodedManifest(manifest);
  const auto assessment = wfa::AssessRuntimeRequirements(profile);

  Expect(profile.has_launcher_activity,
         "expected enabled launcher activity to be found");
  Expect(profile.launcher_activity_name ==
             "org.fdroid.fdroid.views.main.MainActivity",
         "expected disabled launcher candidate to be skipped");
  Expect(assessment.earliest_ui_phase == "P6",
         "expected enabled launcher UI phase to remain P6");
}

void TestAdvancedRuntimeBlockersPushFullUsePastP6() {
  const std::string manifest = R"(
<manifest package="com.example.advanced">
  <uses-permission android:name="android.permission.RECEIVE_BOOT_COMPLETED"/>
  <application>
    <activity android:name="com.example.advanced.MainActivity">
      <intent-filter>
        <action android:name="android.intent.action.MAIN"/>
        <category android:name="android.intent.category.LAUNCHER"/>
      </intent-filter>
    </activity>
    <receiver android:name="com.example.advanced.BootReceiver">
      <intent-filter>
        <action android:name="android.intent.action.BOOT_COMPLETED"/>
      </intent-filter>
    </receiver>
    <service android:name="com.example.advanced.SyncService"
             android:process=":sync"/>
  </application>
</manifest>
)";

  const auto profile = wfa::ParseDecodedManifest(manifest);
  const auto assessment = wfa::AssessRuntimeRequirements(profile);
  const auto report = wfa::RenderManifestAssessmentReport(assessment);

  Expect(profile.uses_secondary_processes,
         "expected advanced manifest to use secondary processes");
  Expect(assessment.earliest_full_use_phase == "POST_P6_ADVANCED_RUNTIME",
         "expected advanced runtime blockers to push full use past P6");
  Expect(report.find("Secondary process declarations require multi-process runtime support.") !=
             std::string::npos,
         "expected advanced report to mention multi-process blocker");
}

void TestInvalidManifestRejected() {
  bool threw = false;
  try {
    (void)wfa::ParseDecodedManifest("<manifest><application/></manifest>");
  } catch (const std::invalid_argument&) {
    threw = true;
  }

  Expect(threw, "expected missing package manifests to be rejected");
}

void TestApktoolMetadataParsing() {
  const std::string yaml = R"(
apkFileName: keyboard-0.1.28.apk
sdkInfo:
  minSdkVersion: 24
  targetSdkVersion: 35
versionInfo:
  versionCode: 11654
  versionName: 0.1.28
)";

  const auto metadata = wfa::ParseApktoolMetadata(yaml);

  Expect(metadata.apk_file_name == "keyboard-0.1.28.apk",
         "expected apk file name");
  Expect(metadata.min_sdk == 24, "expected min sdk");
  Expect(metadata.target_sdk == 35, "expected target sdk");
  Expect(metadata.version_code == 11654, "expected version code");
  Expect(metadata.version_name == "0.1.28", "expected version name");
  Expect(wfa::BuildInstallId(metadata) == "vc11654-0.1.28",
         "expected install id");

  const auto sanitized = wfa::BuildInstallId(wfa::ApktoolMetadata{
      .apk_file_name = "demo.apk",
      .min_sdk = 24,
      .target_sdk = 35,
      .version_code = 7,
      .version_name = "1.0 beta+1",
  });
  Expect(sanitized == "vc7-1.0_beta_1",
         "expected version name sanitization in install id");
}

void TestLoadedApkReportRendering() {
  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/demo.apk",
      .install_id = "vc42-1.0.0",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "demo.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 42,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.demo",
          .launcher_activity_name = "com.example.demo.MainActivity",
          .has_launcher_activity = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.demo",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "P6",
      },
      .layout = wfa::BuildPackageLayout(
          {.package_name = "com.example.demo", .install_id = "vc42-1.0.0", .version_code = 42},
          "/tmp/wfa"),
      .install_root = "/tmp/wfa/users/0/packages/com.example.demo/vc42-1.0.0",
  };

  const auto rendered = wfa::RenderLoadedApkReport(report);
  Expect(rendered.find("Package: com.example.demo") != std::string::npos,
         "expected loaded apk report package");
  Expect(rendered.find("Install ID: vc42-1.0.0") != std::string::npos,
         "expected loaded apk report install id");
}

void TestNativeSpikeAssessmentAcceptsSimpleForegroundApp() {
  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/simple.apk",
      .install_id = "vc7-1.0.0",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "simple.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 7,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.simple",
          .launcher_activity_name = "com.example.simple.MainActivity",
          .declared_components = {"com.example.simple.MainActivity"},
          .declared_activity_components = {"com.example.simple.MainActivity"},
          .has_launcher_activity = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.simple",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "P6",
          .has_launcher_activity = true,
      },
      .layout = wfa::BuildPackageLayout(
          {.package_name = "com.example.simple",
           .install_id = "vc7-1.0.0",
           .version_code = 7},
          "/tmp/linuxoid-native-test"),
      .install_root = "/tmp/linuxoid-native-test/users/0/packages/com.example.simple/vc7-1.0.0",
  };

  const auto assessment = wfa::AssessNativeSpikeCandidate(report);
  Expect(assessment.native_spike_candidate,
         "expected simple foreground app to be a native spike candidate");
  Expect(assessment.launcher_component == "com.example.simple/.MainActivity",
         "expected normalized launcher component");
  Expect(assessment.blockers.empty(),
         "expected no blockers for simple foreground candidate");
}

void TestNativeSpikeAssessmentAcceptsResolvedLauncherWithMultipleActivities() {
  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/multi.apk",
      .install_id = "vc9-1.0.0",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "multi.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 9,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.multi",
          .launcher_activity_name = "com.example.multi.MainActivity",
          .declared_components = {"com.example.multi.MainActivity",
                                  "com.example.multi.SettingsActivity"},
          .declared_activity_components = {"com.example.multi.MainActivity",
                                           "com.example.multi.SettingsActivity"},
          .has_launcher_activity = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.multi",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "P6",
          .has_launcher_activity = true,
      },
      .layout = wfa::BuildPackageLayout(
          {.package_name = "com.example.multi",
           .install_id = "vc9-1.0.0",
           .version_code = 9},
          "/tmp/linuxoid-native-test"),
      .install_root = "/tmp/linuxoid-native-test/users/0/packages/com.example.multi/vc9-1.0.0",
  };

  const auto assessment = wfa::AssessNativeSpikeCandidate(report);
  Expect(assessment.native_spike_candidate,
         "expected launcher-resolved multi-activity app to remain eligible");
  Expect(assessment.launcher_component == "com.example.multi/.MainActivity",
         "expected normalized launcher component for multi-activity app");
  Expect(assessment.blockers.empty(),
         "expected no blockers for multi-activity foreground candidate");
}

void TestNativeSpikeAssessmentRejectsAdvancedRuntimeApp() {
  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/advanced.apk",
      .install_id = "vc8-1.0.0",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "advanced.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 8,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.advanced",
          .launcher_activity_name = "com.example.advanced.MainActivity",
          .declared_components = {"com.example.advanced.MainActivity",
                                  "com.example.advanced.SyncService"},
          .declared_activity_components = {"com.example.advanced.MainActivity"},
          .has_launcher_activity = true,
          .has_background_service = true,
          .uses_secondary_processes = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.advanced",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "POST_P6_ADVANCED_RUNTIME",
          .has_launcher_activity = true,
          .uses_secondary_processes = true,
          .blockers = {"Secondary process declarations require multi-process runtime support."},
      },
      .layout = wfa::BuildPackageLayout(
          {.package_name = "com.example.advanced",
           .install_id = "vc8-1.0.0",
           .version_code = 8},
          "/tmp/linuxoid-native-test"),
      .install_root = "/tmp/linuxoid-native-test/users/0/packages/com.example.advanced/vc8-1.0.0",
  };

  const auto assessment = wfa::AssessNativeSpikeCandidate(report);
  Expect(!assessment.native_spike_candidate,
         "expected advanced runtime app to be rejected for native spike");
  Expect(!assessment.blockers.empty(),
         "expected blockers for advanced runtime app");
  bool saw_runtime_blocker = false;
  for (const auto& blocker : assessment.blockers) {
    if (blocker.find("advanced runtime") != std::string::npos ||
        blocker.find("Secondary process") != std::string::npos) {
      saw_runtime_blocker = true;
    }
  }
  Expect(saw_runtime_blocker,
         "expected advanced runtime blocker to be surfaced");
}

void TestNativeLaunchPlanBuildsBundleLayout() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "linuxoid-native-plan-test";
  const fs::path compat_root = root / "compat";
  const fs::path native_root = root / "native";
  fs::remove_all(root);

  const auto layout = wfa::BuildPackageLayout(
      {.package_name = "com.example.simple",
       .install_id = "vc7-1.0.0",
       .version_code = 7},
      compat_root.string());
  fs::create_directories(layout.host_package_root);
  {
    std::ofstream base_apk(fs::path(layout.host_package_root) / "base.apk");
    base_apk << "apk bytes\n";
  }
  {
    std::ofstream manifest(fs::path(layout.host_package_root) /
                           "AndroidManifest.xml");
    manifest << "<manifest package=\"com.example.simple\"/>\n";
  }
  {
    std::ofstream assessment(fs::path(layout.host_package_root) /
                             "assessment.txt");
    assessment << "simple candidate\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/simple.apk",
      .install_id = "vc7-1.0.0",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "simple.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 7,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.simple",
          .launcher_activity_name = "com.example.simple.MainActivity",
          .declared_components = {"com.example.simple.MainActivity"},
          .declared_activity_components = {"com.example.simple.MainActivity"},
          .has_launcher_activity = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.simple",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "P6",
          .has_launcher_activity = true,
      },
      .layout = layout,
      .install_root = layout.host_package_root,
  };

  const auto plan = wfa::BuildNativeLaunchPlan(report, native_root.string());
  Expect(plan.plan_written, "expected native launch plan to be written");
  Expect(plan.assessment.native_spike_candidate,
         "expected native candidate to remain eligible");
  Expect(fs::exists(plan.bundle_apk_path),
         "expected bundled apk copy to exist");
  Expect(fs::exists(plan.bootstrap_spec_path),
         "expected native bootstrap spec to exist");
  Expect(fs::exists(plan.manifest_copy_path),
         "expected native manifest copy to exist");

  std::ifstream spec_input(plan.bootstrap_spec_path);
  std::string spec((std::istreambuf_iterator<char>(spec_input)),
                   std::istreambuf_iterator<char>());
  Expect(spec.find("\"native_spike_candidate\": true") != std::string::npos,
         "expected candidate flag in native spec");
  Expect(spec.find("com.example.simple/.MainActivity") != std::string::npos,
         "expected launcher component in native spec");

  const auto rendered = wfa::RenderNativeLaunchPlanReport(plan);
  Expect(rendered.find("Native Spike Candidate: yes") != std::string::npos,
         "expected native spike candidate line");
  Expect(rendered.find("Bootstrap Spec: ") != std::string::npos,
         "expected bootstrap spec line");

  fs::remove_all(root);
}

void TestNativeActivityBootstrapWritesArtifacts() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "linuxoid-native-bootstrap-test";
  fs::remove_all(root);
  const fs::path compat_root = root / "compat";
  const fs::path native_root = root / "native";
  const fs::path compatctl_path = root / "compatctl";
  fs::create_directories(root);

  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }

  const auto layout = wfa::BuildPackageLayout(
      {.package_name = "com.example.simple",
       .install_id = "vc7-1.0.0",
       .version_code = 7},
      compat_root.string());
  fs::create_directories(layout.host_package_root);

  {
    std::ofstream apk(layout.host_package_root + "/base.apk");
    apk << "apk payload\n";
  }
  {
    std::ofstream manifest(layout.host_package_root + "/AndroidManifest.xml");
    manifest << "<manifest package=\"com.example.simple\"/>\n";
  }
  {
    std::ofstream assessment(layout.host_package_root + "/assessment.txt");
    assessment << "simple candidate\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/simple.apk",
      .install_id = "vc7-1.0.0",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "simple.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 7,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.simple",
          .launcher_activity_name = "com.example.simple.MainActivity",
          .declared_components = {"com.example.simple.MainActivity"},
          .declared_activity_components = {"com.example.simple.MainActivity"},
          .has_launcher_activity = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.simple",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "P6",
          .has_launcher_activity = true,
      },
      .layout = layout,
      .install_root = layout.host_package_root,
  };

  const auto plan = wfa::BuildNativeLaunchPlan(report, native_root.string());
  const auto bootstrap =
      wfa::BuildNativeActivityBootstrap(plan, compatctl_path.string());

  Expect(bootstrap.bootstrap_ready,
         "expected native activity bootstrap to be ready");
  Expect(!bootstrap.execution_engine_ready,
         "expected native execution engine to remain pending");
  Expect(fs::exists(bootstrap.bootstrap_manifest_path),
         "expected native bootstrap manifest");
  Expect(fs::exists(bootstrap.env_script_path),
         "expected native bootstrap env script");
  Expect(fs::exists(bootstrap.entrypoint_script_path),
         "expected native bootstrap entrypoint script");
  Expect(fs::exists(bootstrap.report_path),
         "expected native bootstrap report");

  std::ifstream manifest_input(bootstrap.bootstrap_manifest_path);
  std::string manifest((std::istreambuf_iterator<char>(manifest_input)),
                       std::istreambuf_iterator<char>());
  Expect(manifest.find("\"launcher_component\": \"com.example.simple/.MainActivity\"") !=
             std::string::npos,
         "expected launcher component in native bootstrap manifest");
  Expect(manifest.find("\"execution_engine_ready\": false") !=
             std::string::npos,
         "expected execution readiness flag in native bootstrap manifest");

  std::ifstream env_input(bootstrap.env_script_path);
  std::string env_script((std::istreambuf_iterator<char>(env_input)),
                         std::istreambuf_iterator<char>());
  Expect(env_script.find("LINUXOID_PACKAGE_NAME='com.example.simple'") !=
             std::string::npos,
         "expected package export in native env script");

  std::ifstream entrypoint_input(bootstrap.entrypoint_script_path);
  std::string entrypoint((std::istreambuf_iterator<char>(entrypoint_input)),
                         std::istreambuf_iterator<char>());
  Expect(entrypoint.find("native-execute-stub") != std::string::npos,
         "expected native execute stub in entrypoint script");
  Expect(entrypoint.find(plan.assessment.package_name) != std::string::npos,
         "expected package name in entrypoint script");
  Expect(entrypoint.find(bootstrap.bootstrap_manifest_path) !=
             std::string::npos,
         "expected bootstrap manifest path in entrypoint script");

  const auto rendered = wfa::RenderNativeActivityBootstrapReport(bootstrap);
  Expect(rendered.find("Bootstrap Ready: yes") != std::string::npos,
         "expected bootstrap readiness line");
  Expect(rendered.find("Execution Engine Ready: no") != std::string::npos,
         "expected execution readiness line");

  fs::remove_all(root);
}

void TestNativeLaunchPlanStagesHostAbiLibrariesAndAssets() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-native-stage-assets-test";
  fs::remove_all(root);
  const fs::path compat_root = root / "compat";
  const fs::path native_root = root / "native";

  const auto layout = wfa::BuildPackageLayout(
      {.package_name = "com.example.nativeapp",
       .install_id = "vc9-1.2.3",
       .version_code = 9},
      compat_root.string());
  fs::create_directories(layout.host_package_root);
  fs::create_directories(fs::path(layout.host_package_root) / "lib" / "x86_64");
  fs::create_directories(fs::path(layout.host_package_root) / "lib" /
                         "arm64-v8a");
  fs::create_directories(fs::path(layout.host_package_root) / "assets" /
                         "config");
  fs::create_directories(fs::path(layout.host_package_root) / "res" / "raw");

  {
    std::ofstream apk(fs::path(layout.host_package_root) / "base.apk");
    apk << "apk payload\n";
  }
  {
    std::ofstream manifest(fs::path(layout.host_package_root) /
                           "AndroidManifest.xml");
    manifest << "<manifest package=\"com.example.nativeapp\"/>\n";
  }
  {
    std::ofstream assessment(fs::path(layout.host_package_root) /
                             "assessment.txt");
    assessment << "native candidate\n";
  }
  {
    std::ofstream lib(fs::path(layout.host_package_root) / "lib" / "x86_64" /
                      "libcalculator.so");
    lib << "x86_64 payload\n";
  }
  {
    std::ofstream lib(fs::path(layout.host_package_root) / "lib" / "arm64-v8a" /
                      "libcalculator.so");
    lib << "arm64 payload\n";
  }
  {
    std::ofstream asset(fs::path(layout.host_package_root) / "assets" /
                        "config" / "hello.txt");
    asset << "hello from asset\n";
  }
  {
    std::ofstream resource(fs::path(layout.host_package_root) / "res" / "raw" /
                           "note.txt");
    resource << "raw resource\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/nativeapp.apk",
      .install_id = "vc9-1.2.3",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "nativeapp.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 9,
          .version_name = "1.2.3",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.nativeapp",
          .launcher_activity_name = "com.example.nativeapp.MainActivity",
          .declared_components = {"com.example.nativeapp.MainActivity"},
          .declared_activity_components = {"com.example.nativeapp.MainActivity"},
          .has_launcher_activity = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.nativeapp",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "P6",
          .has_launcher_activity = true,
      },
      .layout = layout,
      .install_root = layout.host_package_root,
  };

  const auto plan = wfa::BuildNativeLaunchPlan(report, native_root.string());

  Expect(plan.selected_abi == "x86_64", "expected x86_64 ABI selection");
  Expect(plan.host_abi_supported,
         "expected plan to mark host ABI support as available");
  Expect(plan.native_libraries_declared,
         "expected native library declaration to be tracked");
  Expect(plan.discovered_native_library_count == 2,
         "expected declared native library count across supported and unsupported ABIs");
  Expect(plan.staged_native_libraries.size() == 1,
         "expected one staged native library");
  Expect(fs::exists(fs::path(plan.library_root) / "libcalculator.so"),
         "expected staged x86_64 native library in bundle lib root");
  Expect(plan.unsupported_native_libraries.size() == 1,
         "expected one unsupported native library record");
  Expect(plan.asset_root == (fs::path(plan.resource_root) / "assets").string(),
         "expected deterministic asset root inside resource root");
  Expect(fs::exists(fs::path(plan.asset_root) / "config" / "hello.txt"),
         "expected staged asset file");
  Expect(fs::exists(fs::path(plan.resource_root) / "res" / "raw" / "note.txt"),
         "expected staged resource directory");

  std::ifstream spec_input(plan.bootstrap_spec_path);
  std::string spec((std::istreambuf_iterator<char>(spec_input)),
                   std::istreambuf_iterator<char>());
  Expect(spec.find("\"selected_abi\": \"x86_64\"") != std::string::npos,
         "expected selected abi in native plan spec");
  Expect(spec.find("\"native_libraries_declared\": true") !=
             std::string::npos,
         "expected native library declaration flag in spec");
  Expect(spec.find("\"discovered_native_library_count\": 2") !=
             std::string::npos,
         "expected discovered native library count in spec");
  Expect(spec.find("\"staged_native_libraries\": [") != std::string::npos,
         "expected staged native libraries array in spec");
  Expect(spec.find("\"unsupported_native_libraries\": [") !=
             std::string::npos,
         "expected unsupported native libraries array in spec");

  fs::remove_all(root);
}

void TestNativeLaunchPlanReportsUnsupportedAbiClearly() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-native-unsupported-abi-test";
  fs::remove_all(root);
  const fs::path compat_root = root / "compat";
  const fs::path native_root = root / "native";

  const auto layout = wfa::BuildPackageLayout(
      {.package_name = "com.example.armonly",
       .install_id = "vc5-2.0.0",
       .version_code = 5},
      compat_root.string());
  fs::create_directories(layout.host_package_root);
  fs::create_directories(fs::path(layout.host_package_root) / "lib" /
                         "arm64-v8a");

  {
    std::ofstream apk(fs::path(layout.host_package_root) / "base.apk");
    apk << "apk payload\n";
  }
  {
    std::ofstream manifest(fs::path(layout.host_package_root) /
                           "AndroidManifest.xml");
    manifest << "<manifest package=\"com.example.armonly\"/>\n";
  }
  {
    std::ofstream assessment(fs::path(layout.host_package_root) /
                             "assessment.txt");
    assessment << "native candidate\n";
  }
  {
    std::ofstream lib(fs::path(layout.host_package_root) / "lib" / "arm64-v8a" /
                      "libarmonly.so");
    lib << "arm64 payload\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/armonly.apk",
      .install_id = "vc5-2.0.0",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "armonly.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 5,
          .version_name = "2.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.armonly",
          .launcher_activity_name = "com.example.armonly.MainActivity",
          .declared_components = {"com.example.armonly.MainActivity"},
          .declared_activity_components = {"com.example.armonly.MainActivity"},
          .has_launcher_activity = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.armonly",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "P6",
          .has_launcher_activity = true,
      },
      .layout = layout,
      .install_root = layout.host_package_root,
  };

  const auto plan = wfa::BuildNativeLaunchPlan(report, native_root.string());

  Expect(plan.selected_abi.empty(),
         "expected no selected ABI for unsupported-native-only bundle");
  Expect(!plan.host_abi_supported,
         "expected unsupported ABI to remain unavailable");
  Expect(plan.native_libraries_declared,
         "expected unsupported-native-only bundle to declare native libraries");
  Expect(plan.discovered_native_library_count == 1,
         "expected one discovered unsupported native library");
  Expect(plan.staged_native_libraries.empty(),
         "expected no staged native libraries for unsupported ABI");
  Expect(plan.unsupported_native_libraries.size() == 1,
         "expected unsupported library to be reported");

  const auto rendered = wfa::RenderNativeLaunchPlanReport(plan);
  Expect(rendered.find("Selected ABI: unsupported") != std::string::npos,
         "expected unsupported abi report line");
  Expect(rendered.find("Unsupported Native Libraries: 1") !=
             std::string::npos,
         "expected unsupported native library count in report");

  fs::remove_all(root);
}

void TestAssetManagerReadsFixtureAsset() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "linuxoid-asset-read-test";
  fs::remove_all(root);
  const fs::path resource_root = root / "resources";
  const fs::path asset_root = resource_root / "assets";
  fs::create_directories(asset_root / "config");

  {
    std::ofstream asset(asset_root / "config" / "hello.txt");
    asset << "hello asset bridge\n";
  }

  wfa::AAssetManager* manager =
      wfa::MakeStubAssetManager("/tmp/base.apk", resource_root.string());
  const auto asset = wfa::ReadStubAsset(manager, "config/hello.txt");

  Expect(asset.found, "expected asset read to succeed");
  Expect(asset.contents == "hello asset bridge\n",
         "expected asset contents to round-trip");
  Expect(asset.resolved_path ==
             (asset_root / "config" / "hello.txt").string(),
         "expected resolved asset path");

  fs::remove_all(root);
}

void TestAssetManagerListsZipAssetsAndBlocksTraversal() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-asset-zip-list-test";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path apk_path = root / "fixture.apk";
  WriteStoredZipFixture(
      apk_path,
      {{"AndroidManifest.xml",
        "<manifest package=\"com.example.fixture\"/>\n"},
       {"assets/config/hello.txt", "hello zip asset\n"},
       {"assets/images/logo.txt", "zip logo\n"}});

  wfa::AAssetManager* manager =
      wfa::MakeStubAssetManager(apk_path.string(), "");
  const auto listed_assets = wfa::ListStubAssets(manager);
  Expect(listed_assets.size() == 2, "expected two zip-backed assets");
  Expect(listed_assets[0] == "config/hello.txt",
         "expected normalized first asset path");
  Expect(listed_assets[1] == "images/logo.txt",
         "expected normalized second asset path");

  const auto asset = wfa::ReadStubAsset(manager, "assets/config/hello.txt");
  Expect(asset.found, "expected zip-backed asset read to succeed");
  Expect(asset.contents == "hello zip asset\n",
         "expected zip-backed asset contents");

  const auto rejected = wfa::ReadStubAsset(manager, "../secret.txt");
  Expect(!rejected.found, "expected traversal read to fail");
  Expect(rejected.failure_reason == "asset_path_traversal_rejected",
         "expected deterministic traversal rejection");

  fs::remove_all(root);
}

void TestApkResourceReadinessReadsManifestAndAssetsFromZipFixture() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-apk-resource-readiness-test";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path apk_path = root / "fixture.apk";
  WriteStoredZipFixture(
      apk_path,
      {{"AndroidManifest.xml",
        R"(<manifest package="com.example.fixture">
  <uses-sdk android:minSdkVersion="24" android:targetSdkVersion="34"/>
  <application android:name="com.example.fixture.App">
    <activity android:name="com.example.fixture.MainActivity"/>
    <activity android:name="com.example.fixture.SettingsActivity"/>
  </application>
</manifest>
)"},
       {"assets/config/hello.txt", "hello zip asset\n"},
       {"assets/images/logo.txt", "zip logo\n"},
       {"resources.arsc", "arsc"}});

  const auto report = wfa::InspectApkResourceReadiness(apk_path.string());
  Expect(report.manifest.manifest_present, "expected manifest presence");
  Expect(report.manifest.manifest_ready, "expected manifest readiness");
  Expect(report.manifest.manifest_source == "archive_plain_xml",
         "expected plain archive manifest source");
  Expect(report.manifest.package_name == "com.example.fixture",
         "expected package name from archive manifest");
  Expect(report.manifest.min_sdk == 24, "expected min sdk from manifest");
  Expect(report.manifest.target_sdk == 34,
         "expected target sdk from manifest");
  Expect(report.manifest.application_name == "com.example.fixture.App",
         "expected application name");
  Expect(report.manifest.activity_names.size() == 2,
         "expected activity names from manifest");
  Expect(report.asset_listing_ready, "expected asset listing readiness");
  Expect(report.asset_read_ready, "expected asset read readiness");
  Expect(report.asset_source == "archive_entries",
         "expected archive asset source");
  Expect(report.asset_paths.size() == 2, "expected two asset paths");
  Expect(report.asset_paths[0] == "config/hello.txt",
         "expected sorted asset path");
  Expect(report.resources_table_present,
         "expected resources table presence from archive entry");
  Expect(report.errors.empty(), "expected no readiness errors");

  const auto rendered = wfa::RenderApkResourceReadinessJson(report);
  Expect(rendered.find("\"manifest_source\": \"archive_plain_xml\"") !=
             std::string::npos,
         "expected manifest source in JSON");
  Expect(rendered.find("\"asset_root_path\": \"zip:" + apk_path.string() +
                           "!/assets\"") != std::string::npos,
         "expected asset root path in JSON");
  Expect(rendered.find("\"activity_names\": [\"com.example.fixture.MainActivity\", "
                       "\"com.example.fixture.SettingsActivity\"]") !=
             std::string::npos,
         "expected stable activity ordering in JSON");

  fs::remove_all(root);
}

void TestApkResourceReadinessHandlesMissingManifest() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-apk-resource-missing-manifest";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path apk_path = root / "missing-manifest.apk";
  WriteStoredZipFixture(apk_path,
                        {{"assets/config/hello.txt", "hello zip asset\n"}});

  const auto report = wfa::InspectApkResourceReadiness(apk_path.string());
  Expect(!report.manifest.manifest_present,
         "expected missing manifest to be reported");
  Expect(!report.manifest.manifest_ready,
         "expected missing manifest to remain unready");
  Expect(std::find(report.errors.begin(), report.errors.end(),
                   "manifest_missing") != report.errors.end(),
         "expected manifest_missing error");

  const auto rendered = wfa::RenderApkResourceReadinessJson(report);
  Expect(rendered.find("\"manifest_ready\": false") != std::string::npos,
         "expected manifest readiness false in JSON");

  fs::remove_all(root);
}

void TestOpenedApkArchiveReadsEntriesDeterministically() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-opened-apk-archive";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path apk_path = root / "fixture.apk";
  WriteStoredZipFixture(
      apk_path,
      {{"AndroidManifest.xml",
        R"(<manifest package="com.example.archive"/>)"},
       {"assets/config/hello.txt", "hello cached archive\n"}});

  const auto archive = wfa::OpenApkArchive(apk_path.string());
  const auto& entries = wfa::ListApkArchiveEntries(archive);
  Expect(entries.size() == 2, "expected two archive entries");
  Expect(entries[0].path == "AndroidManifest.xml",
         "expected sorted archive manifest entry");
  Expect(entries[1].path == "assets/config/hello.txt",
         "expected sorted archive asset entry");

  const auto asset = wfa::ReadApkArchiveEntry(archive, "assets/config/hello.txt");
  Expect(asset.readable, "expected archive asset read to succeed");
  Expect(asset.contents == "hello cached archive\n",
         "expected archive asset contents");

  fs::remove_all(root);
}

void TestApkResourceReadinessUsesStagedManifestFallback() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() /
      "linuxoid-apk-resource-staged-manifest-fallback";
  const fs::path resource_root = root / "resources";
  const fs::path bundle_root = root / "bundle";
  fs::remove_all(root);
  fs::create_directories(resource_root / "assets");
  fs::create_directories(bundle_root);
  const fs::path apk_path = bundle_root / "base.apk";
  WriteStoredZipFixture(apk_path,
                        {{"assets/config/hello.txt", "hello staged\n"},
                         {"resources.arsc", "arsc"}});
  {
    std::ofstream manifest(bundle_root / "AndroidManifest.xml");
    manifest << R"(<manifest package="com.example.staged">
  <uses-sdk android:minSdkVersion="26" android:targetSdkVersion="35"/>
  <application android:name="com.example.staged.App">
    <activity android:name="com.example.staged.MainActivity"/>
  </application>
</manifest>
)";
  }

  const auto report = wfa::InspectApkResourceReadiness(
      apk_path.string(), resource_root.string());
  Expect(report.manifest.manifest_present,
         "expected staged manifest to count as present");
  Expect(report.manifest.manifest_ready,
         "expected staged manifest fallback readiness");
  Expect(report.manifest.manifest_source == "staged_bundle_manifest",
         "expected staged bundle manifest source");
  Expect(report.manifest.package_name == "com.example.staged",
         "expected package name from staged manifest");
  Expect(report.manifest.activity_names.size() == 1,
         "expected staged manifest activity names");
  Expect(std::find(report.errors.begin(), report.errors.end(),
                   "manifest_missing") == report.errors.end(),
         "expected no manifest_missing error when staged manifest exists");

  fs::remove_all(root);
}

void TestInspectApkResourcesCommandWritesStableJson() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-inspect-apk-resources-command";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path apk_path = root / "fixture.apk";
  WriteStoredZipFixture(
      apk_path,
      {{"AndroidManifest.xml",
        R"(<manifest package="com.example.command">
  <application android:name="com.example.command.App">
    <activity android:name="com.example.command.MainActivity"/>
  </application>
</manifest>
)"},
       {"assets/config/hello.txt", "hello zip asset\n"}});

  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";
  int exit_code = 0;
  const std::string output = ReadCommandOutput(
      compatctl.string() + " inspect-apk-resources " + apk_path.string(),
      &exit_code);
  Expect(exit_code == 0, "expected inspect-apk-resources command success");
  Expect(output.find("\"package_name\": \"com.example.command\"") !=
             std::string::npos,
         "expected package name in command JSON");
  Expect(output.find("\"asset_paths\": [\"config/hello.txt\"]") !=
             std::string::npos,
         "expected stable asset path array in command JSON");

  fs::remove_all(root);
}

void TestHeadlessNativeWindowSurfaceTracksMetadataAndLifecycle() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-native-window-metadata-test";
  fs::remove_all(root);

  const wfa::NativeWindowMetadata metadata{
      .width = 64,
      .height = 48,
      .format = wfa::kNativeWindowFormatRgba8888,
      .stride = 64,
  };
  wfa::ANativeWindow* window =
      wfa::CreateHeadlessNativeWindowSurface(metadata, root.string());

  const auto observed = wfa::InspectNativeWindow(window);
  Expect(observed.width == 64, "expected native window width");
  Expect(observed.height == 48, "expected native window height");
  Expect(observed.format == wfa::kNativeWindowFormatRgba8888,
         "expected native window format");
  Expect(observed.stride == 64, "expected native window stride");
  Expect(wfa::NativeWindowLifecycleReady(window),
         "expected headless native window lifecycle to be ready");

  wfa::DestroyHeadlessNativeWindowSurface(window);
  fs::remove_all(root);
}

void TestHeadlessFirstPixelFixtureWritesDeterministicMarker() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-first-pixel-fixture-test";
  fs::remove_all(root);

  const auto report = wfa::RunHeadlessFirstPixelFixture(
      root.string(),
      {.width = 8,
       .height = 6,
       .format = wfa::kNativeWindowFormatRgba8888,
       .stride = 8},
      0xff336699u);

  Expect(report.surface_ready, "expected first-pixel surface readiness");
  Expect(report.render_ready, "expected first-pixel render readiness");
  Expect(report.surface.activity_window_attached,
         "expected activity window attachment");
  Expect(report.surface.first_pixel_observed,
         "expected first-pixel observation");
  Expect(report.surface.first_pixel_value == 0xff336699u,
         "expected stable first-pixel value");
  Expect(fs::exists(report.surface.marker_path),
         "expected first-pixel marker path");

  std::ifstream marker_input(report.surface.marker_path);
  std::string marker((std::istreambuf_iterator<char>(marker_input)),
                     std::istreambuf_iterator<char>());
  Expect(marker.find("0xff336699") != std::string::npos,
         "expected first-pixel marker contents");

  const auto rendered = wfa::RenderFirstPixelFixtureJson(report);
  Expect(rendered.find("\"render_ready\": true") != std::string::npos,
         "expected render-ready json flag");
  Expect(rendered.find("\"backend_name\": \"wayland-egl-headless-fixture\"") !=
             std::string::npos,
         "expected backend name in fixture json");

  fs::remove_all(root);
}

void TestHeadlessNativeWindowCallbackFixtureWritesJournal() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-native-window-callback-test";
  fs::remove_all(root);

  const auto report = wfa::RunHeadlessNativeWindowCallbackFixture(
      root.string(),
      {.width = 12,
       .height = 9,
       .format = wfa::kNativeWindowFormatRgba8888,
       .stride = 12});

  Expect(report.surface_ready, "expected callback-fixture surface readiness");
  Expect(report.callbacks_ready,
         "expected callback fixture to dispatch lifecycle callbacks");
  Expect(report.events.size() == 3,
         "expected created, changed, and destroyed callbacks");
  Expect(report.events[0].event_name == "window_created",
         "expected created callback first");
  Expect(report.events[1].event_name == "window_changed",
         "expected changed callback second");
  Expect(report.events[2].event_name == "window_destroyed",
         "expected destroyed callback third");
  Expect(report.events[0].window_present,
         "expected created callback to receive a window");
  Expect(report.events[1].metadata.width == 12,
         "expected changed callback width metadata");
  Expect(fs::exists(report.callback_journal_path),
         "expected callback journal artifact");

  std::ifstream journal_input(report.callback_journal_path);
  std::string journal((std::istreambuf_iterator<char>(journal_input)),
                      std::istreambuf_iterator<char>());
  Expect(journal.find("\"event_name\": \"window_created\"") !=
             std::string::npos,
         "expected callback journal created event");
  Expect(journal.find("\"event_name\": \"window_destroyed\"") !=
             std::string::npos,
         "expected callback journal destroyed event");

  const auto rendered = wfa::RenderNativeWindowCallbackFixtureJson(report);
  Expect(rendered.find("\"callbacks_ready\": true") != std::string::npos,
         "expected callbacks-ready json flag");
  Expect(rendered.find("\"callback_journal_path\":") != std::string::npos,
         "expected callback journal path in fixture json");

  fs::remove_all(root);
}

void TestWaylandSurfaceFixtureWritesDeterministicMetadata() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-wayland-surface-fixture-test";
  fs::remove_all(root);

  const auto report = wfa::RunWaylandSurfaceFixture(
      root.string(),
      {.width = 96,
       .height = 72,
       .format = wfa::kNativeWindowFormatRgba8888,
       .stride = 96});

  Expect(report.width == 96, "expected wayland fixture width");
  Expect(report.height == 72, "expected wayland fixture height");
  Expect(report.artifact_root == root.string(),
         "expected deterministic wayland artifact root");
  Expect(report.surface_metadata_path ==
             (root / "surface-metadata.json").string(),
         "expected deterministic wayland metadata path");
  Expect(fs::exists(report.surface_metadata_path),
         "expected wayland metadata artifact");

  const auto rendered = wfa::RenderWaylandSurfaceFixtureJson(report);
  Expect(rendered.find("\"artifact_root\": \"" + root.string() + "\"") !=
             std::string::npos,
         "expected wayland artifact root in json");
  Expect(rendered.find("\"surface_metadata_path\": \"" +
                           report.surface_metadata_path + "\"") !=
             std::string::npos,
         "expected wayland metadata path in json");

  fs::remove_all(root);
}

void TestWaylandSurfaceFixtureReportsAvailabilityHonestly() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-wayland-availability-test";
  fs::remove_all(root);

  const auto report = wfa::RunWaylandSurfaceFixture(
      root.string(),
      {.width = 64,
       .height = 48,
       .format = wfa::kNativeWindowFormatRgba8888,
       .stride = 64});

  if (!wfa::WaylandClientSupportCompiled()) {
    Expect(!report.wayland_available,
           "expected unavailable wayland when support is not compiled");
    Expect(!report.surface_created,
           "expected no surface when support is not compiled");
    Expect(report.exit_reason == "wayland_client_unavailable",
           "expected unavailable build fallback reason");
  } else if (!report.wayland_available) {
    Expect(!report.surface_created,
           "expected no surface when runtime wayland is unavailable");
    Expect(report.exit_reason == "wayland_display_unavailable" ||
               report.exit_reason == "wayland_compositor_unavailable",
           "expected honest runtime wayland fallback reason");
  } else {
    Expect(report.surface_created,
           "expected surface creation when wayland is available");
    Expect(report.exit_reason == "wayland_surface_created",
           "expected created surface reason");
  }

  fs::remove_all(root);
}

void TestEglSmokeFixtureWritesDeterministicMetadata() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-egl-smoke-fixture-test";
  fs::remove_all(root);

  const auto report = wfa::RunEglSmokeFixture(
      root.string(),
      {.width = 80,
       .height = 60,
       .format = wfa::kNativeWindowFormatRgba8888,
       .stride = 80});

  Expect(report.width == 80, "expected egl fixture width");
  Expect(report.height == 60, "expected egl fixture height");
  Expect(report.artifact_root == root.string(),
         "expected deterministic egl artifact root");
  Expect(report.egl_metadata_path == (root / "egl-metadata.json").string(),
         "expected deterministic egl metadata path");
  Expect(fs::exists(report.egl_metadata_path),
         "expected egl metadata artifact");

  const auto rendered = wfa::RenderEglSmokeFixtureJson(report);
  Expect(rendered.find("\"artifact_root\": \"" + root.string() + "\"") !=
             std::string::npos,
         "expected egl artifact root in json");
  Expect(rendered.find("\"egl_metadata_path\": \"" +
                           report.egl_metadata_path + "\"") !=
             std::string::npos,
         "expected egl metadata path in json");

  fs::remove_all(root);
}

void TestEglSmokeFixtureReportsAvailabilityHonestly() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-egl-availability-test";
  fs::remove_all(root);

  const auto report = wfa::RunEglSmokeFixture(
      root.string(),
      {.width = 64,
       .height = 48,
       .format = wfa::kNativeWindowFormatRgba8888,
       .stride = 64});

  if (!wfa::EglSupportCompiled()) {
    Expect(!report.egl_available,
           "expected unavailable egl when support is not compiled");
    Expect(!report.context_created,
           "expected no context when support is not compiled");
    Expect(report.exit_reason == "egl_unavailable",
           "expected unavailable build fallback reason");
  } else if (!report.egl_available) {
    Expect(!report.context_created,
           "expected no context when runtime egl is unavailable");
    Expect(report.exit_reason == "egl_display_unavailable" ||
               report.exit_reason == "egl_initialize_failed" ||
               report.exit_reason == "egl_no_config_found" ||
               report.exit_reason == "egl_context_creation_failed" ||
               report.exit_reason == "egl_pbuffer_creation_failed",
           "expected honest runtime egl fallback reason");
  } else {
    Expect(report.display_initialized,
           "expected initialized display when egl is available");
    Expect(report.config_chosen,
           "expected config selection when egl is available");
    Expect(report.context_created,
           "expected context creation when egl is available");
    Expect(report.pbuffer_created,
           "expected pbuffer creation when egl is available");
    Expect(report.exit_reason == "egl_pbuffer_ready",
           "expected successful egl pbuffer reason");
  }

  fs::remove_all(root);
}

void TestNativeWindowBridgeFixtureWritesStableArtifacts() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-native-window-bridge-test";
  fs::remove_all(root);

  const auto report = wfa::RunNativeWindowBridgeFixture(
      root.string(),
      {.width = 40,
       .height = 30,
       .format = wfa::kNativeWindowFormatRgba8888,
       .stride = 40});

  Expect(report.native_window_bridge_ready,
         "expected native-window bridge readiness");
  Expect(report.width == 48, "expected deterministic bridge width update");
  Expect(report.height == 34, "expected deterministic bridge height update");
  Expect(report.format == wfa::kNativeWindowFormatRgba8888,
         "expected bridge format to remain rgba8888");
  Expect(report.stride == 48, "expected deterministic bridge stride update");
  Expect(report.geometry_updates == 1,
         "expected exactly one geometry update");
  Expect(report.artifact_root == root.string(),
         "expected deterministic bridge artifact root");
  Expect(report.metadata_path ==
             (root / "native-window-bridge-metadata.json").string(),
         "expected deterministic bridge metadata path");
  Expect(report.event_log_path ==
             (root / "native-window-bridge-events.jsonl").string(),
         "expected deterministic bridge event log path");
  Expect(fs::exists(report.metadata_path), "expected bridge metadata artifact");
  Expect(fs::exists(report.event_log_path), "expected bridge event log");

  std::ifstream event_input(report.event_log_path);
  std::string event_log((std::istreambuf_iterator<char>(event_input)),
                        std::istreambuf_iterator<char>());
  Expect(event_log.find("\"event_name\": \"bridge_created\"") !=
             std::string::npos,
         "expected bridge-created event");
  Expect(event_log.find("\"event_name\": \"geometry_updated\"") !=
             std::string::npos,
         "expected geometry-updated event");

  const auto rendered = wfa::RenderNativeWindowBridgeFixtureJson(report);
  Expect(rendered.find("\"native_window_bridge_ready\": true") !=
             std::string::npos,
         "expected bridge-ready json flag");
  Expect(rendered.find("\"metadata_path\": \"" + report.metadata_path + "\"") !=
             std::string::npos,
         "expected bridge metadata path in json");

  fs::remove_all(root);
}

void TestNativeWindowBridgeFixtureReportsFallbackHonestly() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-native-window-bridge-fallback-test";
  fs::remove_all(root);

  const auto report = wfa::RunNativeWindowBridgeFixture(
      root.string(),
      {.width = 32,
       .height = 24,
       .format = wfa::kNativeWindowFormatRgba8888,
       .stride = 32});

  Expect(report.native_window_bridge_ready,
         "expected bridge contract to stay testable");
  if (report.wayland_surface_created && report.egl_pbuffer_created) {
    Expect(report.backing_mode == "probe_only_wayland_egl_available",
           "expected probe-only mode when both backing probes succeed");
    Expect(report.exit_reason == "native_window_bridge_ready_probe_only",
           "expected probe-only bridge exit reason");
  } else {
    Expect(report.backing_mode == "headless_fallback",
           "expected honest fallback mode when real backing is unavailable");
    Expect(report.exit_reason == "native_window_bridge_ready_headless_fallback",
           "expected fallback bridge exit reason");
  }

  fs::remove_all(root);
}

void TestNativeInputQueueFixtureWritesStableArtifacts() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-native-input-queue-test";
  fs::remove_all(root);

  const auto report = wfa::RunNativeInputQueueFixture(
      root.string(),
      {.width = 48,
       .height = 32,
       .format = wfa::kNativeWindowFormatRgba8888,
       .stride = 48});

  Expect(report.input_queue_ready, "expected native input queue readiness");
  Expect(report.focus_owned, "expected focused native input queue");
  Expect(report.focus_owner == "linuxoid-native-window",
         "expected deterministic focus owner");
  Expect(report.width == 56, "expected deterministic input width update");
  Expect(report.height == 36, "expected deterministic input height update");
  Expect(report.format == wfa::kNativeWindowFormatRgba8888,
         "expected input format to remain rgba8888");
  Expect(report.stride == 56, "expected deterministic input stride update");
  Expect(report.pointer_events_injected == 3,
         "expected deterministic pointer event count");
  Expect(report.key_events_injected == 2,
         "expected deterministic key event count");
  Expect(report.artifact_root == root.string(),
         "expected deterministic input artifact root");
  Expect(report.metadata_path ==
             (root / "native-input-queue-metadata.json").string(),
         "expected deterministic input metadata path");
  Expect(report.event_log_path ==
             (root / "native-input-events.jsonl").string(),
         "expected deterministic input event log path");
  Expect(fs::exists(report.metadata_path), "expected input metadata artifact");
  Expect(fs::exists(report.event_log_path), "expected input event log");

  std::ifstream event_input(report.event_log_path);
  std::string event_log((std::istreambuf_iterator<char>(event_input)),
                        std::istreambuf_iterator<char>());
  Expect(event_log.find("\"event_name\": \"focus_acquired\"") !=
             std::string::npos,
         "expected focus-acquired event");
  Expect(event_log.find("\"event_name\": \"pointer_down\"") !=
             std::string::npos,
         "expected pointer-down event");
  Expect(event_log.find("\"event_name\": \"key_up\"") != std::string::npos,
         "expected key-up event");

  const auto rendered = wfa::RenderNativeInputQueueFixtureJson(report);
  Expect(rendered.find("\"input_queue_ready\": true") != std::string::npos,
         "expected input queue ready json flag");
  Expect(rendered.find("\"metadata_path\": \"" + report.metadata_path + "\"") !=
             std::string::npos,
         "expected input metadata path in json");

  fs::remove_all(root);
}

void TestNativeInputQueueFixtureReportsFallbackHonestly() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-native-input-queue-fallback-test";
  fs::remove_all(root);

  const auto report = wfa::RunNativeInputQueueFixture(
      root.string(),
      {.width = 36,
       .height = 24,
       .format = wfa::kNativeWindowFormatRgba8888,
       .stride = 36});

  Expect(report.input_queue_ready,
         "expected input queue contract to stay testable");
  if (report.backing_mode == "probe_only_wayland_egl_available") {
    Expect(report.exit_reason == "native_input_queue_ready_probe_only",
           "expected probe-only input queue exit reason");
  } else {
    Expect(report.backing_mode == "headless_fallback",
           "expected honest fallback mode for input queue");
    Expect(report.exit_reason == "native_input_queue_ready_headless_fallback",
           "expected fallback input queue exit reason");
  }

  fs::remove_all(root);
}

void TestBinderServiceManagerFixtureWritesStableArtifacts() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-binder-service-manager-test";
  fs::remove_all(root);

  const auto report = wfa::RunBinderServiceManagerFixture(
      {.package_name = "com.example.simple",
       .launcher_component = "com.example.simple/.MainActivity",
       .apk_path = "/tmp/simple.apk",
       .artifact_root = root.string()});

  Expect(report.manager_ready, "expected binder-shaped service manager ready");
  Expect(report.package_name == "com.example.simple",
         "expected deterministic binder package name");
  Expect(report.launcher_component == "com.example.simple/.MainActivity",
         "expected deterministic launcher component");
  Expect(report.transport_kind == "unix_socketpair_binder_shape",
         "expected deterministic binder transport kind");
  Expect(report.transport_log_path ==
             (root / "binder" / "transport-messages.jsonl").string(),
         "expected deterministic binder transport log path");
  Expect(report.metadata_path ==
             (root / "binder" / "service-manager.json").string(),
         "expected deterministic binder metadata path");
  Expect(report.registry_path ==
             (root / "binder" / "registered-services.json").string(),
         "expected deterministic binder registry path");
  Expect(report.lookup_log_path ==
             (root / "binder" / "service-lookups.jsonl").string(),
         "expected deterministic binder lookup log path");
  Expect(report.transaction_log_path ==
             (root / "binder" / "service-transactions.jsonl").string(),
         "expected deterministic binder transaction log path");
  Expect(fs::exists(report.metadata_path), "expected binder metadata artifact");
  Expect(fs::exists(report.registry_path), "expected binder registry artifact");
  Expect(fs::exists(report.lookup_log_path),
         "expected binder lookup log artifact");
  Expect(fs::exists(report.transaction_log_path),
         "expected binder transaction log artifact");
  Expect(fs::exists(report.transport_log_path),
         "expected binder transport log artifact");

  Expect(report.services.size() == 3,
         "expected three deterministic binder services");
  Expect(report.lookups.size() == 2,
         "expected two deterministic binder lookups");
  Expect(report.transactions.size() == 2,
         "expected two deterministic binder transactions");
  Expect(report.transport_round_trips == 4,
         "expected deterministic binder transport round trips");

  std::ifstream registry_input(report.registry_path);
  std::string registry((std::istreambuf_iterator<char>(registry_input)),
                       std::istreambuf_iterator<char>());
  Expect(registry.find("\"service_name\": \"package_manager\"") !=
             std::string::npos,
         "expected package manager registration");
  Expect(registry.find("\"service_name\": \"activity_manager\"") !=
             std::string::npos,
         "expected activity manager registration");

  std::ifstream transaction_input(report.transaction_log_path);
  std::string transactions(
      (std::istreambuf_iterator<char>(transaction_input)),
      std::istreambuf_iterator<char>());
  Expect(transactions.find("\"transaction_name\": \"getPackageInfo\"") !=
             std::string::npos,
         "expected package manager transaction");
  Expect(transactions.find(
             "\"transaction_name\": \"scheduleLaunchActivity\"") !=
             std::string::npos,
         "expected activity manager transaction");

  std::ifstream transport_input(report.transport_log_path);
  std::string transport((std::istreambuf_iterator<char>(transport_input)),
                        std::istreambuf_iterator<char>());
  Expect(transport.find("\"message_kind\": \"lookup_request\"") !=
             std::string::npos,
         "expected binder lookup request on transport");
  Expect(transport.find("\"message_kind\": \"transaction_response\"") !=
             std::string::npos,
         "expected binder transaction response on transport");

  const auto rendered = wfa::RenderBinderServiceManagerFixtureJson(report);
  Expect(rendered.find("\"manager_ready\": true") != std::string::npos,
         "expected binder manager ready json flag");
  Expect(rendered.find("\"metadata_path\": \"" + report.metadata_path + "\"") !=
             std::string::npos,
         "expected binder metadata path in json");
  Expect(rendered.find("\"transport_log_path\": \"" +
                           report.transport_log_path + "\"") !=
             std::string::npos,
         "expected binder transport log path in json");

  fs::remove_all(root);
}

void TestRuntimeHealthFixtureWritesStableArtifacts() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-test", true, true);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");

  Expect(report.self_healing_ready,
         "expected runtime health skeleton to be ready");
  Expect(!report.overall_ready,
         "expected dex/classloader gap to prevent false overall success");
  Expect(report.overall_state == "recovery_needed",
         "expected recovery-needed overall state");
  Expect(report.records.size() == 8, "expected eight subsystem health records");
  Expect(fs::exists(report.health_json_path), "expected health json artifact");
  Expect(fs::exists(report.trace_jsonl_path), "expected trace jsonl artifact");
  Expect(fs::exists(report.replay_json_path), "expected replay json artifact");

  const auto dex_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "dex_classloader_readiness";
      });
  Expect(dex_record != report.records.end(),
         "expected dex/classloader health record");
  Expect(dex_record->state == "pending",
         "expected dex/classloader to remain pending");
  Expect(!dex_record->ready, "expected pending dex/classloader readiness");
  Expect(dex_record->selected_recovery_action == "attempt_host_art_class_resolution",
         "expected deterministic dex recovery action");

  const auto activity_bootstrap_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "activity_bootstrap_readiness";
      });
  Expect(activity_bootstrap_record != report.records.end(),
         "expected activity bootstrap health record");
  Expect(activity_bootstrap_record->ready,
         "expected activity bootstrap planning seam to be ready");
  Expect(activity_bootstrap_record->selected_recovery_action.empty(),
         "expected no recovery action once activity bootstrap planning is ready");

  const auto bootstrap_execution_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "bootstrap_execution_readiness";
      });
  Expect(bootstrap_execution_record != report.records.end(),
         "expected bootstrap execution health record");
  Expect(bootstrap_execution_record->selected_recovery_action ==
             "attempt_host_bootstrap_execution",
         "expected deterministic bootstrap execution recovery action");

  const auto native_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "native_loading";
      });
  Expect(native_record != report.records.end() && native_record->ready,
         "expected native loading record to be ready with staged lib");

  const auto rendered = wfa::RenderRuntimeHealthReportJson(report);
  Expect(rendered.find("\"trace_jsonl_path\": \"" + report.trace_jsonl_path +
                           "\"") != std::string::npos,
         "expected trace path in runtime health json");

  fs::remove_all(fixture.root);
}

void TestNativeArtClassloaderFixtureWritesStableArtifacts() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-classloader-fixture", true, true);

  const auto report = wfa::RunNativeArtClassloaderFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(report.classpath_plan_ready,
         "expected deterministic ART classpath plan to be ready");
  Expect(report.dex_entries_present, "expected dex inventory to be present");
  Expect(report.manifest_targets_ready,
         "expected manifest target classes to be ready");
  Expect(fs::exists(report.dex_inventory_path),
         "expected dex inventory artifact");
  Expect(fs::exists(report.classloader_plan_path),
         "expected classloader plan artifact");
  Expect(fs::exists(report.trace_jsonl_path),
         "expected classloader trace artifact");
  Expect(report.target_class_names.size() == 2,
         "expected application and launcher target classes");
  Expect(report.target_class_names[0] == "com.example.runtimehealth.App",
         "expected normalized application class name");
  Expect(report.target_class_descriptors[1] ==
             "Lcom/example/runtimehealth/MainActivity;",
         "expected launcher descriptor");
  Expect(report.dex_entries.size() == 1, "expected one dex entry");
  Expect(report.dex_entries[0].valid_dex_magic,
         "expected stub dex magic to be recognized");
  Expect(report.dex_entries[0].dex_version == "035",
         "expected dex version 035");

  const auto rendered = wfa::RenderNativeArtClassloaderFixtureJson(report);
  Expect(rendered.find("\"classloader_plan_path\": \"" +
                           report.classloader_plan_path + "\"") !=
             std::string::npos,
         "expected classloader plan path in json");

  fs::remove_all(fixture.root);
}

void TestNativeArtClassloaderFixtureHandlesMissingDexHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-classloader-missing-dex", false, true);

  const auto report = wfa::RunNativeArtClassloaderFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(!report.dex_entries_present, "expected no dex entries");
  Expect(!report.classpath_plan_ready,
         "expected classpath plan to stay unready without dex entries");
  Expect(!report.pathclassloader_probe_ready,
         "expected no false classloader readiness without dex entries");
  Expect(report.exit_reason == "no_dex_entries_found",
         "expected missing-dex exit reason");

  fs::remove_all(fixture.root);
}

void TestNativeArtClassloaderCommandWritesStableJson() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-classloader-command", true, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int exit_code = 0;
  const std::string output = ReadCommandOutput(
      compatctl.string() + " native-art-classloader-fixture " +
          fixture.bootstrap.bootstrap_manifest_path,
      &exit_code);
  Expect(exit_code == 0, "expected native-art-classloader-fixture success");
  Expect(output.find("\"classpath_plan_ready\": true") !=
             std::string::npos,
         "expected classpath readiness in json");
  Expect(output.find("\"exit_reason\": ") != std::string::npos,
         "expected classloader exit reason in json");

  fs::remove_all(fixture.root);
}

void TestNativeArtRuntimeSmokeWritesStableArtifacts() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-runtime-smoke", true, true);

  const auto report = wfa::RunNativeArtRuntimeSmokeFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(report.classpath_plan_ready,
         "expected runtime smoke to reuse a ready classpath plan");
  Expect(report.pathclassloader_resolution_planned,
         "expected pathclassloader resolution plan flag");
  Expect(!report.pathclassloader_resolution_attempted,
         "expected no false class resolution attempt yet");
  Expect(fs::exists(report.invocation_plan_path),
         "expected invocation plan artifact");
  Expect(fs::exists(report.invocation_log_path),
         "expected invocation log artifact");
  Expect(fs::exists(report.result_json_path),
         "expected runtime smoke result artifact");
  Expect(report.target_class_names.size() == 2,
         "expected normalized target classes to carry forward");

  const auto rendered = wfa::RenderNativeArtRuntimeSmokeFixtureJson(report);
  Expect(rendered.find("\"invocation_plan_path\": \"" +
                           report.invocation_plan_path + "\"") !=
             std::string::npos,
         "expected invocation plan path in runtime smoke json");

  fs::remove_all(fixture.root);
}

void TestNativeArtRuntimeSmokeHandlesRuntimeAvailabilityHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-runtime-availability", true, true);

  const auto report = wfa::RunNativeArtRuntimeSmokeFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  if (!report.art_runtime_detected) {
    Expect(!report.runtime_probe_attempted,
           "expected no runtime probe when ART is absent");
    Expect(!report.pathclassloader_resolution_attempted,
           "expected no class resolution attempt when ART is absent");
    Expect(report.exit_reason == "art_runtime_not_detected",
           "expected absent-art exit reason");
  } else if (!report.safe_runtime_probe_available) {
    Expect(!report.runtime_probe_attempted,
           "expected no unsafe runtime probe attempt");
    Expect(!report.pathclassloader_resolution_attempted,
           "expected no class resolution attempt without safe probe");
    Expect(report.exit_reason ==
               "art_runtime_detected_without_safe_probe",
           "expected no-safe-probe exit reason");
  } else {
    Expect(report.runtime_probe_attempted,
           "expected runtime probe attempt when safe ART probe exists");
    Expect(report.runtime_exit_code >= 0,
           "expected concrete runtime probe exit code");
    Expect(report.pathclassloader_resolution_attempted,
           "expected class resolution attempt when safe ART probe exists");
    Expect(!report.resolved_target_class_name.empty(),
           "expected deterministic target class name for runtime attempt");
  }

  fs::remove_all(fixture.root);
}

void TestNativeArtRuntimeSmokeRecordsClassResolutionIntent() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-runtime-class-resolution-intent", true, true);

  const auto report = wfa::RunNativeArtRuntimeSmokeFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(report.pathclassloader_resolution_planned,
         "expected runtime smoke to keep class resolution planned");
  Expect(!report.resolved_target_class_name.empty(),
         "expected resolved target class name");
  Expect(!report.resolved_target_class_descriptor.empty(),
         "expected resolved target class descriptor");
  if (report.safe_runtime_probe_available) {
    Expect(report.runtime_probe_command.find(
               report.resolved_target_class_name) != std::string::npos,
           "expected runtime probe command to target resolved class");
  }

  fs::remove_all(fixture.root);
}

void TestNativeArtRuntimeSmokeCommandWritesStableJson() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-runtime-command", true, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int exit_code = 0;
  const std::string output = ReadCommandOutput(
      compatctl.string() + " native-art-runtime-smoke " +
          fixture.bootstrap.bootstrap_manifest_path,
      &exit_code);
  Expect(exit_code == 0, "expected native-art-runtime-smoke success");
  Expect(output.find("\"classpath_plan_ready\": true") !=
             std::string::npos,
         "expected classpath plan readiness in runtime smoke json");
  Expect(output.find("\"pathclassloader_resolution_planned\": true") !=
             std::string::npos,
         "expected pathclassloader plan flag in runtime smoke json");
  Expect(output.find("\"resolved_target_class_name\": ") !=
             std::string::npos,
         "expected resolved target class name in runtime smoke json");

  fs::remove_all(fixture.root);
}

void TestNativeArtRuntimeSmokeUsesFixtureRuntimeOverride() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-runtime-override", true, true);
  const fs::path runtime_probe = fixture.root / "linuxoid-art-runtime-probe";
  {
    std::ofstream output(runtime_probe);
    output << "#!/bin/sh\n";
    output << "printf '%s\\n' \"runtime-fixture:$*\"\n";
    output << "exit 0\n";
  }
  fs::permissions(runtime_probe,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);

  ScopedEnvironmentVariable runtime_override(
      "LINUXOID_ART_RUNTIME_PROBE_OVERRIDE", runtime_probe.string());
  const auto report = wfa::RunNativeArtRuntimeSmokeFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(report.art_runtime_detected,
         "expected runtime override to mark ART probe as detected");
  Expect(report.safe_runtime_probe_available,
         "expected runtime override probe to be treated as safe");
  Expect(report.runtime_probe_attempted,
         "expected runtime probe attempt through override");
  Expect(report.pathclassloader_resolution_attempted,
         "expected class-resolution attempt through override");
  Expect(report.runtime_probe_succeeded,
         "expected successful override runtime probe");
  Expect(report.runtime_class_resolution_succeeded,
         "expected successful override class resolution");
  Expect(report.runtime_exit_code == 0,
         "expected zero exit code from override runtime probe");
  Expect(report.art_runtime_probe == runtime_probe.string(),
         "expected runtime probe path to match override");
  Expect(report.art_runtime_probe_capability ==
             "override_bootstrap_capable",
         "expected override probe capability classification");
  Expect(ReadTextFile(report.invocation_log_path).find("runtime-fixture:") !=
             std::string::npos,
         "expected invocation log to capture override runtime output");

  fs::remove_all(fixture.root);
}

void TestNativeArtRuntimeSmokeClassifiesHostAppProcessAsDetectionOnly() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-runtime-app-process", true, true);
  const fs::path app_process = fixture.root / "app_process";
  {
    std::ofstream output(app_process);
    output << "#!/bin/sh\n";
    output << "printf '%s\\n' \"host-app-process:$*\"\n";
    output << "exit 0\n";
  }
  fs::permissions(app_process,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);

  ScopedEnvironmentVariable path_override("PATH", fixture.root.string());
  const auto report = wfa::RunNativeArtRuntimeSmokeFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(report.art_runtime_detected,
         "expected host app_process candidate to count as detected ART path");
  Expect(report.art_runtime_probe_source == "host",
         "expected host runtime probe source");
  Expect(report.art_runtime_probe_detection_reason ==
             "host_app_process_selected",
         "expected app_process detection reason");
  Expect(report.art_runtime_probe_capability ==
             "host_app_process_detection_only",
         "expected app_process capability classification");
  Expect(!report.safe_runtime_probe_available,
         "expected app_process host probe to stay non-bootstrap-capable");
  Expect(!report.runtime_probe_attempted,
         "expected no runtime probe attempt for detection-only app_process");
  Expect(report.art_runtime_probe.find("app_process") != std::string::npos,
         "expected selected probe path to mention app_process");
  const std::string inventory_json =
      ReadTextFile(report.art_runtime_probe_inventory_path);
  Expect(inventory_json.find("\"selected\": true") != std::string::npos,
         "expected selected app_process candidate in inventory");
  Expect(inventory_json.find("app_process") != std::string::npos,
         "expected app_process candidate in inventory json");

  fs::remove_all(fixture.root);
}

void TestNativeArtRuntimeSmokeRecordsProbeInventoryAndReason() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-runtime-probe-inventory", true, true);
  const fs::path runtime_probe = fixture.root / "linuxoid-art-runtime-probe";
  {
    std::ofstream output(runtime_probe);
    output << "#!/bin/sh\n";
    output << "printf '%s\\n' \"runtime-fixture:$*\"\n";
    output << "exit 0\n";
  }
  fs::permissions(runtime_probe,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);

  ScopedEnvironmentVariable runtime_override(
      "LINUXOID_ART_RUNTIME_PROBE_OVERRIDE", runtime_probe.string());
  const auto report = wfa::RunNativeArtRuntimeSmokeFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(!report.art_runtime_probe_inventory_path.empty(),
         "expected runtime smoke probe inventory artifact path");
  Expect(fs::exists(report.art_runtime_probe_inventory_path),
         "expected runtime smoke probe inventory artifact");
  Expect(report.art_runtime_probe_detection_reason ==
             "override_probe_selected",
         "expected override-backed probe selection reason");
  const std::string inventory_json =
      ReadTextFile(report.art_runtime_probe_inventory_path);
  Expect(inventory_json.find(runtime_probe.string()) != std::string::npos,
         "expected override probe path in inventory json");
  Expect(inventory_json.find("\"selected\": true") != std::string::npos,
         "expected selected probe marker in inventory json");
  const auto rendered = wfa::RenderNativeArtRuntimeSmokeFixtureJson(report);
  Expect(rendered.find("\"art_runtime_probe_inventory_path\": \"" +
                           report.art_runtime_probe_inventory_path + "\"") !=
             std::string::npos,
         "expected probe inventory path in runtime smoke json");
  Expect(rendered.find("\"art_runtime_probe_detection_reason\": "
                       "\"override_probe_selected\"") !=
             std::string::npos,
         "expected probe detection reason in runtime smoke json");

  fs::remove_all(fixture.root);
}

void TestNativeArtActivityBootstrapFixtureWritesStableArtifacts() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-activity-bootstrap", true, true);

  const auto report = wfa::RunNativeArtActivityBootstrapFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(report.manifest_targets_ready,
         "expected manifest targets ready for activity bootstrap");
  Expect(report.classpath_plan_ready,
         "expected classpath plan ready for activity bootstrap");
  Expect(report.offline_resolution_ready,
         "expected offline class resolution ready for activity bootstrap");
  Expect(report.runtime_bootstrap_planned,
         "expected runtime bootstrap plan to be written");
  Expect(fs::exists(report.activity_bootstrap_plan_path),
         "expected activity bootstrap plan artifact");
  Expect(fs::exists(report.trace_jsonl_path),
         "expected activity bootstrap trace artifact");
  Expect(fs::exists(report.result_json_path),
         "expected activity bootstrap result artifact");
  Expect(report.selected_activity_class_name ==
             "com.example.runtimehealth.MainActivity",
         "expected launcher-derived activity bootstrap class");
  Expect(report.selected_activity_class_descriptor ==
             "Lcom/example/runtimehealth/MainActivity;",
         "expected launcher-derived activity descriptor");
  Expect(report.selected_application_class_name ==
             "com.example.runtimehealth.App",
         "expected normalized application bootstrap class");
  Expect(report.selected_application_class_descriptor ==
             "Lcom/example/runtimehealth/App;",
         "expected normalized application bootstrap descriptor");
  const std::string rendered =
      wfa::RenderNativeArtActivityBootstrapFixtureJson(report);
  Expect(rendered.find("\"selected_application_class_name\": "
                       "\"com.example.runtimehealth.App\"") !=
             std::string::npos,
         "expected normalized application class in activity bootstrap json");
  Expect(rendered.find("\"selected_application_class_descriptor\": "
                       "\"Lcom/example/runtimehealth/App;\"") !=
             std::string::npos,
         "expected normalized application descriptor in activity bootstrap json");
  Expect(report.launcher_component ==
             "com.example.runtimehealth/.MainActivity",
         "expected launcher component in activity bootstrap report");

  Expect(rendered.find("\"activity_bootstrap_plan_path\": \"" +
                           report.activity_bootstrap_plan_path + "\"") !=
             std::string::npos,
         "expected bootstrap plan path in activity bootstrap json");

  fs::remove_all(fixture.root);
}

void TestNativeArtActivityBootstrapTraceCapturesApplicationBootstrapSequence() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-activity-bootstrap-trace", true, true);

  const auto report = wfa::RunNativeArtActivityBootstrapFixture(
      fixture.bootstrap.bootstrap_manifest_path);
  const std::string trace = ReadTextFile(report.trace_jsonl_path);
  Expect(trace.find("\"event_type\": \"application_bootstrap_") !=
             std::string::npos,
         "expected application bootstrap event in trace");
  Expect(trace.find("\"event_type\": \"launcher_activity_bootstrap_") !=
             std::string::npos,
         "expected launcher activity bootstrap event in trace");

  fs::remove_all(fixture.root);
}

void TestNativeArtActivityBootstrapFixtureHandlesRuntimeAvailabilityHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-activity-bootstrap-runtime", true, true);

  const auto report = wfa::RunNativeArtActivityBootstrapFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  if (!report.art_runtime_detected) {
    Expect(!report.application_probe_attempted,
           "expected no application bootstrap attempt when ART is absent");
    Expect(!report.runtime_bootstrap_attempted,
           "expected no activity bootstrap attempt when ART is absent");
    Expect(!report.runtime_bootstrap_succeeded,
           "expected no activity bootstrap success when ART is absent");
    Expect(report.dependency_blocked,
           "expected dependency-blocked state when ART is absent");
    Expect(report.exit_reason ==
               "activity_bootstrap_runtime_not_detected",
           "expected absent-art activity bootstrap exit reason");
  } else if (report.safe_runtime_probe_available) {
    Expect(!report.application_probe_attempted,
           "expected planning seam to defer application execution");
    Expect(!report.activity_probe_attempted,
           "expected planning seam to defer activity execution");
    Expect(report.runtime_bootstrap_planned,
           "expected planning seam to stay bootstrap-ready");
    Expect(!report.runtime_bootstrap_attempted,
           "expected planning seam to avoid executing bootstrap");
    Expect(report.exit_reason == "activity_bootstrap_execution_deferred",
           "expected explicit deferred execution exit reason");
  }

  fs::remove_all(fixture.root);
}

void TestNativeArtActivityBootstrapCommandWritesStableJson() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-activity-bootstrap-command", true, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int exit_code = 0;
  const std::string output = ReadCommandOutput(
      compatctl.string() + " native-art-activity-bootstrap-fixture " +
          fixture.bootstrap.bootstrap_manifest_path,
      &exit_code);
  Expect(exit_code == 0,
         "expected native-art-activity-bootstrap-fixture success");
  Expect(output.find("\"runtime_bootstrap_planned\": true") !=
             std::string::npos,
         "expected bootstrap planned flag in activity bootstrap json");
  Expect(output.find("\"selected_application_class_name\": ") !=
             std::string::npos,
         "expected selected application class in activity bootstrap json");
  Expect(output.find("\"selected_activity_class_name\": ") !=
             std::string::npos,
         "expected selected activity class in activity bootstrap json");

  fs::remove_all(fixture.root);
}

void TestNativeArtBootstrapExecutionFixtureWritesStableArtifacts() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-bootstrap-execution", true, true);

  const auto report = wfa::RunNativeArtBootstrapExecutionFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(report.execution_attempt_planned,
         "expected bootstrap execution attempt to be planned");
  Expect(fs::exists(report.execution_plan_path),
         "expected bootstrap execution plan artifact");
  Expect(fs::exists(report.trace_jsonl_path),
         "expected bootstrap execution trace artifact");
  Expect(fs::exists(report.result_json_path),
         "expected bootstrap execution result artifact");
  Expect(fs::exists(report.execution_context_json_path),
         "expected bootstrap execution context artifact");
  Expect(fs::exists(report.runner_script_path),
         "expected bootstrap execution runner script artifact");
  Expect(fs::exists(report.runner_state_json_path),
         "expected bootstrap execution runner state artifact");
  Expect(fs::exists(report.application_execution_log_path),
         "expected application execution log artifact");
  Expect(fs::exists(report.activity_execution_log_path),
         "expected activity execution log artifact");
  Expect(report.selected_activity_class_name ==
             "com.example.runtimehealth.MainActivity",
         "expected launcher-derived activity execution class");
  const std::string rendered =
      wfa::RenderNativeArtBootstrapExecutionFixtureJson(report);
  Expect(rendered.find("\"execution_attempt_planned\": true") !=
             std::string::npos,
         "expected execution planned flag in bootstrap execution json");
  Expect(rendered.find("\"runner_script_path\": \"") !=
             std::string::npos,
         "expected runner script path in bootstrap execution json");
  Expect(rendered.find("\"runner_state_json_path\": \"") !=
             std::string::npos,
         "expected runner state path in bootstrap execution json");
  Expect(rendered.find("\"application_execution_log_path\": \"") !=
             std::string::npos,
         "expected application execution log path in bootstrap execution json");
  Expect(rendered.find("\"activity_bootstrap_result_json_path\": \"") !=
             std::string::npos,
         "expected activity bootstrap result path in bootstrap execution json");

  fs::remove_all(fixture.root);
}

void TestNativeArtBootstrapExecutionFixtureCanReuseActivityBootstrapReport() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-bootstrap-execution-reuse", true, true);

  const auto activity_report = wfa::RunNativeArtActivityBootstrapFixture(
      fixture.bootstrap.bootstrap_manifest_path);
  const auto report = wfa::BuildNativeArtBootstrapExecutionFixture(
      activity_report);

  Expect(report.activity_bootstrap_result_json_path ==
             activity_report.result_json_path,
         "expected bootstrap execution fixture to reuse activity result path");
  Expect(fs::exists(report.result_json_path),
         "expected reused bootstrap execution result artifact");

  fs::remove_all(fixture.root);
}

void TestNativeArtBootstrapExecutionFixtureHandlesRuntimeAvailabilityHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-bootstrap-execution-runtime", true, true);

  const auto report = wfa::RunNativeArtBootstrapExecutionFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  if (!report.art_runtime_detected) {
    Expect(!report.execution_attempted,
           "expected no bootstrap execution attempt when ART is absent");
    Expect(!report.execution_succeeded,
           "expected no bootstrap execution success when ART is absent");
    Expect(report.exit_reason == "bootstrap_execution_runtime_not_detected",
           "expected absent-art bootstrap execution exit reason");
  }

  fs::remove_all(fixture.root);
}

void TestNativeArtBootstrapExecutionFixtureRunsThroughSupervisedRunner() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-art-bootstrap-execution-runner";
  fs::remove_all(root);
  fs::create_directories(root / "art");

  {
    std::ofstream bootstrap_manifest(root / "bootstrap.json");
    bootstrap_manifest << "{}\n";
  }
  {
    std::ofstream activity_result(root / "art" / "activity-bootstrap-result.json");
    activity_result << "{}\n";
  }

  wfa::NativeArtActivityBootstrapFixtureReport activity;
  activity.package_name = "com.example.runner";
  activity.install_id = "vc1-1.0";
  activity.bootstrap_manifest_path = (root / "bootstrap.json").string();
  activity.session_root = root.string();
  activity.artifact_root = (root / "art").string();
  activity.result_json_path =
      (root / "art" / "activity-bootstrap-result.json").string();
  activity.selected_application_class_name = "com.example.runner.App";
  activity.selected_application_class_descriptor =
      "Lcom/example/runner/App;";
  activity.selected_activity_class_name = "com.example.runner.MainActivity";
  activity.selected_activity_class_descriptor =
      "Lcom/example/runner/MainActivity;";
  activity.application_bootstrap_command = "printf 'application-ok\\n'";
  activity.activity_bootstrap_command = "printf 'activity-ok\\n'";
  activity.art_runtime_detected = true;
  activity.safe_runtime_probe_available = true;
  activity.runtime_class_resolution_succeeded = true;
  activity.runtime_bootstrap_planned = true;
  activity.dependency_blocked = false;
  activity.dependency_count = 0;

  const auto report =
      wfa::BuildNativeArtBootstrapExecutionFixture(activity);

  Expect(report.runner_invoked, "expected supervised runner invocation");
  Expect(report.runner_exit_code == 0, "expected zero runner exit code");
  Expect(report.application_exit_code == 0,
         "expected zero application phase exit code");
  Expect(report.activity_exit_code == 0,
         "expected zero activity phase exit code");
  Expect(report.execution_attempted,
         "expected execution attempt through supervised runner");
  Expect(report.execution_succeeded,
         "expected successful supervised runner execution");
  Expect(ReadTextFile(report.application_execution_log_path).find(
             "application-ok") != std::string::npos,
         "expected application log output");
  Expect(ReadTextFile(report.activity_execution_log_path).find("activity-ok") !=
             std::string::npos,
         "expected activity log output");
  Expect(ReadTextFile(report.runner_state_json_path).find(
             "\"runner_exit_code\": 0") != std::string::npos,
         "expected runner state json exit code");

  fs::remove_all(root);
}

void TestNativeArtBootstrapExecutionFixtureAttemptsRuntimeThroughOverride() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-bootstrap-execution-override", true, true);
  const fs::path runtime_probe = fixture.root / "linuxoid-art-runtime-probe";
  {
    std::ofstream output(runtime_probe);
    output << "#!/bin/sh\n";
    output << "case \"$*\" in\n";
    output << "  *linuxoid.bootstrap.mode=application*) printf '%s\\n' "
              "'application-runtime-ok'; exit 0 ;;\n";
    output << "  *linuxoid.bootstrap.mode=activity*) printf '%s\\n' "
              "'activity-runtime-ok'; exit 0 ;;\n";
    output << "  *) printf '%s\\n' 'runtime-fixture-ok'; exit 0 ;;\n";
    output << "esac\n";
  }
  fs::permissions(runtime_probe,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);

  ScopedEnvironmentVariable runtime_override(
      "LINUXOID_ART_RUNTIME_PROBE_OVERRIDE", runtime_probe.string());
  const auto report = wfa::RunNativeArtBootstrapExecutionFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(report.runner_invoked,
         "expected supervised runner to be invoked through override");
  Expect(report.execution_attempted,
         "expected bootstrap execution attempt through override");
  Expect(report.execution_succeeded,
         "expected bootstrap execution success through override");
  Expect(report.application_execution_attempted,
         "expected application phase attempt through override");
  Expect(report.application_execution_succeeded,
         "expected application phase success through override");
  Expect(report.activity_execution_attempted,
         "expected activity phase attempt through override");
  Expect(report.activity_execution_succeeded,
         "expected activity phase success through override");
  Expect(report.runner_exit_code == 0,
         "expected zero runner exit code through override");
  Expect(report.exit_reason == "bootstrap_execution_succeeded",
         "expected successful bootstrap execution exit reason");
  Expect(ReadTextFile(report.application_execution_log_path).find(
             "application-runtime-ok") != std::string::npos,
         "expected application runtime log output");
  Expect(ReadTextFile(report.activity_execution_log_path).find(
             "activity-runtime-ok") != std::string::npos,
         "expected activity runtime log output");

  fs::remove_all(fixture.root);
}

void TestNativeArtBootstrapExecutionCommandWritesStableJson() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-bootstrap-execution-command", true, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int exit_code = 0;
  const std::string output = ReadCommandOutput(
      compatctl.string() + " native-art-bootstrap-execution-fixture " +
          fixture.bootstrap.bootstrap_manifest_path,
      &exit_code);
  Expect(exit_code == 0,
         "expected native-art-bootstrap-execution-fixture success");
  Expect(output.find("\"execution_attempt_planned\": true") !=
             std::string::npos,
         "expected execution planned flag in bootstrap execution command json");
  Expect(output.find("\"execution_context_json_path\": ") !=
             std::string::npos,
         "expected execution context path in bootstrap execution command json");
  Expect(output.find("\"runner_state_json_path\": ") !=
             std::string::npos,
         "expected runner state path in bootstrap execution command json");
  Expect(output.find("\"selected_activity_class_name\": ") !=
             std::string::npos,
         "expected selected activity class in bootstrap execution command json");

  fs::remove_all(fixture.root);
}

void TestNativeArtClassResolutionFixtureResolvesManifestTargets() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-class-resolution", true, true);

  const auto report = wfa::RunNativeArtClassResolutionFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(report.classpath_plan_ready,
         "expected classpath plan ready before class resolution");
  Expect(report.offline_resolution_ready,
         "expected offline dex resolution readiness");
  Expect(report.resolved_target_count == 2,
         "expected both manifest targets resolved");
  Expect(report.missing_target_count == 0,
         "expected no missing manifest targets");
  Expect(fs::exists(report.resolution_map_path),
         "expected resolution map artifact");
  Expect(fs::exists(report.trace_jsonl_path),
         "expected class resolution trace artifact");
  Expect(fs::exists(report.result_json_path),
         "expected class resolution result artifact");
  Expect(report.target_results.size() == 2,
         "expected per-target class resolution results");
  Expect(std::all_of(report.target_results.begin(), report.target_results.end(),
                     [](const wfa::ResolvedClassTarget& result) {
                       return result.resolved_in_dex &&
                              result.resolution_reason == "resolved_in_dex";
                     }),
         "expected every manifest target resolved from dex");

  fs::remove_all(fixture.root);
}

void TestNativeArtClassResolutionFixtureHandlesMissingDexTargetsHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-class-resolution-missing", true, true);
  WriteStoredZipFixture(
      fs::path(fixture.bootstrap.plan.bundle_root) / "base.apk",
      {{"AndroidManifest.xml",
        R"(<manifest package="com.example.runtimehealth">
  <application android:name="com.example.runtimehealth.App">
    <activity android:name="com.example.runtimehealth.MissingActivity"/>
  </application>
</manifest>
)"},
       {"classes.dex", BuildResolvableDexPayload({"Lcom/example/runtimehealth/App;"})}});

  const auto report = wfa::RunNativeArtClassResolutionFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(report.resolved_target_count == 1,
         "expected only application target resolved");
  Expect(report.missing_target_count == 1,
         "expected one missing activity target");
  const auto missing_it = std::find_if(
      report.target_results.begin(), report.target_results.end(),
      [](const wfa::ResolvedClassTarget& result) {
        return result.class_name == "com.example.runtimehealth.MissingActivity";
      });
  Expect(missing_it != report.target_results.end(),
         "expected missing activity result");
  Expect(!missing_it->resolved_in_dex,
         "expected missing activity to stay unresolved");
  Expect(missing_it->resolution_reason == "descriptor_not_found_in_dex",
         "expected honest missing-descriptor reason");

  fs::remove_all(fixture.root);
}

void TestNativeArtClassResolutionCommandWritesStableJson() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-class-resolution-command", true, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int exit_code = 0;
  const std::string output = ReadCommandOutput(
      compatctl.string() + " native-art-class-resolution-fixture " +
          fixture.bootstrap.bootstrap_manifest_path,
      &exit_code);
  Expect(exit_code == 0,
         "expected native-art-class-resolution-fixture success");
  Expect(output.find("\"offline_resolution_ready\": true") !=
             std::string::npos,
         "expected offline resolution readiness in class-resolution json");
  Expect(output.find("\"resolved_target_count\": 2") != std::string::npos,
         "expected resolved target count in class-resolution json");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthFixtureSelectsMissingArtifactRecovery() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-missing-artifact", true, true);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "missing_artifact");

  const auto apk_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "apk_staging";
      });
  Expect(apk_record != report.records.end(), "expected apk staging record");
  Expect(apk_record->state == "missing",
         "expected missing-artifact staging classification");
  Expect(!apk_record->ready, "expected missing staging to stay unready");
  Expect(apk_record->selected_recovery_action == "restage_apk_bundle",
         "expected restage recovery action");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthFixtureSelectsUnavailableDisplayRecovery() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-unavailable-display", true, true);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "unavailable_display");

  const auto surface_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "surface_readiness";
      });
  Expect(surface_record != report.records.end(),
         "expected surface readiness record");
  Expect(surface_record->state == "degraded",
         "expected unavailable display degradation");
  Expect(surface_record->selected_recovery_action ==
             "fallback_to_headless_surface_probe",
         "expected fallback display recovery action");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthFixtureSelectsFailedServiceLookupRecovery() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-service-lookup", true, true);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "failed_service_lookup");

  const auto binder_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "binder_service_readiness";
      });
  Expect(binder_record != report.records.end(),
         "expected binder readiness record");
  Expect(!binder_record->ready,
         "expected forced service lookup failure to stay unready");
  Expect(binder_record->selected_recovery_action ==
             "rebuild_service_registry_and_retry_lookup",
         "expected binder recovery action");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthFixtureIncludesCoreSubsystemRecords() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-core-subsystems", true, true);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");

  const std::vector<std::string> expected_prefix = {
      "apk_staging",
      "native_loading",
      "surface_readiness",
      "input_queue_readiness",
      "binder_service_readiness",
      "dex_classloader_readiness",
  };

  Expect(report.records.size() >= expected_prefix.size(),
         "expected runtime health records to include the core subsystem set");
  Expect(report.core_subsystems == expected_prefix,
         "expected explicit core subsystem list in runtime health report");
  Expect(report.core_subsystem_count ==
             static_cast<int>(expected_prefix.size()),
         "expected deterministic core subsystem count");
  Expect(report.core_records.size() == expected_prefix.size(),
         "expected explicit core subsystem records in runtime health report");
  Expect(report.core_ready_subsystem_count == 5,
         "expected five ready core subsystems before dex/classloader startup");
  Expect(!report.core_subsystems_ready,
         "expected core subsystem summary to stay blocked on dex/classloader");
  for (std::size_t index = 0; index < expected_prefix.size(); ++index) {
    Expect(report.records[index].subsystem_name == expected_prefix[index],
           "expected deterministic core subsystem ordering in runtime health");
    Expect(report.core_records[index].subsystem_name == expected_prefix[index],
           "expected deterministic core subsystem ordering in core summary");
    Expect(!report.records[index].artifact_path.empty(),
           "expected core runtime health record artifact path");
    Expect(!report.records[index].state.empty(),
           "expected core runtime health record state");
  }

  const auto rendered = wfa::RenderRuntimeHealthReportJson(report);
  for (const auto& subsystem_name : expected_prefix) {
    Expect(rendered.find("\"subsystem_name\": \"" + subsystem_name + "\"") !=
               std::string::npos,
           "expected core subsystem name in runtime health json");
  }
  Expect(rendered.find("\"core_subsystem_count\": 6") != std::string::npos,
         "expected core subsystem count in runtime health json");
  Expect(rendered.find("\"core_ready_subsystem_count\": 5") !=
             std::string::npos,
         "expected core ready subsystem count in runtime health json");
  Expect(rendered.find("\"core_subsystems_ready\": false") !=
             std::string::npos,
         "expected blocked core subsystem summary in runtime health json");
  Expect(rendered.find("\"core_subsystem_records\": [") != std::string::npos,
         "expected explicit core subsystem record array in runtime health json");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthFixtureTreatsDexOnlyNativeLoadingAsNotRequired() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-dex-only-native", true, false);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");

  const auto native_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "native_loading";
      });
  Expect(native_record != report.records.end(),
         "expected native loading health record");
  Expect(native_record->state == "not_required",
         "expected dex-only bundle native loading to be not required");
  Expect(native_record->ready,
         "expected dex-only bundle native loading readiness");
  Expect(native_record->selected_recovery_action.empty(),
         "expected no native loading recovery action for dex-only bundle");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthFixtureRejectsMissingNativeDependencyWithoutFalseSuccess() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-missing-native", true, false, true);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");

  Expect(report.self_healing_ready,
         "expected runtime health fixture itself to stay available");
  Expect(!report.overall_ready,
         "expected missing native dependency to prevent false success");
  Expect(report.overall_state == "recovery_needed",
         "expected recovery-needed state for missing native dependency");

  const auto native_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "native_loading";
      });
  Expect(native_record != report.records.end(),
         "expected native loading health record");
  Expect(native_record->state == "blocked",
         "expected blocked native loading state");
  Expect(!native_record->ready,
         "expected missing native dependency to stay unready");
  Expect(native_record->selected_recovery_action ==
             "retry_native_load_after_bundle_refresh",
         "expected native load recovery action");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthFixtureTracksActivityBootstrapReadiness() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-activity-bootstrap", true, true);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");

  const auto bootstrap_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "activity_bootstrap_readiness";
      });
  Expect(bootstrap_record != report.records.end(),
         "expected activity bootstrap readiness record");
  Expect(bootstrap_record->artifact_path.find(
             "activity-bootstrap-result.json") != std::string::npos,
         "expected activity bootstrap result artifact path");
  Expect(bootstrap_record->ready,
         "expected activity bootstrap planning seam to be ready");
  Expect(bootstrap_record->selected_recovery_action.empty(),
         "expected no recovery action for ready activity bootstrap planning");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthFixtureCarriesBootstrapExecutionEvidence() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-bootstrap-execution", true, true);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");

  const auto bootstrap_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "bootstrap_execution_readiness";
      });
  Expect(bootstrap_record != report.records.end(),
         "expected bootstrap execution readiness record");
  Expect(bootstrap_record->artifact_path.find(
             "bootstrap-execution-result.json") != std::string::npos,
         "expected bootstrap execution result artifact path");
  Expect(bootstrap_record->selected_recovery_action ==
             "attempt_host_bootstrap_execution",
         "expected deterministic bootstrap execution recovery action");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthFixtureBecomesReadyWithRuntimeOverride() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-override-ready", true, true);
  const fs::path runtime_probe = fixture.root / "linuxoid-art-runtime-probe";
  {
    std::ofstream output(runtime_probe);
    output << "#!/bin/sh\n";
    output << "case \"$*\" in\n";
    output << "  *linuxoid.bootstrap.mode=application*) printf '%s\\n' "
              "'application-runtime-ok'; exit 0 ;;\n";
    output << "  *linuxoid.bootstrap.mode=activity*) printf '%s\\n' "
              "'activity-runtime-ok'; exit 0 ;;\n";
    output << "  *) printf '%s\\n' 'runtime-fixture-ok'; exit 0 ;;\n";
    output << "esac\n";
  }
  fs::permissions(runtime_probe,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);

  ScopedEnvironmentVariable runtime_override(
      "LINUXOID_ART_RUNTIME_PROBE_OVERRIDE", runtime_probe.string());
  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");

  Expect(report.self_healing_ready,
         "expected runtime health seam to stay available");
  Expect(report.overall_ready,
         "expected override-backed runtime path to satisfy runtime health");
  Expect(!report.dependency_blocked,
         "expected no dependency block once override-backed execution succeeds");
  Expect(report.overall_state == "ready",
         "expected ready overall state with successful override-backed execution");
  Expect(report.failing_subsystem_count == 0,
         "expected no failing subsystems after successful override-backed execution");
  Expect(report.recovery_actions_selected == 0,
         "expected no recovery actions after successful override-backed execution");

  const auto dex_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "dex_classloader_readiness";
      });
  Expect(dex_record != report.records.end(),
         "expected dex/classloader health record");
  Expect(dex_record->ready, "expected dex/classloader readiness");
  Expect(dex_record->state == "ready",
         "expected ready dex/classloader state");
  Expect(dex_record->selected_recovery_action.empty(),
         "expected no dex recovery action after success");

  const auto bootstrap_record = std::find_if(
      report.records.begin(), report.records.end(),
      [](const wfa::RuntimeHealthRecord& record) {
        return record.subsystem_name == "bootstrap_execution_readiness";
      });
  Expect(bootstrap_record != report.records.end(),
         "expected bootstrap execution readiness record");
  Expect(bootstrap_record->ready,
         "expected ready bootstrap execution state");
  Expect(bootstrap_record->state == "ready",
         "expected ready bootstrap execution status");
  Expect(bootstrap_record->selected_recovery_action.empty(),
         "expected no bootstrap execution recovery action after success");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthSummaryFieldsStayDeterministic() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-summary", true, false);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");

  Expect(report.dependency_blocked,
         "expected dex and bootstrap execution gaps to keep the runtime dependency blocked");
  Expect(report.failing_subsystem_count == 2,
         "expected dex and bootstrap execution subsystems to be counted as failing");
  Expect(report.recovery_actions_selected == 2,
         "expected two bounded recovery actions in summary fields");
  Expect(report.failing_subsystems.size() == 2,
         "expected stable failing subsystem list size");
  Expect(report.failing_subsystems[0] == "bootstrap_execution_readiness",
         "expected deterministic sorted failing subsystem order");
  Expect(report.failing_subsystems[1] == "dex_classloader_readiness",
         "expected deterministic sorted failing subsystem order");

  const auto rendered = wfa::RenderRuntimeHealthReportJson(report);
  Expect(rendered.find("\"dependency_blocked\": true") != std::string::npos,
         "expected dependency_blocked in runtime health json");
  Expect(rendered.find("\"failing_subsystem_count\": 2") != std::string::npos,
         "expected failing subsystem count in runtime health json");
  Expect(rendered.find("\"recovery_actions_selected\": 2") != std::string::npos,
         "expected recovery action count in runtime health json");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthReplaySummarizesTrace() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-replay", true, true);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");
  const auto replay = wfa::ReplayRuntimeHealthTrace(report.trace_jsonl_path);

  Expect(replay.events_read >= 8, "expected runtime health trace events");
  Expect(replay.subsystems_observed == 8,
         "expected eight subsystems in replay");
  Expect(std::find(replay.failing_subsystems.begin(),
                   replay.failing_subsystems.end(),
                   "dex_classloader_readiness") !=
             replay.failing_subsystems.end(),
         "expected dex classloader replay failure");
  Expect(std::find(replay.failing_subsystems.begin(),
                   replay.failing_subsystems.end(),
                   "bootstrap_execution_readiness") !=
             replay.failing_subsystems.end(),
         "expected bootstrap execution replay failure");
  Expect(std::find(replay.selected_actions.begin(),
                   replay.selected_actions.end(),
                   "attempt_host_art_class_resolution") !=
             replay.selected_actions.end(),
         "expected dex recovery action in replay");
  Expect(std::find(replay.selected_actions.begin(),
                   replay.selected_actions.end(),
                   "attempt_host_bootstrap_execution") !=
             replay.selected_actions.end(),
         "expected bootstrap execution recovery action in replay");

  const auto rendered = wfa::RenderRuntimeHealthReplayJson(replay);
  Expect(rendered.find("\"overall_state\": \"recovery_needed\"") !=
             std::string::npos,
         "expected replay overall state in json");

  fs::remove_all(fixture.root);
}

void TestRuntimeDiagnosticReplayReportsMissingNativeDependencyHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-diagnostic-missing-native", true, false, true);

  static_cast<void>(wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline"));
  const auto replay = wfa::ReplayRuntimeDiagnosticBundle(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(replay.replay_ready, "expected diagnostic replay artifacts");
  Expect(replay.overall_state == "recovery_needed",
         "expected missing native dependency to keep replay in recovery state");
  Expect(std::find(replay.failing_subsystems.begin(),
                   replay.failing_subsystems.end(),
                   "native_loading") != replay.failing_subsystems.end(),
         "expected native loading in failing subsystem list");
  Expect(std::find(replay.selected_actions.begin(),
                   replay.selected_actions.end(),
                   "retry_native_load_after_bundle_refresh") !=
             replay.selected_actions.end(),
         "expected native recovery action in diagnostic replay");
  Expect(std::find(replay.selected_actions.begin(),
                   replay.selected_actions.end(),
                   "attempt_host_bootstrap_execution") !=
             replay.selected_actions.end(),
         "expected bootstrap execution recovery action in diagnostic replay");

  fs::remove_all(fixture.root);
}

void TestNativeArtRuntimeSmokeWritesTraceJsonl() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-art-runtime-trace", true, true);

  const auto report = wfa::RunNativeArtRuntimeSmokeFixture(
      fixture.bootstrap.bootstrap_manifest_path);

  Expect(!report.trace_jsonl_path.empty(),
         "expected runtime smoke trace path");
  Expect(fs::exists(report.trace_jsonl_path),
         "expected runtime smoke trace artifact");
  const std::string trace = ReadTextFile(report.trace_jsonl_path);
  Expect(trace.find("\"event_type\": \"runtime_smoke_started\"") !=
             std::string::npos,
         "expected runtime smoke start event");
  Expect(trace.find("\"event_type\": \"runtime_smoke_complete\"") !=
             std::string::npos,
         "expected runtime smoke completion event");

  fs::remove_all(fixture.root);
}

void TestRuntimeDiagnosticReplayWritesStableArtifacts() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-diagnostic-replay", true, true);

  const auto health = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");
  const auto replay =
      wfa::ReplayRuntimeDiagnosticBundle(
          fixture.bootstrap.bootstrap_manifest_path);

  Expect(health.self_healing_ready, "expected baseline health fixture");
  Expect(replay.replay_ready, "expected diagnostic replay readiness");
  Expect(replay.trace_bundle_complete,
         "expected complete replayable trace bundle");
  Expect(fs::exists(replay.result_json_path),
         "expected diagnostic replay json artifact");
  Expect(fs::exists(replay.merged_trace_jsonl_path),
         "expected merged diagnostic trace artifact");
  Expect(fs::exists(replay.trace_index_json_path),
         "expected diagnostic trace index artifact");
  Expect(replay.health_trace_jsonl_path ==
             (fs::path(replay.artifact_root) / "runtime-health-trace.jsonl")
                 .string(),
         "expected stable health trace path in diagnostic replay report");
  Expect(replay.health_replay_json_path ==
             (fs::path(replay.artifact_root) / "runtime-health-replay.json")
                 .string(),
         "expected stable health replay path in diagnostic replay report");
  Expect(replay.canonical_trace_source_count == 7,
         "expected seven canonical replay trace sources");
  Expect(replay.trace_sources_found == replay.canonical_trace_source_count,
         "expected full trace-source coverage in diagnostic replay");
  Expect(replay.missing_trace_source_count == 0,
         "expected no missing trace sources in complete replay bundle");
  const std::vector<std::string> expected_source_names = {
      "runtime_health_trace",
      "runtime_recovery_actions",
      "art_classloader_trace",
      "art_class_resolution_trace",
      "art_runtime_smoke_trace",
      "art_activity_bootstrap_trace",
      "art_bootstrap_execution_trace",
  };
  Expect(replay.canonical_trace_source_names == expected_source_names,
         "expected deterministic canonical trace source ordering");
  Expect(replay.total_events_read >= replay.trace_sources_found,
         "expected trace events across diagnostic sources");
  Expect(std::find(replay.selected_actions.begin(),
                   replay.selected_actions.end(),
                   "attempt_host_art_class_resolution") !=
             replay.selected_actions.end(),
         "expected dex recovery action in diagnostic replay");
  Expect(std::find_if(replay.trace_sources.begin(), replay.trace_sources.end(),
                      [](const wfa::RuntimeDiagnosticTraceSource& source) {
                        return source.source_name ==
                               "art_activity_bootstrap_trace";
                      }) != replay.trace_sources.end(),
         "expected activity bootstrap trace source in diagnostic replay");
  Expect(std::find_if(replay.trace_sources.begin(), replay.trace_sources.end(),
                      [](const wfa::RuntimeDiagnosticTraceSource& source) {
                        return source.source_name ==
                               "art_bootstrap_execution_trace";
                      }) != replay.trace_sources.end(),
         "expected bootstrap execution trace source in diagnostic replay");
  const std::string trace_index = ReadTextFile(replay.trace_index_json_path);
  Expect(trace_index.find("\"source_name\": \"runtime_health_trace\"") !=
             std::string::npos,
         "expected runtime health trace in diagnostic index");
  Expect(trace_index.find(
             "\"source_name\": \"art_activity_bootstrap_trace\"") !=
             std::string::npos,
         "expected activity bootstrap trace in diagnostic index");
  Expect(trace_index.find(
             "\"source_name\": \"art_bootstrap_execution_trace\"") !=
             std::string::npos,
         "expected bootstrap execution trace in diagnostic index");
  Expect(trace_index.find("\"source_fingerprint\": ") != std::string::npos,
         "expected source fingerprint in diagnostic index");
  Expect(trace_index.find("\"first_event_type\": ") != std::string::npos,
         "expected first event type in diagnostic index");
  Expect(trace_index.find("\"trace_bundle_complete\": true") !=
             std::string::npos,
         "expected trace bundle completeness in diagnostic index");
  Expect(trace_index.find("\"canonical_trace_source_count\": 7") !=
             std::string::npos,
         "expected canonical trace source count in diagnostic index");
  Expect(trace_index.find("\"canonical_trace_source_names\": ") !=
             std::string::npos,
         "expected canonical trace source names in diagnostic index");

  fs::remove_all(fixture.root);
}

void TestRuntimeDiagnosticReplayHandlesMissingTraceHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-diagnostic-missing", true, true);

  const auto health = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");
  fs::remove(health.trace_jsonl_path);

  const auto replay =
      wfa::ReplayRuntimeDiagnosticBundle(
          fixture.bootstrap.bootstrap_manifest_path);

  Expect(!replay.replay_ready,
         "expected replay to stay unready when a trace is missing");
  Expect(!replay.trace_bundle_complete,
         "expected incomplete trace bundle when a trace is missing");
  Expect(replay.exit_reason == "missing_trace_artifact",
         "expected missing-trace exit reason");
  Expect(replay.missing_trace_source_count == 1,
         "expected one missing trace source");
  Expect(std::find(replay.missing_trace_sources.begin(),
                   replay.missing_trace_sources.end(),
                   "runtime_health_trace") !=
             replay.missing_trace_sources.end(),
         "expected missing runtime health trace classification");

  fs::remove_all(fixture.root);
}

void TestRuntimeDiagnosticReplayCommandWritesStableJson() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-diagnostic-command", true, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  static_cast<void>(wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline"));

  int exit_code = 0;
  const std::string output = ReadCommandOutput(
      compatctl.string() + " native-runtime-diagnostic-replay " +
          fixture.bootstrap.bootstrap_manifest_path,
      &exit_code);
  Expect(exit_code == 0, "expected native-runtime-diagnostic-replay success");
  Expect(output.find("\"replay_ready\": true") != std::string::npos,
         "expected replay readiness in diagnostic replay json");
  Expect(output.find("\"trace_bundle_complete\": true") !=
             std::string::npos,
         "expected trace bundle completeness in diagnostic replay json");
  Expect(output.find("\"canonical_trace_source_count\": 7") !=
             std::string::npos,
         "expected canonical trace source count in diagnostic replay json");
  Expect(output.find("\"health_trace_jsonl_path\": ") != std::string::npos,
         "expected health trace path in diagnostic replay json");
  Expect(output.find("\"health_replay_json_path\": ") != std::string::npos,
         "expected health replay path in diagnostic replay json");
  Expect(output.find("\"merged_trace_jsonl_path\": ") != std::string::npos,
         "expected merged trace path in diagnostic replay json");
  Expect(output.find("\"trace_index_json_path\": ") != std::string::npos,
         "expected trace index path in diagnostic replay json");

  fs::remove_all(fixture.root);
}

void TestRuntimeDiagnosticFixtureCommandWritesStableJson() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-diagnostic-fixture-command", true, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int exit_code = 0;
  const std::string output = ReadCommandOutput(
      compatctl.string() + " native-runtime-diagnostic-fixture " +
          fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &exit_code);
  Expect(exit_code == 0, "expected native-runtime-diagnostic-fixture success");
  Expect(output.find("\"scenario_name\": \"baseline\"") != std::string::npos,
         "expected scenario name in diagnostic fixture json");
  Expect(output.find("\"trace_bundle_complete\": true") !=
             std::string::npos,
         "expected trace bundle completeness in diagnostic fixture json");
  Expect(output.find("\"canonical_trace_source_count\": 7") !=
             std::string::npos,
         "expected canonical trace source count in diagnostic fixture json");
  Expect(output.find("\"trace_index_json_path\": ") != std::string::npos,
         "expected trace index path in diagnostic fixture json");
  Expect(output.find("\"replay_ready\": true") != std::string::npos,
         "expected replay readiness in diagnostic fixture json");

  fs::remove_all(fixture.root);
}

void TestRuntimeDiagnosticFixtureMaterializesReplayableTraceBundle() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-diagnostic-fixture-replayable-bundle", true, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int fixture_exit_code = 0;
  const std::string fixture_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-diagnostic-fixture " +
          fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &fixture_exit_code);
  Expect(fixture_exit_code == 0,
         "expected native-runtime-diagnostic-fixture success");
  Expect(fixture_output.find("\"trace_sources_found\": 7") !=
             std::string::npos,
         "expected all replay trace sources in fixture json");
  Expect(fixture_output.find("\"trace_bundle_complete\": true") !=
             std::string::npos,
         "expected complete trace bundle in fixture json");
  Expect(fixture_output.find("\"canonical_trace_source_count\": 7") !=
             std::string::npos,
         "expected canonical trace source count in fixture json");

  const auto lifecycle =
      wfa::BuildNativeLifecycleShimFromManifest(
          fixture.bootstrap.bootstrap_manifest_path);
  const fs::path session_root = lifecycle.session_root;
  const std::vector<fs::path> expected_trace_paths = {
      session_root / "health" / "runtime-health-trace.jsonl",
      session_root / "health" / "runtime-recovery-actions.jsonl",
      session_root / "art" / "art-classloader-trace.jsonl",
      session_root / "art" / "art-class-resolution-trace.jsonl",
      session_root / "art" / "runtime-smoke-trace.jsonl",
      session_root / "art" / "activity-bootstrap-trace.jsonl",
      session_root / "art" / "bootstrap-execution-trace.jsonl",
  };
  for (const auto& trace_path : expected_trace_paths) {
    Expect(fs::exists(trace_path),
           "expected replayable trace artifact from diagnostic fixture");
    Expect(fs::file_size(trace_path) > 0,
           "expected non-empty replayable trace artifact");
  }

  int replay_exit_code = 0;
  const std::string replay_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-replay " +
          (session_root / "health" / "runtime-health-trace.jsonl").string(),
      &replay_exit_code);
  Expect(replay_exit_code == 0, "expected native-runtime-health-replay success");
  Expect(replay_output.find("\"subsystems_observed\": 8") !=
             std::string::npos,
         "expected replay to summarize the full runtime-health trace");
  Expect(replay_output.find("\"overall_state\": \"recovery_needed\"") !=
             std::string::npos,
         "expected replay to stay honest about remaining runtime gaps");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthCommandWritesStableJson() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-command", true, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int exit_code = 0;
  const std::string output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &exit_code);
  Expect(exit_code == 0, "expected native-runtime-health-fixture success");
  Expect(output.find("\"scenario_name\": \"baseline\"") != std::string::npos,
         "expected scenario name in runtime health json");
  Expect(output.find("\"self_healing_ready\": true") != std::string::npos,
         "expected self-healing ready flag in runtime health json");
  Expect(output.find("\"core_subsystems\": [\"apk_staging\", \"native_loading\", "
                     "\"surface_readiness\", \"input_queue_readiness\", "
                     "\"binder_service_readiness\", "
                     "\"dex_classloader_readiness\"]") !=
             std::string::npos,
         "expected explicit core subsystem list in runtime health command json");
  Expect(output.find("\"core_subsystem_records\": [") != std::string::npos,
         "expected core subsystem record array in runtime health command json");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthCommandOutputIsStableAcrossRepeatedRuns() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-command-stability", true, false);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int first_exit_code = 0;
  const std::string first_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &first_exit_code);
  int second_exit_code = 0;
  const std::string second_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &second_exit_code);

  Expect(first_exit_code == 0 && second_exit_code == 0,
         "expected repeated runtime-health command success");
  Expect(first_output == second_output,
         "expected stable repeated runtime-health JSON output");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthCommandSupportsLegacyBootstrapManifestWithoutNativeLibrarySummaryFields() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-command-legacy-bootstrap", true, false);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  std::string legacy_manifest =
      ReadTextFile(fixture.bootstrap.bootstrap_manifest_path);
  auto strip_line = [&legacy_manifest](const std::string& line) {
    const std::size_t position = legacy_manifest.find(line);
    if (position != std::string::npos) {
      legacy_manifest.erase(position, line.size());
    }
  };
  strip_line("  \"native_libraries_declared\": false,\n");
  strip_line("  \"discovered_native_library_count\": 0,\n");
  {
    std::ofstream output(fixture.bootstrap.bootstrap_manifest_path);
    output << legacy_manifest;
  }

  int exit_code = 0;
  const std::string output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &exit_code);
  Expect(exit_code == 0,
         "expected runtime-health command success for legacy bootstrap manifest");
  Expect(output.find("\"subsystem_name\": \"native_loading\"") !=
             std::string::npos,
         "expected native loading record in legacy bootstrap command json");
  Expect(output.find("\"state\": \"not_required\"") != std::string::npos,
         "expected dex-only legacy bootstrap native loading to stay not required");
  Expect(output.find("\"overall_ready\": false") != std::string::npos,
         "expected legacy bootstrap command to stay honest about remaining gaps");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthCommandReportsMissingNativeDependencyHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-command-missing-native", true, false, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int exit_code = 0;
  const std::string output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &exit_code);
  Expect(exit_code == 0, "expected native-runtime-health-fixture success");
  Expect(output.find("\"overall_ready\": false") != std::string::npos,
         "expected overall readiness to stay false");
  Expect(output.find("\"overall_state\": \"recovery_needed\"") !=
             std::string::npos,
         "expected recovery-needed state in command json");
  Expect(output.find("\"subsystem_name\": \"native_loading\"") !=
             std::string::npos,
         "expected native loading record in command json");
  Expect(output.find(
             "\"selected_recovery_action\": "
             "\"retry_native_load_after_bundle_refresh\"") !=
             std::string::npos,
         "expected native recovery action in command json");

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthCommandMissingNativeContractStaysDeterministic() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-command-missing-native-contract", true, false,
      true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int first_exit_code = 0;
  const std::string first_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &first_exit_code);
  int second_exit_code = 0;
  const std::string second_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &second_exit_code);

  Expect(first_exit_code == 0 && second_exit_code == 0,
         "expected repeated missing-native runtime-health command success");
  Expect(first_output == second_output,
         "expected stable repeated missing-native runtime-health JSON output");
  Expect(first_output.find("\"overall_ready\": false") != std::string::npos,
         "expected no false success in missing-native runtime-health json");
  Expect(first_output.find("\"overall_state\": \"recovery_needed\"") !=
             std::string::npos,
         "expected recovery-needed classification in missing-native json");
  Expect(first_output.find("\"dependency_blocked\": true") !=
             std::string::npos,
         "expected dependency-blocked classification in missing-native json");
  Expect(first_output.find("\"subsystem_name\": \"native_loading\"") !=
             std::string::npos,
         "expected native-loading record in missing-native json");
  Expect(first_output.find("\"state\": \"blocked\"") != std::string::npos,
         "expected blocked native-loading state in missing-native json");
  Expect(first_output.find(
             "\"selected_recovery_action\": "
             "\"retry_native_load_after_bundle_refresh\"") !=
             std::string::npos,
         "expected deterministic native-load recovery action in missing-native json");

  fs::remove_all(fixture.root);
}

void TestRuntimeRecoveryPlanWritesStableArtifacts() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-recovery-plan", true, true);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");

  Expect(fs::exists(report.recovery_plan_path),
         "expected runtime recovery plan artifact");
  Expect(fs::exists(report.recovery_actions_jsonl_path),
         "expected runtime recovery actions trace artifact");
  Expect(!report.recovery_actions.empty(),
         "expected at least one deterministic baseline recovery action");

  const auto dex_action = std::find_if(
      report.recovery_actions.begin(), report.recovery_actions.end(),
      [](const wfa::RuntimeRecoveryAction& action) {
        return action.subsystem_name == "dex_classloader_readiness";
      });
  Expect(dex_action != report.recovery_actions.end(),
         "expected dex recovery action");
  Expect(dex_action->action_name == "attempt_host_art_class_resolution",
         "expected host ART recovery action");
  Expect(!dex_action->artifact_path.empty(),
         "expected deterministic recovery artifact path");
  Expect(!dex_action->replay_trace_path.empty(),
         "expected deterministic replay trace path");

  const std::string recovery_plan_json = ReadTextFile(report.recovery_plan_path);
  Expect(recovery_plan_json.find("\"action_rank\": 50") != std::string::npos,
         "expected deterministic action rank in recovery plan");
  Expect(recovery_plan_json.find("\"retry_budget\": 0") != std::string::npos,
         "expected deterministic retry budget in recovery plan");
  Expect(recovery_plan_json.find("\"recovery_scope\": \"art_bridge\"") !=
             std::string::npos,
         "expected deterministic recovery scope in recovery plan");

  fs::remove_all(fixture.root);
}

void TestRuntimeRecoveryPlanScenariosSelectDeterministicActions() {
  namespace fs = std::filesystem;
  auto missing_fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-recovery-missing", true, true);
  auto native_fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-recovery-native", true, true);
  auto display_fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-recovery-display", true, true);
  auto binder_fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-recovery-binder", true, true);

  const auto missing = wfa::RunRuntimeHealthFixture(
      missing_fixture.bootstrap.bootstrap_manifest_path, "missing_artifact");
  const auto native = wfa::RunRuntimeHealthFixture(
      native_fixture.bootstrap.bootstrap_manifest_path, "failed_native_load");
  const auto display = wfa::RunRuntimeHealthFixture(
      display_fixture.bootstrap.bootstrap_manifest_path, "unavailable_display");
  const auto binder = wfa::RunRuntimeHealthFixture(
      binder_fixture.bootstrap.bootstrap_manifest_path, "failed_service_lookup");

  auto has_action = [](const wfa::RuntimeHealthReport& report,
                       const std::string& action_name) {
    return std::find_if(report.recovery_actions.begin(),
                        report.recovery_actions.end(),
                        [&](const wfa::RuntimeRecoveryAction& action) {
                          return action.action_name == action_name;
                        }) != report.recovery_actions.end();
  };

  Expect(has_action(missing, "restage_apk_bundle"),
         "expected restage action for missing artifact");
  Expect(has_action(native, "retry_native_load_after_bundle_refresh"),
         "expected native refresh action for failed native load");
  Expect(has_action(display, "fallback_to_headless_surface_probe"),
         "expected fallback action for unavailable display");
  Expect(has_action(binder, "rebuild_service_registry_and_retry_lookup"),
         "expected registry rebuild action for failed service lookup");

  const std::vector<std::string> expected_scenarios = {
      "missing_artifact",
      "failed_native_load",
      "unavailable_display",
      "failed_service_lookup",
  };
  const std::vector<std::string> expected_actions = {
      "restage_apk_bundle",
      "retry_native_load_after_bundle_refresh",
      "fallback_to_headless_surface_probe",
      "rebuild_service_registry_and_retry_lookup",
  };
  Expect(missing.canonical_recovery_scenario_count == 4,
         "expected four canonical recovery scenarios");
  Expect(missing.canonical_recovery_scenarios.size() == expected_scenarios.size(),
         "expected explicit canonical recovery scenario contract");
  for (std::size_t index = 0; index < expected_scenarios.size(); ++index) {
    Expect(missing.canonical_recovery_scenarios[index].scenario_name ==
               expected_scenarios[index],
           "expected deterministic canonical recovery scenario ordering");
    Expect(missing.canonical_recovery_scenarios[index].action_name ==
               expected_actions[index],
           "expected deterministic canonical recovery action mapping");
  }

  const std::string missing_plan = ReadTextFile(missing.recovery_plan_path);
  const std::string native_plan = ReadTextFile(native.recovery_plan_path);
  const std::string display_plan = ReadTextFile(display.recovery_plan_path);
  const std::string binder_plan = ReadTextFile(binder.recovery_plan_path);
  Expect(missing_plan.find("\"canonical_recovery_scenario_count\": 4") !=
             std::string::npos,
         "expected canonical recovery scenario count in recovery plan");
  Expect(missing_plan.find("\"canonical_recovery_scenarios\": [") !=
             std::string::npos,
         "expected explicit canonical recovery scenarios in recovery plan");
  Expect(missing_plan.find("\"scenario_name\": \"missing_artifact\"") !=
             std::string::npos,
         "expected missing-artifact scenario in recovery plan");
  Expect(missing_plan.find(
             "\"action_name\": \"restage_apk_bundle\"") !=
             std::string::npos,
         "expected missing-artifact action in recovery plan");
  Expect(missing_plan.find("\"action_rank\": 10") != std::string::npos,
         "expected deterministic missing-artifact rank");
  Expect(missing_plan.find("\"retry_budget\": 1") != std::string::npos,
         "expected deterministic missing-artifact retry budget");
  Expect(missing_plan.find("\"recovery_scope\": \"bundle\"") !=
             std::string::npos,
         "expected deterministic missing-artifact scope");
  Expect(native_plan.find("\"action_rank\": 20") != std::string::npos,
         "expected deterministic native-load rank");
  Expect(native_plan.find("\"retry_budget\": 1") != std::string::npos,
         "expected deterministic native-load retry budget");
  Expect(native_plan.find("\"recovery_scope\": \"native_loader\"") !=
             std::string::npos,
         "expected deterministic native-load scope");
  Expect(display_plan.find("\"action_rank\": 30") != std::string::npos,
         "expected deterministic display rank");
  Expect(display_plan.find("\"retry_budget\": 0") != std::string::npos,
         "expected deterministic display retry budget");
  Expect(display_plan.find("\"recovery_scope\": \"graphics_probe\"") !=
             std::string::npos,
         "expected deterministic display scope");
  Expect(binder_plan.find("\"action_rank\": 40") != std::string::npos,
         "expected deterministic binder rank");
  Expect(binder_plan.find("\"retry_budget\": 1") != std::string::npos,
         "expected deterministic binder retry budget");
  Expect(binder_plan.find("\"recovery_scope\": \"service_registry\"") !=
             std::string::npos,
         "expected deterministic binder scope");

  fs::remove_all(missing_fixture.root);
  fs::remove_all(native_fixture.root);
  fs::remove_all(display_fixture.root);
  fs::remove_all(binder_fixture.root);
}

void TestRuntimeRecoveryPlanCommandWritesStableJson() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-recovery-command", true, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int exit_code = 0;
  const std::string output = ReadCommandOutput(
      compatctl.string() + " native-runtime-recovery-plan " +
          fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &exit_code);
  Expect(exit_code == 0, "expected native-runtime-recovery-plan success");
  Expect(output.find("\"recovery_plan_path\": ") != std::string::npos,
         "expected recovery plan path in recovery json");
  Expect(output.find("\"action_name\": \"attempt_host_art_class_resolution\"") !=
             std::string::npos,
         "expected deterministic dex recovery action in json");
  Expect(output.find("\"action_rank\": 50") != std::string::npos,
         "expected deterministic action rank in recovery json");
  Expect(output.find("\"retry_budget\": 0") != std::string::npos,
         "expected deterministic retry budget in recovery json");
  Expect(output.find("\"recovery_scope\": \"art_bridge\"") !=
             std::string::npos,
         "expected deterministic recovery scope in recovery json");
  Expect(output.find("\"canonical_recovery_scenario_count\": 4") !=
             std::string::npos,
         "expected canonical recovery scenario count in recovery json");
  Expect(output.find("\"canonical_recovery_scenarios\": [") !=
             std::string::npos,
         "expected canonical recovery scenario contract in recovery json");
  Expect(output.find("\"scenario_name\": \"failed_native_load\"") !=
             std::string::npos,
         "expected failed-native-load scenario in recovery json");
  Expect(output.find(
             "\"action_name\": \"retry_native_load_after_bundle_refresh\"") !=
             std::string::npos,
         "expected failed-native-load action in recovery json");

  fs::remove_all(fixture.root);
}

void TestRuntimeRecoveryPlanCommandCoreScenariosStayDeterministic() {
  namespace fs = std::filesystem;
  auto missing_fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-recovery-command-missing", true, true);
  auto native_fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-recovery-command-native", true, true);
  auto display_fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-recovery-command-display", true, true);
  auto binder_fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-recovery-command-binder", true, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  auto run_command = [&](const std::string& manifest_path,
                         const std::string& scenario_name) {
    int exit_code = 0;
    const std::string output = ReadCommandOutput(
        compatctl.string() + " native-runtime-recovery-plan " + manifest_path +
            " " + scenario_name,
        &exit_code);
    Expect(exit_code == 0,
           "expected native-runtime-recovery-plan scenario command success");
    return output;
  };

  const std::string missing_output = run_command(
      missing_fixture.bootstrap.bootstrap_manifest_path, "missing_artifact");
  const std::string native_output = run_command(
      native_fixture.bootstrap.bootstrap_manifest_path, "failed_native_load");
  const std::string display_output = run_command(
      display_fixture.bootstrap.bootstrap_manifest_path, "unavailable_display");
  const std::string binder_output = run_command(
      binder_fixture.bootstrap.bootstrap_manifest_path,
      "failed_service_lookup");

  Expect(missing_output.find("\"scenario_name\": \"missing_artifact\"") !=
             std::string::npos,
         "expected missing-artifact scenario name in command json");
  Expect(missing_output.find("\"action_name\": \"restage_apk_bundle\"") !=
             std::string::npos,
         "expected restage action in missing-artifact command json");
  Expect(missing_output.find("\"action_rank\": 10") != std::string::npos,
         "expected deterministic missing-artifact rank in command json");

  Expect(native_output.find("\"scenario_name\": \"failed_native_load\"") !=
             std::string::npos,
         "expected failed-native-load scenario name in command json");
  Expect(native_output.find(
             "\"action_name\": "
             "\"retry_native_load_after_bundle_refresh\"") !=
             std::string::npos,
         "expected native-load retry action in command json");
  Expect(native_output.find("\"action_rank\": 20") != std::string::npos,
         "expected deterministic native-load rank in command json");

  Expect(display_output.find("\"scenario_name\": \"unavailable_display\"") !=
             std::string::npos,
         "expected unavailable-display scenario name in command json");
  Expect(display_output.find(
             "\"action_name\": \"fallback_to_headless_surface_probe\"") !=
             std::string::npos,
         "expected headless fallback action in command json");
  Expect(display_output.find("\"action_rank\": 30") != std::string::npos,
         "expected deterministic display rank in command json");

  Expect(binder_output.find("\"scenario_name\": \"failed_service_lookup\"") !=
             std::string::npos,
         "expected failed-service-lookup scenario name in command json");
  Expect(binder_output.find(
             "\"action_name\": "
             "\"rebuild_service_registry_and_retry_lookup\"") !=
             std::string::npos,
         "expected binder retry action in command json");
  Expect(binder_output.find("\"action_rank\": 40") != std::string::npos,
         "expected deterministic binder rank in command json");

  fs::remove_all(missing_fixture.root);
  fs::remove_all(native_fixture.root);
  fs::remove_all(display_fixture.root);
  fs::remove_all(binder_fixture.root);
}

void TestSelfHealingRuntimeCliContractMatrix() {
  namespace fs = std::filesystem;
  auto baseline_fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-self-healing-cli-contract-baseline", true, true);
  auto missing_native_fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-self-healing-cli-contract-missing-native", true, false, true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int baseline_exit_code = 0;
  const std::string baseline_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          baseline_fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &baseline_exit_code);
  Expect(baseline_exit_code == 0,
         "expected baseline runtime-health command success");
  Expect(baseline_output.find("\"overall_state\": \"recovery_needed\"") !=
             std::string::npos,
         "expected deterministic health classification in baseline command json");
  Expect(baseline_output.find("\"self_healing_ready\": true") !=
             std::string::npos,
         "expected self-healing ready flag in baseline command json");

  const struct RecoveryScenarioExpectation {
    const char* scenario_name;
    const char* action_name;
  } recovery_expectations[] = {
      {"missing_artifact", "restage_apk_bundle"},
      {"failed_native_load", "retry_native_load_after_bundle_refresh"},
      {"unavailable_display", "fallback_to_headless_surface_probe"},
      {"failed_service_lookup", "rebuild_service_registry_and_retry_lookup"},
  };

  for (const auto& expectation : recovery_expectations) {
    int recovery_exit_code = 0;
    const std::string recovery_output = ReadCommandOutput(
        compatctl.string() + " native-runtime-recovery-plan " +
            baseline_fixture.bootstrap.bootstrap_manifest_path + " " +
            expectation.scenario_name,
        &recovery_exit_code);
    Expect(recovery_exit_code == 0,
           "expected scenario recovery-plan command success");
    Expect(recovery_output.find("\"scenario_name\": \"" +
                                    std::string(expectation.scenario_name) +
                                    "\"") != std::string::npos,
           "expected recovery scenario name in command json");
    Expect(recovery_output.find("\"action_name\": \"" +
                                    std::string(expectation.action_name) +
                                    "\"") != std::string::npos,
           "expected deterministic recovery action selection in command json");
  }

  int first_missing_native_exit_code = 0;
  const std::string first_missing_native_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          missing_native_fixture.bootstrap.bootstrap_manifest_path +
          " baseline",
      &first_missing_native_exit_code);
  int second_missing_native_exit_code = 0;
  const std::string second_missing_native_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          missing_native_fixture.bootstrap.bootstrap_manifest_path +
          " baseline",
      &second_missing_native_exit_code);

  Expect(first_missing_native_exit_code == 0 &&
             second_missing_native_exit_code == 0,
         "expected repeated missing-native runtime-health command success");
  Expect(first_missing_native_output == second_missing_native_output,
         "expected stable repeated missing-native runtime-health json");
  Expect(first_missing_native_output.find("\"overall_ready\": false") !=
             std::string::npos,
         "expected no false success when a dependency is missing");
  Expect(first_missing_native_output.find("\"overall_state\": \"recovery_needed\"") !=
             std::string::npos,
         "expected recovery-needed classification when a dependency is missing");
  Expect(first_missing_native_output.find(
             "\"selected_recovery_action\": "
             "\"retry_native_load_after_bundle_refresh\"") !=
             std::string::npos,
         "expected deterministic recovery selection when native dependency is missing");

  fs::remove_all(baseline_fixture.root);
  fs::remove_all(missing_native_fixture.root);
}

void TestSelfHealingRuntimeCliContractMatrixIncludesReplayHonesty() {
  namespace fs = std::filesystem;
  auto baseline_fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-self-healing-cli-contract-replay-baseline", true, true);
  auto missing_native_fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-self-healing-cli-contract-replay-missing-native", true, false,
      true);
  const fs::path compatctl = ResolveBuildDirFromTestBinary() / "compatctl";

  int baseline_first_exit_code = 0;
  const std::string baseline_first_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          baseline_fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &baseline_first_exit_code);
  int baseline_second_exit_code = 0;
  const std::string baseline_second_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          baseline_fixture.bootstrap.bootstrap_manifest_path + " baseline",
      &baseline_second_exit_code);

  Expect(baseline_first_exit_code == 0 && baseline_second_exit_code == 0,
         "expected repeated baseline runtime-health command success");
  Expect(baseline_first_output == baseline_second_output,
         "expected stable repeated baseline runtime-health json");
  Expect(baseline_first_output.find("\"core_subsystem_count\": 6") !=
             std::string::npos,
         "expected six core subsystems in baseline runtime-health json");
  Expect(baseline_first_output.find("\"core_subsystems\": [") !=
             std::string::npos,
         "expected explicit core subsystem projection in baseline json");
  Expect(baseline_first_output.find("\"apk_staging\"") != std::string::npos,
         "expected apk staging in baseline core subsystem projection");
  Expect(baseline_first_output.find("\"dex_classloader_readiness\"") !=
             std::string::npos,
         "expected dex classloader readiness in baseline json");

  int recovery_exit_code = 0;
  const std::string recovery_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-recovery-plan " +
          baseline_fixture.bootstrap.bootstrap_manifest_path +
          " failed_native_load",
      &recovery_exit_code);
  Expect(recovery_exit_code == 0,
         "expected failed-native-load recovery-plan command success");
  Expect(recovery_output.find("\"scenario_name\": \"failed_native_load\"") !=
             std::string::npos,
         "expected failed-native-load scenario name in recovery command json");
  Expect(recovery_output.find(
             "\"action_name\": "
             "\"retry_native_load_after_bundle_refresh\"") !=
             std::string::npos,
         "expected deterministic failed-native-load action in recovery command json");
  Expect(recovery_output.find("\"action_rank\": 20") != std::string::npos,
         "expected deterministic failed-native-load action rank");
  Expect(recovery_output.find("\"retry_budget\": 1") != std::string::npos,
         "expected deterministic failed-native-load retry budget");

  int missing_first_exit_code = 0;
  const std::string missing_first_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          missing_native_fixture.bootstrap.bootstrap_manifest_path +
          " baseline",
      &missing_first_exit_code);
  int missing_second_exit_code = 0;
  const std::string missing_second_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-health-fixture " +
          missing_native_fixture.bootstrap.bootstrap_manifest_path +
          " baseline",
      &missing_second_exit_code);

  Expect(missing_first_exit_code == 0 && missing_second_exit_code == 0,
         "expected repeated missing-native runtime-health command success");
  Expect(missing_first_output == missing_second_output,
         "expected stable repeated missing-native runtime-health json");
  Expect(missing_first_output.find("\"overall_ready\": false") !=
             std::string::npos,
         "expected no false success in missing-native runtime-health json");
  Expect(missing_first_output.find("\"overall_state\": \"recovery_needed\"") !=
             std::string::npos,
         "expected recovery-needed classification in missing-native json");
  Expect(missing_first_output.find("\"dependency_blocked\": true") !=
             std::string::npos,
         "expected dependency-blocked state in missing-native json");
  Expect(missing_first_output.find("\"subsystem_name\": \"native_loading\"") !=
             std::string::npos,
         "expected native_loading record in missing-native json");
  Expect(missing_first_output.find(
             "\"selected_recovery_action\": "
             "\"retry_native_load_after_bundle_refresh\"") !=
             std::string::npos,
         "expected deterministic missing-native recovery action in runtime-health json");

  int replay_exit_code = 0;
  const std::string replay_output = ReadCommandOutput(
      compatctl.string() + " native-runtime-diagnostic-replay " +
          missing_native_fixture.bootstrap.bootstrap_manifest_path,
      &replay_exit_code);
  Expect(replay_exit_code == 0,
         "expected missing-native diagnostic replay command success");
  Expect(replay_output.find("\"replay_ready\": true") != std::string::npos,
         "expected replay readiness in missing-native diagnostic replay json");
  Expect(replay_output.find("\"trace_bundle_complete\": true") !=
             std::string::npos,
         "expected complete trace bundle in missing-native diagnostic replay json");
  Expect(replay_output.find("\"overall_state\": \"recovery_needed\"") !=
             std::string::npos,
         "expected recovery-needed state in missing-native diagnostic replay json");
  Expect(replay_output.find("\"native_loading\"") != std::string::npos,
         "expected native_loading in missing-native diagnostic replay json");
  Expect(replay_output.find("\"retry_native_load_after_bundle_refresh\"") !=
             std::string::npos,
         "expected deterministic missing-native recovery action in diagnostic replay json");

  fs::remove_all(baseline_fixture.root);
  fs::remove_all(missing_native_fixture.root);
}

void TestNativeLifecycleShimWritesSessionArtifacts() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "linuxoid-native-lifecycle-test";
  fs::remove_all(root);
  const fs::path compat_root = root / "compat";
  const fs::path native_root = root / "native";
  const fs::path compatctl_path = root / "compatctl";
  fs::create_directories(root);

  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }

  const auto layout = wfa::BuildPackageLayout(
      {.package_name = "com.example.simple",
       .install_id = "vc7-1.0.0",
       .version_code = 7},
      compat_root.string());
  fs::create_directories(layout.host_package_root);

  {
    std::ofstream apk(layout.host_package_root + "/base.apk");
    apk << "apk payload\n";
  }
  {
    std::ofstream manifest(layout.host_package_root + "/AndroidManifest.xml");
    manifest << "<manifest package=\"com.example.simple\"/>\n";
  }
  {
    std::ofstream assessment(layout.host_package_root + "/assessment.txt");
    assessment << "simple candidate\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/simple.apk",
      .install_id = "vc7-1.0.0",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "simple.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 7,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.simple",
          .launcher_activity_name = "com.example.simple.MainActivity",
          .declared_components = {"com.example.simple.MainActivity"},
          .declared_activity_components = {"com.example.simple.MainActivity"},
          .has_launcher_activity = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.simple",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "P6",
          .has_launcher_activity = true,
      },
      .layout = layout,
      .install_root = layout.host_package_root,
  };

  const auto plan = wfa::BuildNativeLaunchPlan(report, native_root.string());
  const auto bootstrap =
      wfa::BuildNativeActivityBootstrap(plan, compatctl_path.string());
  const auto lifecycle = wfa::BuildNativeLifecycleShim(bootstrap);

  Expect(lifecycle.lifecycle_handoff_ready,
         "expected lifecycle shim handoff to be ready");
  Expect(!lifecycle.execution_engine_ready,
         "expected lifecycle execution engine to remain pending");
  Expect(lifecycle.current_activity_state == "NOT_CREATED",
         "expected lifecycle shim to remain pre-launch");
  Expect(fs::exists(lifecycle.session_manifest_path),
         "expected session manifest");
  Expect(fs::exists(lifecycle.activity_state_path),
         "expected activity state file");
  Expect(fs::exists(lifecycle.service_registry_path),
         "expected service registry file");
  Expect(fs::exists(lifecycle.binder_manager_metadata_path),
         "expected binder manager metadata file");
  Expect(fs::exists(lifecycle.binder_lookup_log_path),
         "expected binder lookup log file");
  Expect(fs::exists(lifecycle.binder_transaction_log_path),
         "expected binder transaction log file");
  Expect(fs::exists(lifecycle.report_path),
         "expected lifecycle report");

  std::ifstream state_input(lifecycle.activity_state_path);
  std::string state((std::istreambuf_iterator<char>(state_input)),
                    std::istreambuf_iterator<char>());
  Expect(state.find("created=false") != std::string::npos,
         "expected pre-launch created state in lifecycle file");
  Expect(state.find("started=false") != std::string::npos,
         "expected pre-launch started state in lifecycle file");
  Expect(state.find("resumed=false") != std::string::npos,
         "expected pre-launch resumed state in lifecycle file");
  Expect(state.find("current_state=NOT_CREATED") != std::string::npos,
         "expected pre-launch activity state in lifecycle file");

  std::ifstream services_input(lifecycle.service_registry_path);
  std::string services((std::istreambuf_iterator<char>(services_input)),
                       std::istreambuf_iterator<char>());
  Expect(services.find("activity_manager") != std::string::npos,
         "expected activity manager service");
  Expect(services.find("package_manager") != std::string::npos,
         "expected package manager service");

  std::ifstream binder_lookup_input(lifecycle.binder_lookup_log_path);
  std::string binder_lookups(
      (std::istreambuf_iterator<char>(binder_lookup_input)),
      std::istreambuf_iterator<char>());
  Expect(binder_lookups.find("\"service_name\": \"package_manager\"") !=
             std::string::npos,
         "expected package manager lookup");

  std::ifstream binder_transaction_input(lifecycle.binder_transaction_log_path);
  std::string binder_transactions(
      (std::istreambuf_iterator<char>(binder_transaction_input)),
      std::istreambuf_iterator<char>());
  Expect(binder_transactions.find(
             "\"transaction_name\": \"scheduleLaunchActivity\"") !=
             std::string::npos,
         "expected activity manager transaction");

  const auto rendered = wfa::RenderNativeLifecycleShimReport(lifecycle);
  Expect(rendered.find("Lifecycle Handoff Ready: yes") !=
             std::string::npos,
         "expected lifecycle handoff line");
  Expect(rendered.find("Binder Service Manager Ready: yes") !=
             std::string::npos,
         "expected binder manager readiness line");
  Expect(rendered.find("Execution Engine Ready: no") != std::string::npos,
         "expected lifecycle execution readiness line");
  Expect(rendered.find("Process State: BOOTSTRAPPED") != std::string::npos,
         "expected bootstrapped process state in lifecycle report");

  fs::remove_all(root);
}

void TestNativeProcessBootstrapRunsFixtureAndWritesSessionState() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-native-process-bootstrap-test";
  fs::remove_all(root);
  const fs::path compat_root = root / "compat";
  const fs::path native_root = root / "native";
  const fs::path compatctl_path = ResolveBuildDirFromTestBinary() / "compatctl";
  fs::create_directories(root);

  const auto layout = wfa::BuildPackageLayout(
      {.package_name = "com.example.simple",
       .install_id = "vc7-1.0.0",
       .version_code = 7},
      compat_root.string());
  fs::create_directories(layout.host_package_root);

  {
    std::ofstream apk(layout.host_package_root + "/base.apk");
    apk << "apk payload\n";
  }
  {
    std::ofstream manifest(layout.host_package_root + "/AndroidManifest.xml");
    manifest << "<manifest package=\"com.example.simple\"/>\n";
  }
  {
    std::ofstream assessment(layout.host_package_root + "/assessment.txt");
    assessment << "simple candidate\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/simple.apk",
      .install_id = "vc7-1.0.0",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "simple.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 7,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.simple",
          .launcher_activity_name = "com.example.simple.MainActivity",
          .declared_components = {"com.example.simple.MainActivity"},
          .declared_activity_components = {"com.example.simple.MainActivity"},
          .has_launcher_activity = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.simple",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "P6",
          .has_launcher_activity = true,
      },
      .layout = layout,
      .install_root = layout.host_package_root,
  };

  const auto plan = wfa::BuildNativeLaunchPlan(report, native_root.string());
  const auto bootstrap =
      wfa::BuildNativeActivityBootstrap(plan, compatctl_path.string());

  const fs::path build_dir = ResolveBuildDirFromTestBinary();
  const fs::path fixture_library = build_dir / "liblinuxoid_p1_fixture.so";
  Expect(fs::exists(fixture_library),
         "expected linuxoid p1 fixture library to exist");
  fs::copy_file(fixture_library, fs::path(plan.library_root) / "libcalculator.so",
                fs::copy_options::overwrite_existing);

  const std::string command =
      compatctl_path.string() + " native-execute-stub " +
      bootstrap.bootstrap_manifest_path;

  int status = 0;
  const std::string output = ReadCommandOutput(command, &status);

  Expect(WIFEXITED(status), "expected bootstrap command to exit normally");
  Expect(WEXITSTATUS(status) == 0,
         "expected fixture native process bootstrap to exit 0");
  Expect(output.find("\"execution_engine_ready\": true") != std::string::npos,
         "expected execution readiness in bootstrap json");
  Expect(output.find("\"libraries_loaded\": [") != std::string::npos,
         "expected libraries array in bootstrap json");
  Expect(output.find("\"jni_onload_results\": [") != std::string::npos,
         "expected jni results in bootstrap json");
  Expect(output.find("\"runner_report_path\":") != std::string::npos,
         "expected runner report path in bootstrap json");

  const fs::path session_root =
      fs::path(plan.package_root) / "lifecycle" / (plan.assessment.install_id + "-default");
  const fs::path session_manifest = session_root / "session.json";
  const fs::path runner_log = session_root / "runner.log";
  const fs::path runner_report = session_root / "runner-report.json";
  Expect(fs::exists(session_manifest),
         "expected session manifest after process bootstrap");
  Expect(fs::exists(runner_log),
         "expected runner log after process bootstrap");
  Expect(fs::exists(runner_report),
         "expected runner report after process bootstrap");

  std::ifstream session_input(session_manifest);
  std::string session((std::istreambuf_iterator<char>(session_input)),
                      std::istreambuf_iterator<char>());
  Expect(session.find("\"process_state\": \"EXITED\"") != std::string::npos,
         "expected exited process state in session manifest");
  Expect(session.find("\"exit_code\": 0") != std::string::npos,
         "expected zero exit code in session manifest");
  Expect(session.find("\"entrypoint_found\": true") != std::string::npos,
         "expected entrypoint flag in session manifest");
  Expect(session.find("\"exit_reason\": \"native_activity_completed\"") !=
             std::string::npos,
         "expected exit reason in session manifest");

  std::ifstream runner_input(runner_log);
  std::string runner((std::istreambuf_iterator<char>(runner_input)),
                     std::istreambuf_iterator<char>());
  Expect(runner.find("[p1] entrypoint found: ANativeActivity_onCreate in") !=
             std::string::npos,
         "expected native runner entrypoint line in log");
  Expect(runner.find("[fixture] JNI_OnLoad invoked") != std::string::npos,
         "expected fixture jni onload line in log");
  Expect(runner.find("[p1] watchdog: 5s elapsed, clean exit") !=
             std::string::npos,
         "expected native runner watchdog line in log");

  std::ifstream runner_report_input(runner_report);
  std::string runner_report_text(
      (std::istreambuf_iterator<char>(runner_report_input)),
      std::istreambuf_iterator<char>());
  Expect(runner_report_text.find("\"jni_onload_results\": [") !=
             std::string::npos,
         "expected jni results in runner report");
  Expect(runner_report_text.find("\"working_directory\":") !=
             std::string::npos,
         "expected working directory in runner report");

  fs::remove_all(root);
}

void TestNativeExecuteStubReportsMissingNativeLibraryPayload() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-p1-missing-lib-test";
  fs::remove_all(root);
  fs::create_directories(root / "sandbox");
  fs::create_directories(root / "dex-cache");
  fs::create_directories(root / "resources");
  fs::create_directories(root / "lib");
  {
    std::ofstream apk(root / "base.apk");
    apk << "fake apk\n";
  }
  {
    std::ofstream manifest(root / "activity-bootstrap.json");
    manifest << "{}\n";
  }

  const fs::path build_dir = ResolveBuildDirFromTestBinary();
  const std::string command =
      (build_dir / "compatctl").string() +
      " native-execute-stub com.android.calculator2 com.android.calculator2/.Calculator " +
      (root / "base.apk").string() + " " + (root / "sandbox").string() + " " +
      (root / "dex-cache").string() + " " + (root / "resources").string() +
      " " + (root / "lib").string() + " " +
      (root / "activity-bootstrap.json").string();

  int status = 0;
  const std::string output = ReadCommandOutput(command, &status);

  Expect(WIFEXITED(status), "expected command to exit normally");
  Expect(WEXITSTATUS(status) == 0,
         "expected missing native library path to exit 0 as a soft failure");
  Expect(output.find("\"execution_engine_ready\": false") !=
             std::string::npos,
         "expected false execution readiness for missing-library case");
  Expect(output.find("\"exit_reason\": \"no_native_libraries_found\"") !=
             std::string::npos,
         "expected missing native library exit reason");
  Expect(output.find("\"libraries_loaded\": []") != std::string::npos,
         "expected empty libraries list");

  fs::remove_all(root);
}

void TestNativeExecuteStubRunsFixtureNativeActivity() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-p1-fixture-test";
  fs::remove_all(root);
  fs::create_directories(root / "sandbox");
  fs::create_directories(root / "dex-cache");
  fs::create_directories(root / "resources");
  fs::create_directories(root / "lib");
  {
    std::ofstream apk(root / "base.apk");
    apk << "fixture apk\n";
  }
  {
    std::ofstream manifest(root / "activity-bootstrap.json");
    manifest << "{}\n";
  }

  const fs::path build_dir = ResolveBuildDirFromTestBinary();
  const fs::path fixture_library = build_dir / "liblinuxoid_p1_fixture.so";
  Expect(fs::exists(fixture_library),
         "expected linuxoid p1 fixture library to exist");
  fs::copy_file(fixture_library, root / "lib" / "libcalculator.so",
                fs::copy_options::overwrite_existing);

  const std::string command =
      (build_dir / "compatctl").string() +
      " native-execute-stub com.android.calculator2 com.android.calculator2/.Calculator " +
      (root / "base.apk").string() + " " + (root / "sandbox").string() + " " +
      (root / "dex-cache").string() + " " + (root / "resources").string() +
      " " + (root / "lib").string() + " " +
      (root / "activity-bootstrap.json").string();

  int status = 0;
  const std::string output = ReadCommandOutput(command, &status);

  Expect(WIFEXITED(status), "expected fixture command to exit normally");
  Expect(WEXITSTATUS(status) == 0,
         "expected fixture native execute path to exit 0");
  Expect(output.find("\"execution_engine_ready\": true") !=
             std::string::npos,
         "expected execution readiness for fixture run");
  Expect(output.find("\"libraries_loaded\": [") != std::string::npos,
         "expected loaded libraries in fixture json");
  Expect(output.find("libcalculator.so") != std::string::npos,
         "expected calculator fixture library in output json");
  Expect(output.find("\"status\": \"called\"") != std::string::npos,
         "expected called jni onload result");
  Expect(output.find("\"entrypoint_found\": true") !=
             std::string::npos,
         "expected entrypoint flag in fixture json");
  Expect(output.find("\"activity_called\": true") !=
             std::string::npos,
         "expected activity flag in fixture json");

  fs::remove_all(root);
}

void TestRuntimeBridgeOutputParsers() {
  Expect(wfa::OutputContainsInstalledPackage("package:org.futo.inputmethod.latin\n",
                                             "org.futo.inputmethod.latin"),
         "expected installed package parser");
  Expect(!wfa::OutputContainsInstalledPackage(
             "package:org.futo.inputmethod.latin.debug\n",
             "org.futo.inputmethod.latin"),
         "expected exact package matching");
  Expect(wfa::OutputContainsImeId("org.futo.inputmethod.latin/.LatinIME\n",
                                  "org.futo.inputmethod.latin/.LatinIME"),
         "expected ime parser");
  Expect(wfa::OutputContainsImeId(
             "org.futo.inputmethod.latin/.LatinIME:\n",
             "org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME"),
         "expected equivalent ime component forms");
  Expect(!wfa::OutputContainsImeId(
             "org.futo.inputmethod.latin/.LatinIMEBeta:\n",
             "org.futo.inputmethod.latin/.LatinIME"),
         "expected exact ime matching");
  Expect(wfa::EnabledInputMethodsContainIme(
             "com.example.other/.Ime:org.futo.inputmethod.latin/.LatinIME",
             "org.futo.inputmethod.latin/.LatinIME"),
         "expected enabled-input-method parser");
  Expect(!wfa::EnabledInputMethodsContainIme(
             "org.futo.inputmethod.latin/.LatinIMEBeta:com.example.other/.Ime",
             "org.futo.inputmethod.latin/.LatinIME"),
         "expected exact enabled-input-method matching");
  Expect(wfa::LaunchOutputLooksSuccessful("Status: ok\nComplete\n"),
         "expected launch parser");

  const auto report = wfa::RenderAdbImeStatusReport(wfa::AdbImeStatus{
      .serial = "emulator-5590",
      .package_name = "org.futo.inputmethod.latin",
      .ime_id = "org.futo.inputmethod.latin/.LatinIME",
      .settings_component = "",
      .package_installed = true,
      .ime_registered = true,
      .ime_enabled = true,
      .is_default_ime = true,
      .settings_launch_ok = false,
      .enabled_input_methods =
          "com.example.other/.Ime:org.futo.inputmethod.latin/.LatinIME",
      .default_input_method = "org.futo.inputmethod.latin/.LatinIME",
  });
  Expect(report.find("IME enabled: yes") != std::string::npos,
         "expected enabled ime in status report");
  Expect(report.find("Settings launch OK: not checked") != std::string::npos,
         "expected not-checked launch status");
}

void TestActivityLaunchReportRendering() {
  const std::string component =
      "org.futo.inputmethod.latin/.uix.settings.SettingsActivity";
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command.find(" shell am start -W -n ") != std::string::npos) {
      return {0,
              "Starting: Intent { cmp=org.futo.inputmethod.latin/.uix.settings.SettingsActivity }\nStatus: ok\nComplete\n"};
    }
    throw std::runtime_error("unexpected command in launch-activity test");
  };

  const auto report = wfa::LaunchAdbActivityWithRunner(
      "emulator-5590", component, runner);
  Expect(report.launch_ok, "expected successful activity launch");

  const auto rendered = wfa::RenderAdbActivityLaunchReport(report);
  Expect(rendered.find("Launch OK: yes") != std::string::npos,
         "expected launch success line");
}

void TestNativeRuntimeDiscoveryUsesCompatRootOverride() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-runtime-discovery");
  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());

  const auto report = wfa::DiscoverRuntimeTargetsWithRunner(
      wfa::RuntimeBackendKind::kNative,
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error("native discovery should not shell out");
      });

  Expect(report.backend_available, "expected native backend availability");
  Expect(report.targets.size() == 1, "expected one native runtime target");
  Expect(report.targets.front().serial == "linuxoid-native",
         "expected deterministic native runtime serial");
  Expect(report.targets.front().online, "expected native runtime online");
  Expect(report.backend_check_output.find(fixture.compat_root.string()) !=
             std::string::npos,
         "expected compat root in native discovery output");

  fs::remove_all(fixture.root);
}

void TestNativeRuntimeMetadataReadsStagedPackage() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-runtime-metadata");
  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());

  const auto report = wfa::QueryInstalledPackageMetadataWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = fixture.package_name},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error("native metadata lookup should not shell out");
      });

  Expect(report.package_visible, "expected staged native package visibility");
  Expect(report.launcher_resolved, "expected native launcher resolution");
  Expect(report.resolved_component == fixture.launcher_component,
         "expected launcher component from staged metadata");
  Expect(report.install_path == fixture.apk_path,
         "expected staged install path");
  Expect(report.version_code == "9", "expected staged version code");
  Expect(report.version_name == "2.0.0", "expected staged version name");
  Expect(report.notes.find("native package metadata resolved") !=
             std::string::npos,
         "expected native metadata success note");

  fs::remove_all(fixture.root);
}

void TestNativeRuntimePreflightUsesStagedMetadata() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-runtime-preflight");
  const fs::path native_root = fixture.root / "native";
  const fs::path runtime_probe = fixture.root / "linuxoid-art-runtime-probe";
  {
    std::ofstream output(runtime_probe);
    output << "#!/bin/sh\n";
    output << "printf '%s\\n' 'runtime-fixture-ok'\n";
    output << "exit 0\n";
  }
  fs::permissions(runtime_probe,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);
  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());
  ScopedEnvironmentVariable native_root_override("LINUXOID_NATIVE_SPIKE_ROOT",
                                                 native_root.string());
  ScopedEnvironmentVariable runtime_override(
      "LINUXOID_ART_RUNTIME_PROBE_OVERRIDE", runtime_probe.string());
  ScopedEnvironmentVariable allow_override_launch(
      "LINUXOID_NATIVE_ALLOW_RUNTIME_OVERRIDE", "1");

  const auto report = wfa::PreflightRuntimeWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = fixture.package_name},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error("native preflight should not shell out");
      });

  Expect(report.backend_available, "expected native backend availability");
  Expect(report.target_discovered, "expected native target discovery");
  Expect(report.target_selected, "expected native target selection");
  Expect(report.target_online, "expected native target online");
  Expect(report.package_visible, "expected staged package visibility");
  Expect(report.component_ready, "expected native component readiness");
  Expect(report.ready_for_launch, "expected native preflight readiness");
  Expect(report.runtime_probe_ready,
         "expected runtime probe readiness for override-backed preflight");
  Expect(report.bootstrap_planned,
         "expected bootstrap planning for override-backed preflight");
  Expect(report.art_runtime_probe_source == "override",
         "expected override-backed preflight runtime source");
  Expect(!report.bootstrap_manifest_path.empty(),
         "expected bootstrap manifest path in native preflight");
  Expect(!report.runtime_health_json_path.empty(),
         "expected runtime health path in native preflight");
  Expect(!report.runtime_health_trace_jsonl_path.empty(),
         "expected runtime health trace path in native preflight");
  Expect(!report.runtime_recovery_actions_jsonl_path.empty(),
         "expected runtime recovery actions path in native preflight");
  Expect(!report.runtime_health_replay_json_path.empty(),
         "expected runtime health replay path in native preflight");
  Expect(!report.runtime_diagnostic_replay_json_path.empty(),
         "expected diagnostic replay path in native preflight");
  Expect(!report.runtime_diagnostic_trace_index_path.empty(),
         "expected diagnostic trace index path in native preflight");
  Expect(!report.runtime_diagnostic_events_jsonl_path.empty(),
         "expected diagnostic events path in native preflight");
  Expect(report.runtime_diagnostic_replay_ready,
         "expected diagnostic replay readiness in native preflight");
  Expect(report.runtime_trace_bundle_complete,
         "expected complete trace bundle in native preflight");
  Expect(report.runtime_canonical_trace_source_count == 7,
         "expected seven canonical trace sources in native preflight");
  Expect(report.runtime_trace_sources_found == 7,
         "expected full trace-source coverage in native preflight");
  Expect(report.runtime_missing_trace_source_count == 0,
         "expected no missing trace sources in native preflight");
  Expect(report.recovery_actions_selected == 0,
         "expected no selected recovery actions in ready native preflight");
  Expect(report.selected_recovery_actions.empty(),
         "expected no selected recovery action names in ready native preflight");
  Expect(report.canonical_recovery_scenario_count == 4,
         "expected four canonical recovery scenarios in native preflight");
  Expect(report.canonical_recovery_scenarios ==
             std::vector<std::string>{
                 "missing_artifact=>restage_apk_bundle",
                 "failed_native_load=>retry_native_load_after_bundle_refresh",
                 "unavailable_display=>fallback_to_headless_surface_probe",
                 "failed_service_lookup=>rebuild_service_registry_and_retry_lookup"},
         "expected canonical recovery scenarios in ready native preflight");
  Expect(report.component == fixture.launcher_component,
         "expected launcher component in preflight result");

  fs::remove_all(fixture.root);
}

void TestNativeRuntimePreflightBlocksWithoutHostArt() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-runtime-preflight-host-art-missing");
  const fs::path native_root = fixture.root / "native";
  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());
  ScopedEnvironmentVariable native_root_override("LINUXOID_NATIVE_SPIKE_ROOT",
                                                 native_root.string());
  ScopedEnvironmentVariable disable_host_art(
      "LINUXOID_DISABLE_HOST_ART_RUNTIME_PROBE", "1");

  const auto report = wfa::PreflightRuntimeWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = fixture.package_name},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error("native preflight should not shell out");
      });

  Expect(report.package_visible, "expected staged package visibility");
  Expect(report.component_ready, "expected native component readiness");
  Expect(!report.ready_for_launch,
         "expected native preflight to block without host ART");
  Expect(!report.runtime_probe_ready,
         "expected runtime probe to stay unready without host ART");
  Expect(report.bootstrap_planned,
         "expected bootstrap planning even when host ART is unavailable");
  Expect(report.art_runtime_probe_source == "missing",
         "expected missing runtime probe source without host ART");
  Expect(!report.runtime_health_trace_jsonl_path.empty(),
         "expected runtime health trace path without host ART");
  Expect(!report.runtime_recovery_actions_jsonl_path.empty(),
         "expected runtime recovery actions path without host ART");
  Expect(!report.runtime_health_replay_json_path.empty(),
         "expected runtime health replay path without host ART");
  Expect(!report.runtime_diagnostic_trace_index_path.empty(),
         "expected diagnostic trace index path without host ART");
  Expect(!report.runtime_diagnostic_events_jsonl_path.empty(),
         "expected diagnostic events path without host ART");
  Expect(report.runtime_diagnostic_replay_ready,
         "expected diagnostic replay readiness without host ART");
  Expect(report.runtime_trace_bundle_complete,
         "expected complete trace bundle without host ART");
  Expect(report.runtime_canonical_trace_source_count == 7,
         "expected seven canonical trace sources without host ART");
  Expect(report.runtime_trace_sources_found == 7,
         "expected full trace-source coverage without host ART");
  Expect(report.runtime_missing_trace_source_count == 0,
         "expected no missing trace sources without host ART");
  Expect(report.dependency_blocked,
         "expected dependency block without host ART");
  Expect(report.failing_subsystem_count == 2,
         "expected dex and bootstrap execution to remain blocked in preflight");
  Expect(report.recovery_actions_selected == 2,
         "expected two selected recovery actions without host ART");
  Expect(std::find(report.selected_recovery_actions.begin(),
                   report.selected_recovery_actions.end(),
                   "dex_classloader_readiness=>attempt_host_art_class_resolution") !=
             report.selected_recovery_actions.end(),
         "expected dex recovery action in native preflight");
  Expect(std::find(report.selected_recovery_actions.begin(),
                   report.selected_recovery_actions.end(),
                   "bootstrap_execution_readiness=>attempt_host_bootstrap_execution") !=
             report.selected_recovery_actions.end(),
         "expected bootstrap execution recovery action in native preflight");
  Expect(report.canonical_recovery_scenario_count == 4,
         "expected four canonical recovery scenarios without host ART");
  Expect(std::find(report.canonical_recovery_scenarios.begin(),
                   report.canonical_recovery_scenarios.end(),
                   "missing_artifact=>restage_apk_bundle") !=
             report.canonical_recovery_scenarios.end(),
         "expected missing-artifact canonical recovery scenario in native preflight");
  Expect(std::find(report.failing_subsystems.begin(),
                   report.failing_subsystems.end(),
                   "dex_classloader_readiness") !=
             report.failing_subsystems.end(),
         "expected dex classloader failure in native preflight");
  Expect(std::find(report.failing_subsystems.begin(),
                   report.failing_subsystems.end(),
                   "bootstrap_execution_readiness") !=
             report.failing_subsystems.end(),
         "expected bootstrap execution failure in native preflight");
  Expect(report.notes.find("host ART runtime is not detected") !=
             std::string::npos,
         "expected missing host ART note in native preflight");

  fs::remove_all(fixture.root);
}

void TestNativeRuntimePreflightReportsHostAppProcessCapabilityHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-runtime-preflight-app-process");
  const fs::path native_root = fixture.root / "native";
  const fs::path app_process = fixture.root / "app_process";
  {
    std::ofstream output(app_process);
    output << "#!/bin/sh\n";
    output << "printf '%s\\n' 'host-app-process'\n";
    output << "exit 0\n";
  }
  fs::permissions(app_process,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);

  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());
  ScopedEnvironmentVariable native_root_override("LINUXOID_NATIVE_SPIKE_ROOT",
                                                 native_root.string());
  ScopedEnvironmentVariable path_override("PATH", fixture.root.string());

  const auto report = wfa::PreflightRuntimeWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = fixture.package_name},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error("native preflight should not shell out");
      });

  Expect(report.art_runtime_probe_source == "host",
         "expected host probe source in native preflight");
  Expect(report.runtime_probe_detection_reason == "host_app_process_selected",
         "expected app_process detection reason in native preflight");
  Expect(report.art_runtime_probe_capability ==
             "host_app_process_detection_only",
         "expected app_process capability in native preflight");
  Expect(!report.runtime_probe_ready,
         "expected detection-only app_process not to mark preflight ready");
  Expect(report.notes.find("not yet bootstrap-capable") != std::string::npos,
         "expected capability-specific native preflight note");
  const auto rendered = wfa::RenderRuntimePreflightReport(report);
  Expect(rendered.find(
             "ART Runtime Probe Capability: host_app_process_detection_only") !=
             std::string::npos,
         "expected app_process capability line in native preflight render");

  fs::remove_all(fixture.root);
}

void TestNativeRuntimePreflightSurfacesProbeInventoryAndReason() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-runtime-preflight-probe-inventory");
  const fs::path native_root = fixture.root / "native";
  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());
  ScopedEnvironmentVariable native_root_override("LINUXOID_NATIVE_SPIKE_ROOT",
                                                 native_root.string());
  ScopedEnvironmentVariable disable_host_art(
      "LINUXOID_DISABLE_HOST_ART_RUNTIME_PROBE", "1");

  const auto report = wfa::PreflightRuntimeWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = fixture.package_name},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error("native preflight should not shell out");
      });

  Expect(!report.runtime_probe_inventory_json_path.empty(),
         "expected probe inventory path in native preflight");
  Expect(fs::exists(report.runtime_probe_inventory_json_path),
         "expected probe inventory artifact in native preflight");
  Expect(report.runtime_probe_detection_reason == "host_probe_disabled",
         "expected disabled-host probe detection reason in native preflight");
  const auto rendered = wfa::RenderRuntimePreflightReport(report);
  Expect(rendered.find("ART Runtime Probe Inventory Path: ") !=
             std::string::npos,
         "expected probe inventory path line in native preflight render");
  Expect(rendered.find("ART Runtime Probe Detection Reason: "
                       "host_probe_disabled") != std::string::npos,
         "expected probe detection reason line in native preflight render");

  fs::remove_all(fixture.root);
}

void TestNativeRuntimeLaunchCanUseOverrideBackedBootstrapExecution() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-runtime-launch");
  const fs::path native_root = fixture.root / "native";
  const fs::path runtime_probe = fixture.root / "linuxoid-art-runtime-probe";
  {
    std::ofstream output(runtime_probe);
    output << "#!/bin/sh\n";
    output << "case \"$*\" in\n";
    output << "  *linuxoid.bootstrap.mode=application*) printf '%s\\n' "
              "'application-runtime-ok'; exit 0 ;;\n";
    output << "  *linuxoid.bootstrap.mode=activity*) printf '%s\\n' "
              "'activity-runtime-ok'; exit 0 ;;\n";
    output << "  *) printf '%s\\n' 'runtime-fixture-ok'; exit 0 ;;\n";
    output << "esac\n";
  }
  fs::permissions(runtime_probe,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);

  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());
  ScopedEnvironmentVariable native_root_override("LINUXOID_NATIVE_SPIKE_ROOT",
                                                 native_root.string());
  ScopedEnvironmentVariable runtime_override(
      "LINUXOID_ART_RUNTIME_PROBE_OVERRIDE", runtime_probe.string());
  ScopedEnvironmentVariable allow_override_launch(
      "LINUXOID_NATIVE_ALLOW_RUNTIME_OVERRIDE", "1");

  const auto report = wfa::LaunchInstalledAppWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = fixture.package_name},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error("native launch should not shell out through runtime bridge runner");
      });

  Expect(report.launch_ok,
         "expected override-backed native launch success classification");
  Expect(report.launch_classification ==
             "native_bootstrap_execution_succeeded",
         "expected successful native bootstrap launch classification");
  Expect(report.art_runtime_probe_source == "override",
         "expected override runtime probe source");
  Expect(report.component == fixture.launcher_component,
         "expected native launch component");
  Expect(!report.bootstrap_manifest_path.empty(),
         "expected bootstrap manifest path on successful native launch");
  Expect(!report.bootstrap_execution_result_path.empty(),
         "expected bootstrap execution result path on successful native launch");
  Expect(!report.bootstrap_execution_trace_jsonl_path.empty(),
         "expected bootstrap execution trace path on successful native launch");
  Expect(!report.bootstrap_execution_runner_state_json_path.empty(),
         "expected bootstrap runner state path on successful native launch");
  Expect(!report.runtime_health_json_path.empty(),
         "expected runtime health artifact path on successful native launch");
  Expect(!report.runtime_health_trace_jsonl_path.empty(),
         "expected runtime health trace path on successful native launch");
  Expect(!report.runtime_recovery_plan_path.empty(),
         "expected runtime recovery plan path on successful native launch");
  Expect(!report.runtime_recovery_actions_jsonl_path.empty(),
         "expected runtime recovery actions trace path on successful native launch");
  Expect(!report.runtime_health_replay_json_path.empty(),
         "expected runtime health replay path on successful native launch");
  Expect(!report.runtime_diagnostic_replay_json_path.empty(),
         "expected diagnostic replay artifact path on successful native launch");
  Expect(!report.runtime_diagnostic_trace_index_path.empty(),
         "expected diagnostic trace index path on successful native launch");
  Expect(!report.runtime_diagnostic_events_jsonl_path.empty(),
         "expected diagnostic events path on successful native launch");
  Expect(report.runtime_health_ready,
         "expected runtime health report on successful native launch");
  Expect(report.runtime_diagnostic_replay_ready,
         "expected diagnostic replay readiness on successful native launch");
  Expect(report.runtime_trace_bundle_complete,
         "expected complete trace bundle on successful native launch");
  Expect(report.runtime_canonical_trace_source_count == 7,
         "expected seven canonical trace sources on successful native launch");
  Expect(report.runtime_trace_sources_found == 7,
         "expected full trace-source coverage on successful native launch");
  Expect(report.runtime_missing_trace_source_count == 0,
         "expected no missing trace sources on successful native launch");
  Expect(!report.runtime_dependency_blocked,
         "expected no blocked runtime dependency on successful native launch");
  Expect(report.runtime_failing_subsystem_count == 0,
         "expected no failing runtime subsystems on successful native launch");
  Expect(report.runtime_recovery_actions_selected == 0,
         "expected no selected recovery actions on successful native launch");
  Expect(report.runtime_selected_recovery_actions.empty(),
         "expected no selected recovery action names on successful native launch");
  Expect(report.runtime_canonical_recovery_scenario_count == 4,
         "expected four canonical recovery scenarios on successful native launch");
  Expect(report.output.find("\"execution_succeeded\": true") !=
             std::string::npos,
         "expected successful bootstrap execution output");

  fs::remove_all(fixture.root);
}

void TestNativeRuntimeLaunchRejectsOverrideBackedBootstrapByDefault() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-runtime-launch-reject-override");
  const fs::path native_root = fixture.root / "native";
  const fs::path runtime_probe = fixture.root / "linuxoid-art-runtime-probe";
  {
    std::ofstream output(runtime_probe);
    output << "#!/bin/sh\n";
    output << "case \"$*\" in\n";
    output << "  *linuxoid.bootstrap.mode=application*) printf '%s\\n' "
              "'application-runtime-ok'; exit 0 ;;\n";
    output << "  *linuxoid.bootstrap.mode=activity*) printf '%s\\n' "
              "'activity-runtime-ok'; exit 0 ;;\n";
    output << "  *) printf '%s\\n' 'runtime-fixture-ok'; exit 0 ;;\n";
    output << "esac\n";
  }
  fs::permissions(runtime_probe,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);

  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());
  ScopedEnvironmentVariable native_root_override("LINUXOID_NATIVE_SPIKE_ROOT",
                                                 native_root.string());
  ScopedEnvironmentVariable runtime_override(
      "LINUXOID_ART_RUNTIME_PROBE_OVERRIDE", runtime_probe.string());

  const auto report = wfa::LaunchInstalledAppWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = fixture.package_name},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error(
            "native launch should not shell out through runtime bridge runner");
      });

  Expect(!report.launch_ok,
         "expected native launch to reject fixture override by default");
  Expect(report.launch_classification == "fixture_override_rejected",
         "expected fixture override rejection classification");
  Expect(report.art_runtime_probe_source == "override",
         "expected override runtime probe source on rejected launch");
  Expect(!report.bootstrap_manifest_path.empty(),
         "expected bootstrap manifest path on rejected override launch");
  Expect(!report.bootstrap_execution_result_path.empty(),
         "expected bootstrap execution result path on rejected override launch");
  Expect(!report.runtime_health_json_path.empty(),
         "expected runtime health artifact path on rejected override launch");
  Expect(!report.runtime_health_trace_jsonl_path.empty(),
         "expected runtime health trace path on rejected override launch");
  Expect(!report.runtime_recovery_plan_path.empty(),
         "expected runtime recovery plan path on rejected override launch");
  Expect(!report.runtime_recovery_actions_jsonl_path.empty(),
         "expected runtime recovery actions trace path on rejected override launch");
  Expect(!report.runtime_health_replay_json_path.empty(),
         "expected runtime health replay path on rejected override launch");
  Expect(!report.runtime_diagnostic_replay_json_path.empty(),
         "expected diagnostic replay artifact path on rejected override launch");
  Expect(!report.runtime_diagnostic_trace_index_path.empty(),
         "expected diagnostic trace index path on rejected override launch");
  Expect(!report.runtime_diagnostic_events_jsonl_path.empty(),
         "expected diagnostic events path on rejected override launch");
  Expect(report.runtime_health_ready,
         "expected runtime health readiness on rejected override launch");
  Expect(report.runtime_diagnostic_replay_ready,
         "expected diagnostic replay readiness on rejected override launch");
  Expect(report.runtime_trace_bundle_complete,
         "expected complete replay trace bundle on rejected override launch");
  Expect(report.runtime_canonical_trace_source_count == 7,
         "expected seven canonical trace sources on rejected override launch");
  Expect(report.runtime_trace_sources_found == 7,
         "expected full trace-source coverage on rejected override launch");
  Expect(report.runtime_missing_trace_source_count == 0,
         "expected no missing trace sources on rejected override launch");
  Expect(!report.runtime_dependency_blocked,
         "expected no blocked runtime dependency on rejected override launch");
  Expect(report.runtime_failing_subsystem_count == 0,
         "expected no failing runtime subsystems on rejected override launch");
  Expect(report.runtime_recovery_actions_selected == 0,
         "expected no selected recovery actions on rejected override launch");
  Expect(report.runtime_canonical_recovery_scenario_count == 4,
         "expected four canonical recovery scenarios on rejected override launch");
  Expect(report.output.find("\"execution_succeeded\": true") !=
             std::string::npos,
         "expected underlying bootstrap execution success in rejection output");
  Expect(report.output.find("fixture-only") != std::string::npos,
         "expected fixture-only explanation in rejection output");

  fs::remove_all(fixture.root);
}

void TestNativeRuntimeLaunchReportsNonCandidateFailureHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-runtime-launch-noncandidate", true, true);
  const fs::path native_root = fixture.root / "native";

  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());
  ScopedEnvironmentVariable native_root_override("LINUXOID_NATIVE_SPIKE_ROOT",
                                                 native_root.string());

  const auto report = wfa::LaunchInstalledAppWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = fixture.package_name},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error(
            "native launch should not shell out through runtime bridge runner");
      });

  Expect(!report.launch_ok,
         "expected non-candidate native launch to fail honestly");
  Expect(report.output.find("native spike candidate") != std::string::npos,
         "expected native candidate failure reason in launch output");
  Expect(report.bootstrap_manifest_path.empty(),
         "expected no bootstrap manifest for non-candidate launch failure");
  Expect(report.runtime_health_json_path.empty(),
         "expected no runtime health artifact for non-candidate launch failure");

  fs::remove_all(fixture.root);
}

void TestNativeRuntimeLaunchSurfacesBlockedSubsystemsWithoutHostArt() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-runtime-launch-host-art-missing");
  const fs::path native_root = fixture.root / "native";

  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());
  ScopedEnvironmentVariable native_root_override("LINUXOID_NATIVE_SPIKE_ROOT",
                                                 native_root.string());

  const auto report = wfa::LaunchInstalledAppWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = fixture.package_name},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error(
            "native launch should not shell out through runtime bridge runner");
      });

  Expect(!report.launch_ok,
         "expected native launch failure when host ART is unavailable");
  Expect(report.launch_classification == "native_bootstrap_execution_failed",
         "expected bootstrap execution failure classification without host ART");
  Expect(report.art_runtime_probe_source == "missing",
         "expected missing runtime probe source without host ART");
  Expect(!report.bootstrap_manifest_path.empty(),
         "expected bootstrap manifest path without host ART");
  Expect(!report.bootstrap_execution_result_path.empty(),
         "expected bootstrap execution result path without host ART");
  Expect(!report.runtime_health_json_path.empty(),
         "expected runtime health artifact path without host ART");
  Expect(!report.runtime_health_trace_jsonl_path.empty(),
         "expected runtime health trace path without host ART");
  Expect(!report.runtime_recovery_plan_path.empty(),
         "expected runtime recovery plan path without host ART");
  Expect(!report.runtime_recovery_actions_jsonl_path.empty(),
         "expected runtime recovery actions trace path without host ART");
  Expect(!report.runtime_health_replay_json_path.empty(),
         "expected runtime health replay path without host ART");
  Expect(!report.runtime_diagnostic_replay_json_path.empty(),
         "expected diagnostic replay artifact path without host ART");
  Expect(!report.runtime_diagnostic_trace_index_path.empty(),
         "expected diagnostic trace index path without host ART");
  Expect(!report.runtime_diagnostic_events_jsonl_path.empty(),
         "expected diagnostic events path without host ART");
  Expect(report.runtime_health_ready,
         "expected runtime health artifact generation without host ART");
  Expect(report.runtime_dependency_blocked,
         "expected runtime dependency block without host ART");
  Expect(report.runtime_diagnostic_replay_ready,
         "expected diagnostic replay readiness without host ART");
  Expect(report.runtime_trace_bundle_complete,
         "expected complete replay trace bundle without host ART");
  Expect(report.runtime_canonical_trace_source_count == 7,
         "expected seven canonical trace sources without host ART");
  Expect(report.runtime_trace_sources_found == 7,
         "expected full trace-source coverage without host ART");
  Expect(report.runtime_missing_trace_source_count == 0,
         "expected no missing trace sources without host ART");
  Expect(report.runtime_failing_subsystem_count == 2,
         "expected dex and bootstrap execution to remain blocked without host ART");
  Expect(report.runtime_recovery_actions_selected == 2,
         "expected two selected recovery actions without host ART");
  Expect(std::find(report.runtime_selected_recovery_actions.begin(),
                   report.runtime_selected_recovery_actions.end(),
                   "dex_classloader_readiness=>attempt_host_art_class_resolution") !=
             report.runtime_selected_recovery_actions.end(),
         "expected dex recovery action without host ART");
  Expect(std::find(report.runtime_selected_recovery_actions.begin(),
                   report.runtime_selected_recovery_actions.end(),
                   "bootstrap_execution_readiness=>attempt_host_bootstrap_execution") !=
             report.runtime_selected_recovery_actions.end(),
         "expected bootstrap execution recovery action without host ART");
  Expect(report.runtime_canonical_recovery_scenario_count == 4,
         "expected four canonical recovery scenarios without host ART");
  Expect(std::find(report.runtime_canonical_recovery_scenarios.begin(),
                   report.runtime_canonical_recovery_scenarios.end(),
                   "failed_native_load=>retry_native_load_after_bundle_refresh") !=
             report.runtime_canonical_recovery_scenarios.end(),
         "expected failed-native-load canonical recovery scenario without host ART");
  Expect(std::find(report.runtime_failing_subsystems.begin(),
                   report.runtime_failing_subsystems.end(),
                   "dex_classloader_readiness") !=
             report.runtime_failing_subsystems.end(),
         "expected dex classloader failure without host ART");
  Expect(std::find(report.runtime_failing_subsystems.begin(),
                   report.runtime_failing_subsystems.end(),
                   "bootstrap_execution_readiness") !=
             report.runtime_failing_subsystems.end(),
         "expected bootstrap execution failure without host ART");

  fs::remove_all(fixture.root);
}

void TestNativeRuntimeLaunchReportsHostAppProcessCapabilityHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-runtime-launch-app-process");
  const fs::path native_root = fixture.root / "native";
  const fs::path app_process = fixture.root / "app_process";
  {
    std::ofstream output(app_process);
    output << "#!/bin/sh\n";
    output << "printf '%s\\n' 'host-app-process'\n";
    output << "exit 0\n";
  }
  fs::permissions(app_process,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);

  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());
  ScopedEnvironmentVariable native_root_override("LINUXOID_NATIVE_SPIKE_ROOT",
                                                 native_root.string());
  ScopedEnvironmentVariable path_override("PATH", fixture.root.string());

  const auto report = wfa::LaunchInstalledAppWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = fixture.package_name},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error(
            "native launch should not shell out through runtime bridge runner");
      });

  Expect(!report.launch_ok,
         "expected native launch to stay blocked with app_process-only host probe");
  Expect(report.art_runtime_probe_source == "host",
         "expected host probe source in native launch");
  Expect(report.runtime_probe_detection_reason == "host_app_process_selected",
         "expected app_process detection reason in native launch");
  Expect(report.art_runtime_probe_capability ==
             "host_app_process_detection_only",
         "expected app_process capability in native launch");
  const auto rendered = wfa::RenderInstalledAppLaunchReport(report);
  Expect(rendered.find(
             "ART Runtime Probe Capability: host_app_process_detection_only") !=
             std::string::npos,
         "expected app_process capability line in native launch render");

  fs::remove_all(fixture.root);
}

void TestNativeRuntimeLaunchSurfacesProbeInventoryAndReason() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-runtime-launch-probe-inventory");
  const fs::path native_root = fixture.root / "native";

  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());
  ScopedEnvironmentVariable native_root_override("LINUXOID_NATIVE_SPIKE_ROOT",
                                                 native_root.string());
  ScopedEnvironmentVariable disable_host_art(
      "LINUXOID_DISABLE_HOST_ART_RUNTIME_PROBE", "1");

  const auto report = wfa::LaunchInstalledAppWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = fixture.package_name},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error(
            "native launch should not shell out through runtime bridge runner");
      });

  Expect(!report.runtime_probe_inventory_json_path.empty(),
         "expected probe inventory path in native launch report");
  Expect(fs::exists(report.runtime_probe_inventory_json_path),
         "expected probe inventory artifact in native launch report");
  Expect(report.runtime_probe_detection_reason == "host_probe_disabled",
         "expected disabled-host probe detection reason in native launch");
  const auto rendered = wfa::RenderInstalledAppLaunchReport(report);
  Expect(rendered.find("ART Runtime Probe Inventory Path: ") !=
             std::string::npos,
         "expected probe inventory path line in native launch render");
  Expect(rendered.find("ART Runtime Probe Detection Reason: "
                       "host_probe_disabled") != std::string::npos,
         "expected probe detection reason line in native launch render");

  fs::remove_all(fixture.root);
}

void TestDesktopLaunchArtifactsForImeApp() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "wfa desktop test";
  fs::remove_all(root);
  fs::create_directories(root);

  const fs::path compatctl_path = root / "compatctl";
  const fs::path apk_path = root / "keyboard.apk";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }
  {
    std::ofstream apk_output(apk_path);
    apk_output << "placeholder apk\n";
  }

  const wfa::DesktopLaunchSpec spec{
      .app_name = "FUTO Keyboard",
      .serial = "emulator-5590",
      .apk_path = apk_path.string(),
      .package_name = "org.futo.inputmethod.latin",
      .launcher_component =
          "org.futo.inputmethod.latin/.uix.settings.SettingsActivity",
      .ime_component = "org.futo.inputmethod.latin/.LatinIME",
      .compatctl_path = compatctl_path.string(),
      .desktop_root = root.string(),
  };

  const auto artifacts = wfa::CreateDesktopLaunchArtifacts(spec);

  Expect(artifacts.uses_provision_mode,
         "expected IME apps to use provision mode");
  Expect(artifacts.host_launch_ready,
         "expected desktop artifacts to be ready");
  Expect(fs::exists(artifacts.desktop_file_path),
         "expected desktop file to exist");
  Expect(fs::exists(artifacts.script_path),
         "expected launcher script to exist");

  std::ifstream script_input(artifacts.script_path);
  std::string script((std::istreambuf_iterator<char>(script_input)),
                     std::istreambuf_iterator<char>());
  Expect(script.find("provision-ime") != std::string::npos,
         "expected script to use provision-ime");
  Expect(script.find("org.futo.inputmethod.latin/.LatinIME") !=
             std::string::npos,
         "expected script to include ime component");

  std::ifstream desktop_input(artifacts.desktop_file_path);
  std::string desktop_entry((std::istreambuf_iterator<char>(desktop_input)),
                            std::istreambuf_iterator<char>());
  Expect(desktop_entry.find("Name=FUTO Keyboard (Android)") !=
             std::string::npos,
         "expected desktop entry name");
  Expect(desktop_entry.find("Exec=\"" + artifacts.script_path + "\"") !=
             std::string::npos,
         "expected quoted desktop entry exec path");

  fs::remove_all(root);
}

void TestDesktopLaunchArtifactsForLoadedApkUseStagedPath() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "wfa-desktop-loaded-test";
  fs::remove_all(root);
  fs::create_directories(root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1");

  const fs::path compatctl_path = root / "compatctl";
  const fs::path staged_apk_path =
      root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1/base.apk";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }
  {
    std::ofstream staged_apk_output(staged_apk_path);
    staged_apk_output << "staged apk\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/original.apk",
      .install_id = "vc1",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "keyboard.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 1,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "org.futo.inputmethod.latin",
          .launcher_activity_name =
              "org.futo.inputmethod.latin/.uix.settings.SettingsActivity",
          .input_method_service_name = "org.futo.inputmethod.latin/.LatinIME",
          .declared_components =
              {"org.futo.inputmethod.latin.uix.settings.SettingsActivity",
               "org.futo.inputmethod.latin.LatinIME"},
          .declared_activity_components =
              {"org.futo.inputmethod.latin.uix.settings.SettingsActivity"},
          .has_launcher_activity = true,
          .has_input_method_service = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "org.futo.inputmethod.latin",
          .app_profile = "input_method",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "POST_P6_IME",
      },
      .layout = wfa::BuildPackageLayout(
          {.package_name = "org.futo.inputmethod.latin",
           .install_id = "vc1",
           .version_code = 1},
          (root / "compat").string()),
      .install_root =
          (root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1")
              .string(),
  };

  const auto artifacts = wfa::CreateDesktopLaunchArtifactsForLoadedApk(
      report, "emulator-5590",
      "org.futo.inputmethod.latin/.uix.settings.SettingsActivity",
      compatctl_path.string(), root.string());

  std::ifstream script_input(artifacts.script_path);
  std::string script((std::istreambuf_iterator<char>(script_input)),
                     std::istreambuf_iterator<char>());
  Expect(script.find(staged_apk_path.string()) != std::string::npos,
         "expected staged apk path in generated script");
  Expect(script.find("/tmp/original.apk") == std::string::npos,
         "did not expect original apk path in generated script");

  fs::remove_all(root);
}

void TestDesktopLaunchArtifactsRejectCrossPackageComponent() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "wfa-cross-package-test";
  fs::remove_all(root);
  fs::create_directories(root);

  const fs::path compatctl_path = root / "compatctl";
  const fs::path apk_path = root / "keyboard.apk";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }
  {
    std::ofstream apk_output(apk_path);
    apk_output << "placeholder apk\n";
  }

  bool threw = false;
  try {
    (void)wfa::CreateDesktopLaunchArtifacts(wfa::DesktopLaunchSpec{
        .app_name = "FUTO Keyboard",
        .serial = "emulator-5590",
        .apk_path = apk_path.string(),
        .package_name = "org.futo.inputmethod.latin",
        .launcher_component = "com.example.other/.SettingsActivity",
        .ime_component = "org.futo.inputmethod.latin/.LatinIME",
        .compatctl_path = compatctl_path.string(),
        .desktop_root = root.string(),
    });
  } catch (const std::invalid_argument&) {
    threw = true;
  }

  Expect(threw, "expected cross-package launcher component rejection");
  fs::remove_all(root);
}

void TestDesktopLaunchArtifactsRejectUnknownDeclaredComponent() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "wfa-unknown-component-test";
  fs::remove_all(root);
  fs::create_directories(root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1");

  const fs::path compatctl_path = root / "compatctl";
  const fs::path staged_apk_path =
      root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1/base.apk";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }
  {
    std::ofstream staged_apk_output(staged_apk_path);
    staged_apk_output << "staged apk\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/original.apk",
      .install_id = "vc1",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "keyboard.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 1,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "org.futo.inputmethod.latin",
          .launcher_activity_name =
              "org.futo.inputmethod.latin/.uix.settings.SettingsActivity",
          .input_method_service_name = "org.futo.inputmethod.latin/.LatinIME",
          .declared_components =
              {"org.futo.inputmethod.latin.uix.settings.SettingsActivity",
               "org.futo.inputmethod.latin.LatinIME"},
          .declared_activity_components =
              {"org.futo.inputmethod.latin.uix.settings.SettingsActivity"},
          .has_launcher_activity = true,
          .has_input_method_service = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "org.futo.inputmethod.latin",
          .app_profile = "input_method",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "POST_P6_IME",
      },
      .layout = wfa::BuildPackageLayout(
          {.package_name = "org.futo.inputmethod.latin",
           .install_id = "vc1",
           .version_code = 1},
          (root / "compat").string()),
      .install_root =
          (root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1")
              .string(),
  };

  bool threw = false;
  try {
    (void)wfa::CreateDesktopLaunchArtifactsForLoadedApk(
        report, "emulator-5590", "org.futo.inputmethod.latin/.DoesNotExist",
        compatctl_path.string(), root.string());
  } catch (const std::invalid_argument&) {
    threw = true;
  }

  Expect(threw, "expected unknown same-package component rejection");
  fs::remove_all(root);
}

void TestDesktopLaunchArtifactsRejectServiceLaunchTarget() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "wfa-service-target-test";
  fs::remove_all(root);
  fs::create_directories(root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1");

  const fs::path compatctl_path = root / "compatctl";
  const fs::path staged_apk_path =
      root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1/base.apk";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }
  {
    std::ofstream staged_apk_output(staged_apk_path);
    staged_apk_output << "staged apk\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/original.apk",
      .install_id = "vc1",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "keyboard.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 1,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "org.futo.inputmethod.latin",
          .launcher_activity_name =
              "org.futo.inputmethod.latin/.uix.settings.SettingsActivity",
          .input_method_service_name = "org.futo.inputmethod.latin/.LatinIME",
          .declared_components =
              {"org.futo.inputmethod.latin.uix.settings.SettingsActivity",
               "org.futo.inputmethod.latin.SyncService"},
          .declared_activity_components =
              {"org.futo.inputmethod.latin.uix.settings.SettingsActivity"},
          .has_launcher_activity = true,
          .has_input_method_service = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "org.futo.inputmethod.latin",
          .app_profile = "input_method",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "POST_P6_IME",
      },
      .layout = wfa::BuildPackageLayout(
          {.package_name = "org.futo.inputmethod.latin",
           .install_id = "vc1",
           .version_code = 1},
          (root / "compat").string()),
      .install_root =
          (root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1")
              .string(),
  };

  bool threw = false;
  try {
    (void)wfa::CreateDesktopLaunchArtifactsForLoadedApk(
        report, "emulator-5590", "org.futo.inputmethod.latin/.SyncService",
        compatctl_path.string(), root.string());
  } catch (const std::invalid_argument&) {
    threw = true;
  }

  Expect(threw, "expected service launch target rejection");
  fs::remove_all(root);
}

void TestAutoDesktopLaunchArtifactsInferLauncherAndSplitRoots() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "wfa-auto-desktop-test";
  const fs::path desktop_root = root / "applications";
  const fs::path launcher_root = root / "launchers";
  fs::remove_all(root);
  fs::create_directories(root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1");

  const fs::path compatctl_path = root / "compatctl";
  const fs::path staged_apk_path =
      root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1/base.apk";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }
  {
    std::ofstream staged_apk_output(staged_apk_path);
    staged_apk_output << "staged apk\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/original.apk",
      .install_id = "vc1",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "keyboard.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 1,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "org.futo.inputmethod.latin",
          .launcher_activity_name =
              "org.futo.inputmethod.latin/.uix.settings.SettingsActivity",
          .input_method_service_name = "org.futo.inputmethod.latin/.LatinIME",
          .declared_components =
              {"org.futo.inputmethod.latin.uix.settings.SettingsActivity",
               "org.futo.inputmethod.latin.LatinIME"},
          .declared_activity_components =
              {"org.futo.inputmethod.latin.uix.settings.SettingsActivity"},
          .has_launcher_activity = true,
          .has_input_method_service = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "org.futo.inputmethod.latin",
          .app_profile = "input_method",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "POST_P6_IME",
      },
      .layout = wfa::BuildPackageLayout(
          {.package_name = "org.futo.inputmethod.latin",
           .install_id = "vc1",
           .version_code = 1},
          (root / "compat").string()),
      .install_root =
          (root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1")
              .string(),
  };

  const auto artifacts = wfa::CreateDesktopLaunchArtifactsForLoadedApkAuto(
      report, "emulator-5590", compatctl_path.string(), desktop_root.string(),
      launcher_root.string());

  Expect(artifacts.launcher_component ==
             "org.futo.inputmethod.latin/.uix.settings.SettingsActivity",
         "expected inferred launcher activity");
  Expect(fs::exists(artifacts.desktop_file_path),
         "expected auto desktop file to exist");
  Expect(fs::exists(artifacts.script_path),
         "expected auto launcher script to exist");
  Expect(artifacts.desktop_file_path.find(desktop_root.string()) == 0,
         "expected desktop file under desktop-entry root");
  Expect(artifacts.script_path.find(launcher_root.string()) == 0,
         "expected launcher script under launcher root");

  std::ifstream desktop_input(artifacts.desktop_file_path);
  std::string desktop_entry((std::istreambuf_iterator<char>(desktop_input)),
                            std::istreambuf_iterator<char>());
  Expect(desktop_entry.find("Exec=\"" + artifacts.script_path + "\"") !=
             std::string::npos,
         "expected desktop entry to point at split-root launcher");

  fs::remove_all(root);
}

void TestAutoDesktopLaunchArtifactsRejectHeadlessApp() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "wfa-headless-auto-test";
  fs::remove_all(root);
  fs::create_directories(root / "compat/users/0/packages/com.example.headless/vc1");

  const fs::path compatctl_path = root / "compatctl";
  const fs::path staged_apk_path =
      root / "compat/users/0/packages/com.example.headless/vc1/base.apk";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }
  {
    std::ofstream staged_apk_output(staged_apk_path);
    staged_apk_output << "staged apk\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/headless.apk",
      .install_id = "vc1",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "headless.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 1,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.headless",
          .declared_components = {"com.example.headless.SyncService"},
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.headless",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6_EXPLICIT_COMPONENT",
          .earliest_full_use_phase = "POST_P6_ADVANCED_RUNTIME",
      },
      .layout = wfa::BuildPackageLayout(
          {.package_name = "com.example.headless",
           .install_id = "vc1",
           .version_code = 1},
          (root / "compat").string()),
      .install_root =
          (root / "compat/users/0/packages/com.example.headless/vc1").string(),
  };

  bool threw = false;
  try {
    (void)wfa::CreateDesktopLaunchArtifactsForLoadedApkAuto(
        report, "emulator-5590", compatctl_path.string(), root.string(),
        (root / "launchers").string());
  } catch (const std::invalid_argument&) {
    threw = true;
  }

  Expect(threw, "expected headless apps to reject automatic launcher inference");
  fs::remove_all(root);
}

void TestWaydroidAppLaunchReportRendering() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command == "waydroid app launch com.android.calculator2") {
      return {0, ""};
    }
    throw std::runtime_error("unexpected command in waydroid launch test");
  };

  const auto report =
      wfa::LaunchWaydroidAppWithRunner("com.android.calculator2", runner);
  Expect(report.launch_ok, "expected waydroid app launch success");

  const auto rendered = wfa::RenderWaydroidAppLaunchReport(report);
  Expect(rendered.find("Package: com.android.calculator2") !=
             std::string::npos,
         "expected package in waydroid launch report");
  Expect(rendered.find("Launch OK: yes") != std::string::npos,
         "expected launch success in waydroid launch report");
}

void TestInstalledAppLaunchReportRendering() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command == "waydroid app launch com.android.calculator2") {
      return {0, ""};
    }
    throw std::runtime_error("unexpected command in installed app launch test");
  };

  const auto report = wfa::LaunchInstalledAppWithRunner(
      {.backend = wfa::RuntimeBackendKind::kWaydroid,
       .package_name = "com.android.calculator2"},
      runner);
  Expect(report.launch_ok, "expected generic installed app launch success");

  const auto rendered = wfa::RenderInstalledAppLaunchReport(report);
  Expect(rendered.find("Runtime Backend: waydroid") != std::string::npos,
         "expected backend name in installed app launch report");
  Expect(rendered.find("Package: com.android.calculator2") !=
             std::string::npos,
         "expected package in installed app launch report");
}

void TestInstalledAppLaunchReportRenderingIncludesRuntimeTraceBundle() {
  wfa::InstalledAppLaunchReport report;
  report.backend_name = "native";
  report.serial = "linuxoid-native";
  report.package_name = "com.example.demo";
  report.component = "com.example.demo/.MainActivity";
  report.launch_classification = "native_bootstrap_execution_failed";
  report.runtime_health_json_path = "/tmp/linuxoid/health/runtime-health.json";
  report.runtime_health_trace_jsonl_path =
      "/tmp/linuxoid/health/runtime-health-trace.jsonl";
  report.runtime_recovery_plan_path =
      "/tmp/linuxoid/health/runtime-recovery-plan.json";
  report.runtime_recovery_actions_jsonl_path =
      "/tmp/linuxoid/health/runtime-recovery-actions.jsonl";
  report.runtime_health_replay_json_path =
      "/tmp/linuxoid/health/runtime-health-replay.json";
  report.runtime_diagnostic_replay_json_path =
      "/tmp/linuxoid/health/runtime-diagnostic-replay.json";
  report.runtime_diagnostic_trace_index_path =
      "/tmp/linuxoid/health/runtime-diagnostic-trace-index.json";
  report.runtime_diagnostic_events_jsonl_path =
      "/tmp/linuxoid/health/runtime-diagnostic-events.jsonl";
  report.runtime_diagnostic_replay_ready = true;
  report.runtime_trace_bundle_complete = true;
  report.runtime_canonical_trace_source_count = 7;
  report.runtime_trace_sources_found = 7;
  report.runtime_missing_trace_source_count = 0;

  const auto rendered = wfa::RenderInstalledAppLaunchReport(report);
  Expect(rendered.find("Runtime Health Trace Path: "
                       "/tmp/linuxoid/health/runtime-health-trace.jsonl") !=
             std::string::npos,
         "expected runtime health trace path in installed app launch report");
  Expect(rendered.find(
             "Runtime Recovery Actions Trace Path: "
             "/tmp/linuxoid/health/runtime-recovery-actions.jsonl") !=
             std::string::npos,
         "expected recovery actions trace path in installed app launch report");
  Expect(rendered.find("Runtime Diagnostic Events Path: "
                       "/tmp/linuxoid/health/runtime-diagnostic-events.jsonl") !=
             std::string::npos,
         "expected diagnostic events path in installed app launch report");
  Expect(rendered.find("Runtime Trace Bundle Complete: yes") !=
             std::string::npos,
         "expected trace bundle completeness in installed app launch report");
  Expect(rendered.find("Runtime Canonical Trace Source Count: 7") !=
             std::string::npos,
         "expected canonical trace source count in installed app launch report");
}

void TestAttachedAdbInstalledAppLaunchUsesExplicitComponent() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command == "adb -s 'device-01' shell am start -W -n "
                   "'com.example.demo/.MainActivity'") {
      return {0,
              "Status: ok\n"
              "Activity: com.example.demo/.MainActivity\n"
              "cmp=com.example.demo/.MainActivity\n"
              "Complete\n"};
    }
    throw std::runtime_error(
        "unexpected command in attached-adb installed app launch test");
  };

  const auto report = wfa::LaunchInstalledAppWithRunner(
      {.backend = wfa::RuntimeBackendKind::kAttachedAdb,
       .serial = "device-01",
       .package_name = "com.example.demo",
       .component = "com.example.demo/.MainActivity"},
      runner);

  Expect(report.launch_ok, "expected attached-adb launch success");
  Expect(report.backend_name == "attached-adb",
         "expected attached-adb backend name");
}

void TestAttachedAdbInstalledAppLaunchAutoResolvesComponent() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command ==
        "timeout 5s adb -s 'device-01' shell cmd package resolve-activity --brief 'com.example.demo'") {
      return {0,
              "priority=0 preferredOrder=0 match=0x108000 specificIndex=-1 isDefault=true\n"
              "com.example.demo/.MainActivity\n"};
    }
    if (command == "adb -s 'device-01' shell am start -W -n "
                   "'com.example.demo/.MainActivity'") {
      return {0,
              "Status: ok\n"
              "Activity: com.example.demo/.MainActivity\n"
              "cmp=com.example.demo/.MainActivity\n"
              "Complete\n"};
    }
    throw std::runtime_error(
        "unexpected command in attached-adb auto-resolve launch test");
  };

  const auto report = wfa::LaunchInstalledAppWithRunner(
      {.backend = wfa::RuntimeBackendKind::kAttachedAdb,
       .serial = "device-01",
       .package_name = "com.example.demo"},
      runner);

  Expect(report.launch_ok, "expected attached-adb auto-resolved launch success");
  Expect(report.component == "com.example.demo/.MainActivity",
         "expected attached-adb launch to expose resolved component");
}

void TestAttachedAdbPackageMetadataLookupExtractsLauncherAndVersion() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command ==
        "timeout 5s adb -s 'device-01' shell pm list packages 'com.example.demo'") {
      return {0, "package:com.example.demo\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell cmd package resolve-activity --brief 'com.example.demo'") {
      return {0,
              "priority=0 preferredOrder=0 match=0x108000 specificIndex=-1 isDefault=true\n"
              "com.example.demo/.MainActivity\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell pm path 'com.example.demo'") {
      return {0, "package:/data/app/~~demo/base.apk\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell dumpsys package 'com.example.demo'") {
      return {0,
              "Packages:\n"
              "  Package [com.example.demo] (123abc):\n"
              "    versionCode=42 minSdk=24 targetSdk=35\n"
              "    versionName=1.0.0\n"};
    }
    throw std::runtime_error(
        "unexpected command in attached-adb metadata lookup test");
  };

  const auto report = wfa::QueryInstalledPackageMetadataWithRunner(
      {.backend = wfa::RuntimeBackendKind::kAttachedAdb,
       .serial = "device-01",
       .package_name = "com.example.demo"},
      runner);

  Expect(report.package_visible, "expected package visibility");
  Expect(report.launcher_resolved, "expected launcher resolution");
  Expect(report.resolved_component == "com.example.demo/.MainActivity",
         "expected resolved launcher component");
  Expect(report.install_path == "/data/app/~~demo/base.apk",
         "expected install path extraction");
  Expect(report.version_code == "42", "expected version code extraction");
  Expect(report.version_name == "1.0.0", "expected version name extraction");

  const auto rendered = wfa::RenderInstalledPackageMetadataReport(report);
  Expect(rendered.find("Resolved Component: com.example.demo/.MainActivity") !=
             std::string::npos,
         "expected resolved component in metadata report");
  Expect(rendered.find("Version Name: 1.0.0") != std::string::npos,
         "expected version name in metadata report");
}

void TestAttachedAdbRuntimeDiscoveryParsesTargets() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command == "timeout 5s adb devices") {
      return {0,
              "List of devices attached\n"
              "device-01\tdevice\n"
              "device-02\toffline\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell getprop 'ro.product.model'") {
      return {0, "Pixel 7\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell getprop 'ro.build.version.release'") {
      return {0, "14\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell getprop 'ro.product.cpu.abi'") {
      return {0, "x86_64\n"};
    }
    throw std::runtime_error("unexpected command in adb discovery test");
  };

  const auto report = wfa::DiscoverRuntimeTargetsWithRunner(
      wfa::RuntimeBackendKind::kAttachedAdb, runner);

  Expect(report.backend_available, "expected adb backend to be available");
  Expect(report.targets.size() == 2, "expected two discovered targets");
  Expect(report.targets[0].serial == "device-01",
         "expected first serial to match");
  Expect(report.targets[0].online, "expected first target to be online");
  Expect(report.targets[0].model == "Pixel 7",
         "expected first target model");
  Expect(report.targets[0].android_release == "14",
         "expected first target Android release");
  Expect(report.targets[1].serial == "device-02",
         "expected second serial to match");
  Expect(!report.targets[1].online,
         "expected second target to be offline");
}

void TestAttachedAdbPreflightAutoSelectsSingleTarget() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command == "timeout 5s adb devices") {
      return {0,
              "List of devices attached\n"
              "device-01\tdevice\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell getprop 'ro.product.model'") {
      return {0, "Pixel 7\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell getprop 'ro.build.version.release'") {
      return {0, "14\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell getprop 'ro.product.cpu.abi'") {
      return {0, "x86_64\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell pm list packages 'com.example.demo'") {
      return {0, "package:com.example.demo\n"};
    }
    throw std::runtime_error("unexpected command in adb preflight success test");
  };

  const auto report = wfa::PreflightRuntimeWithRunner(
      {.backend = wfa::RuntimeBackendKind::kAttachedAdb,
       .package_name = "com.example.demo",
       .component = "com.example.demo/.MainActivity"},
      runner);

  Expect(report.target_selected, "expected preflight to select the only target");
  Expect(report.target_online, "expected selected target to be online");
  Expect(report.package_visible, "expected package to be visible");
  Expect(report.component_ready, "expected component readiness");
  Expect(report.ready_for_launch, "expected ready-for-launch success");
  Expect(report.serial == "device-01",
         "expected auto-selected serial in preflight report");
}

void TestAttachedAdbPreflightResolvesComponentWhenOmitted() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command == "timeout 5s adb devices") {
      return {0,
              "List of devices attached\n"
              "device-01\tdevice\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell getprop 'ro.product.model'") {
      return {0, "Pixel 7\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell getprop 'ro.build.version.release'") {
      return {0, "14\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell getprop 'ro.product.cpu.abi'") {
      return {0, "x86_64\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell pm list packages 'com.example.demo'") {
      return {0, "package:com.example.demo\n"};
    }
    if (command ==
        "timeout 5s adb -s 'device-01' shell cmd package resolve-activity --brief 'com.example.demo'") {
      return {0,
              "priority=0 preferredOrder=0 match=0x108000 specificIndex=-1 isDefault=true\n"
              "com.example.demo/.MainActivity\n"};
    }
    throw std::runtime_error(
        "unexpected command in adb preflight auto-resolve test");
  };

  const auto report = wfa::PreflightRuntimeWithRunner(
      {.backend = wfa::RuntimeBackendKind::kAttachedAdb,
       .package_name = "com.example.demo"},
      runner);

  Expect(report.target_selected, "expected preflight to select the only target");
  Expect(report.package_visible, "expected package to be visible");
  Expect(report.component_ready, "expected component readiness after resolution");
  Expect(report.component == "com.example.demo/.MainActivity",
         "expected resolved component in preflight report");
  Expect(report.ready_for_launch,
         "expected ready-for-launch success after resolution");
}

void TestAttachedAdbPreflightRequiresSerialWhenMultipleTargetsExist() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command == "timeout 5s adb devices") {
      return {0,
              "List of devices attached\n"
              "device-01\tdevice\n"
              "device-02\tdevice\n"};
    }
    if (command.find("timeout 5s adb -s") != std::string::npos &&
        command.find("getprop") != std::string::npos) {
      return {0, "value\n"};
    }
    throw std::runtime_error(
        "unexpected command in adb preflight multi-target test");
  };

  const auto report = wfa::PreflightRuntimeWithRunner(
      {.backend = wfa::RuntimeBackendKind::kAttachedAdb,
       .package_name = "com.example.demo",
       .component = "com.example.demo/.MainActivity"},
      runner);

  Expect(!report.target_selected,
         "expected preflight to reject multiple auto-select candidates");
  Expect(!report.ready_for_launch,
         "expected multi-target preflight to be not ready");
  Expect(report.notes.find("multiple online targets") != std::string::npos,
         "expected multi-target note");
}

void TestAttachedAdbDiscoveryTimeoutReturnsUnavailable() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command == "timeout 5s adb devices") {
      return {124, ""};
    }
    throw std::runtime_error("unexpected command in adb timeout test");
  };

  const auto report = wfa::DiscoverRuntimeTargetsWithRunner(
      wfa::RuntimeBackendKind::kAttachedAdb, runner);

  Expect(!report.backend_available,
         "expected timed-out adb discovery to report unavailable");
  Expect(report.backend_check_output.find("Timed out") != std::string::npos,
         "expected timeout note in discovery output");
}

void TestNativeRuntimePreflightReportsMissingStagedPackageHonestly() {
  const auto runner = [](const std::string&) -> wfa::CommandResult {
    throw std::runtime_error("native preflight should not execute commands");
  };

  const auto report = wfa::PreflightRuntimeWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = "com.example.demo"},
      runner);

  Expect(report.backend_available,
         "expected native backend availability");
  Expect(report.target_discovered,
         "expected native target discovery");
  Expect(report.target_selected,
         "expected native target selection");
  Expect(!report.ready_for_launch,
         "expected native preflight to stay unready without staged package");
  Expect(report.notes.find("not staged") != std::string::npos,
         "expected missing staged-package note");
}

void TestWaydroidDesktopLaunchArtifacts() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "linuxoid-waydroid-launch";
  const fs::path desktop_root = root / "applications";
  const fs::path launcher_root = root / "launchers";
  fs::remove_all(root);
  fs::create_directories(root);

  const fs::path compatctl_path = root / "compatctl";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }

  const auto artifacts = wfa::CreateWaydroidDesktopLaunchArtifacts(
      {.app_name = "Calculator",
       .package_name = "com.android.calculator2",
       .compatctl_path = compatctl_path.string(),
       .desktop_root = desktop_root.string(),
       .launcher_root = launcher_root.string()});

  Expect(artifacts.host_launch_ready,
         "expected waydroid desktop artifacts to be ready");
  Expect(fs::exists(artifacts.script_path),
         "expected waydroid launcher script to exist");
  Expect(fs::exists(artifacts.desktop_file_path),
         "expected waydroid desktop entry to exist");

  std::ifstream script_input(artifacts.script_path);
  std::string script((std::istreambuf_iterator<char>(script_input)),
                     std::istreambuf_iterator<char>());
  Expect(script.find("launch-package") != std::string::npos,
         "expected script to use generic launch command");
  Expect(script.find("waydroid") != std::string::npos,
         "expected script to keep waydroid backend identity");
  Expect(script.find("com.android.calculator2") != std::string::npos,
         "expected script to include package name");

  std::ifstream desktop_input(artifacts.desktop_file_path);
  std::string desktop_entry((std::istreambuf_iterator<char>(desktop_input)),
                            std::istreambuf_iterator<char>());
  Expect(desktop_entry.find("Name=Calculator (Android)") !=
             std::string::npos,
         "expected waydroid desktop entry name");
  Expect(desktop_entry.find("Exec=\"" + artifacts.script_path + "\"") !=
             std::string::npos,
         "expected waydroid desktop entry exec path");

  fs::remove_all(root);
}

void TestInstalledPackageDesktopLaunchArtifactsRequireAttachedAdbFields() {
  bool missing_serial = false;
  try {
    (void)wfa::CreateInstalledPackageDesktopLaunchArtifacts(
        {.backend = wfa::RuntimeBackendKind::kAttachedAdb,
         .app_name = "Demo",
         .package_name = "com.example.demo",
         .component = "com.example.demo/.MainActivity",
         .compatctl_path = "/tmp/compatctl",
         .desktop_root = "/tmp/linuxoid-installed-desktop"});
  } catch (const std::invalid_argument&) {
    missing_serial = true;
  }
  Expect(missing_serial,
         "expected attached-adb desktop launch without serial to throw");

  bool missing_component = false;
  try {
    (void)wfa::CreateInstalledPackageDesktopLaunchArtifacts(
        {.backend = wfa::RuntimeBackendKind::kAttachedAdb,
         .app_name = "Demo",
         .serial = "device-01",
         .package_name = "com.example.demo",
         .compatctl_path = "/tmp/compatctl",
         .desktop_root = "/tmp/linuxoid-installed-desktop"});
  } catch (const std::invalid_argument&) {
    missing_component = true;
  }
  Expect(missing_component,
         "expected attached-adb desktop launch without component to throw");
}

void TestWaydroidDesktopLaunchArtifactsRejectInvalidPackage() {
  bool threw = false;
  try {
    (void)wfa::CreateWaydroidDesktopLaunchArtifacts(
        {.app_name = "Bad",
         .package_name = "invalid-package",
         .compatctl_path = "/tmp/compatctl",
         .desktop_root = "/tmp/linuxoid-invalid-desktop",
         .launcher_root = "/tmp/linuxoid-invalid-launcher"});
  } catch (const std::invalid_argument&) {
    threw = true;
  }

  Expect(threw, "expected invalid waydroid package names to be rejected");
}

void TestInstalledPackageVerificationSuccessPath() {
  const auto runtime_runner = [](const std::string& command)
      -> wfa::CommandResult {
    if (command == "timeout 5s adb devices") {
      return {0, "List of devices attached\ndevice-01\tdevice\n"};
    }
    if (command.find("adb -s 'device-01' shell getprop") != std::string::npos &&
        command.find("ro.product.model") != std::string::npos) {
      return {0, "Pixel 9\n"};
    }
    if (command.find("adb -s 'device-01' shell getprop") != std::string::npos &&
        command.find("ro.build.version.release") != std::string::npos) {
      return {0, "15\n"};
    }
    if (command.find("adb -s 'device-01' shell getprop") != std::string::npos &&
        command.find("ro.product.cpu.abi") != std::string::npos) {
      return {0, "x86_64\n"};
    }
    if (command.find(
            "adb -s 'device-01' shell pm list packages 'com.example.demo'") !=
        std::string::npos) {
      return {0, "package:com.example.demo\n"};
    }
    if (command ==
        "adb -s 'device-01' shell am start -W -n 'com.example.demo/.MainActivity'") {
      return {0,
              "Starting: Intent { cmp=com.example.demo/.MainActivity }\nStatus: ok\nComplete\n"};
    }
    throw std::runtime_error(
        "unexpected runtime command in installed package verification test: " +
        command);
  };

  const auto launcher_runner = [](const std::string& command)
      -> wfa::CommandResult {
    if (command.find("com.example.demo.sh") != std::string::npos) {
      return {0, "launcher ok\n"};
    }
    throw std::runtime_error(
        "unexpected launcher command in installed package verification test");
  };

  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "linuxoid-installed-verify";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path compatctl_path = root / "compatctl";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }

  const auto report = wfa::VerifyInstalledPackageWithRunners(
      {.backend = wfa::RuntimeBackendKind::kAttachedAdb,
       .app_name = "Demo",
       .serial = "device-01",
       .package_name = "com.example.demo",
       .component = "com.example.demo/.MainActivity",
       .compatctl_path = compatctl_path.string(),
       .desktop_root = (root / "applications").string(),
       .launcher_root = (root / "launchers").string()},
      runtime_runner, launcher_runner);

  Expect(report.direct_launch_ok,
         "expected direct installed-package launch success");
  Expect(report.launcher_generation_ok,
         "expected installed-package launcher generation success");
  Expect(report.generated_launcher_ok,
         "expected installed-package generated launcher success");
  Expect(report.backend_name == "attached-adb",
         "expected attached-adb backend name");

  const auto rendered = wfa::RenderInstalledPackageVerificationReport(report);
  Expect(rendered.find("Runtime Backend: attached-adb") != std::string::npos,
         "expected backend name in generic verification report");
  Expect(rendered.find("Runtime Target: device-01") != std::string::npos,
         "expected target label in generic verification report");
  Expect(rendered.find("Preflight OK: yes") != std::string::npos,
         "expected preflight success line in generic verification report");
  Expect(rendered.find("Package Visible: yes") != std::string::npos,
         "expected package visibility line in generic verification report");
  Expect(rendered.find("Component Ready: yes") != std::string::npos,
         "expected component readiness line in generic verification report");
  Expect(rendered.find("Verification Loading: [##########] 100/100") !=
             std::string::npos,
         "expected full generic verification loading");

  fs::remove_all(root);
}

void TestInstalledPackageMatrixSuccessPath() {
  const auto runtime_runner = [](const std::string& command)
      -> wfa::CommandResult {
    if (command == "timeout 5s adb devices") {
      return {0, "List of devices attached\ndevice-01\tdevice\n"};
    }
    if (command.find("adb -s 'device-01' shell getprop") != std::string::npos &&
        command.find("ro.product.model") != std::string::npos) {
      return {0, "Pixel 9\n"};
    }
    if (command.find("adb -s 'device-01' shell getprop") != std::string::npos &&
        command.find("ro.build.version.release") != std::string::npos) {
      return {0, "15\n"};
    }
    if (command.find("adb -s 'device-01' shell getprop") != std::string::npos &&
        command.find("ro.product.cpu.abi") != std::string::npos) {
      return {0, "x86_64\n"};
    }
    if (command ==
            "timeout 5s adb -s 'device-01' shell pm list packages 'com.example.demo'" ||
        command ==
            "timeout 5s adb -s 'device-01' shell pm list packages 'com.example.tools'") {
      if (command.find("com.example.demo") != std::string::npos) {
        return {0, "package:com.example.demo\n"};
      }
      return {0, "package:com.example.tools\n"};
    }
    if (command ==
            "adb -s 'device-01' shell am start -W -n 'com.example.demo/.MainActivity'" ||
        command ==
            "adb -s 'device-01' shell am start -W -n 'com.example.tools/.HomeActivity'") {
      if (command.find("com.example.demo/.MainActivity") != std::string::npos) {
        return {0,
                "Starting: Intent { cmp=com.example.demo/.MainActivity }\nStatus: ok\nComplete\n"};
      }
      return {0,
              "Starting: Intent { cmp=com.example.tools/.HomeActivity }\nStatus: ok\nComplete\n"};
    }
    throw std::runtime_error(
        "unexpected runtime command in installed package matrix test: " +
        command);
  };

  const auto launcher_runner = [](const std::string& command)
      -> wfa::CommandResult {
    if (command.find("com.example.demo.sh") != std::string::npos ||
        command.find("com.example.tools.sh") != std::string::npos) {
      return {0, "launcher ok\n"};
    }
    throw std::runtime_error(
        "unexpected launcher command in installed package matrix test: " +
        command);
  };

  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "linuxoid-installed-matrix";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path compatctl_path = root / "compatctl";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }

  const auto report = wfa::VerifyInstalledPackageMatrixWithRunners(
      {{.backend = wfa::RuntimeBackendKind::kAttachedAdb,
        .app_name = "Demo",
        .serial = "device-01",
        .package_name = "com.example.demo",
        .component = "com.example.demo/.MainActivity",
        .compatctl_path = compatctl_path.string()},
       {.backend = wfa::RuntimeBackendKind::kAttachedAdb,
        .app_name = "Tools",
        .serial = "device-01",
        .package_name = "com.example.tools",
        .component = "com.example.tools/.HomeActivity",
        .compatctl_path = compatctl_path.string()}},
      root.string(), runtime_runner, launcher_runner);

  Expect(report.entries.size() == 2, "expected two generic matrix entries");
  if (!report.entries[0].verification_ok) {
    throw std::runtime_error(
        "first matrix verification failed:\n" +
        wfa::RenderInstalledPackageVerificationReport(
            report.entries[0].verification));
  }
  Expect(report.entries[0].verification_ok,
         "expected first generic matrix package to pass");
  Expect(report.entries[1].verification_ok,
         "expected second generic matrix package to pass");

  const auto rendered = wfa::RenderInstalledPackageMatrixReport(report);
  Expect(rendered.find("Runtime Backend: attached-adb") !=
             std::string::npos,
         "expected backend name in generic matrix report");
  Expect(rendered.find("Runtime Target: device-01") != std::string::npos,
         "expected target label in generic matrix report");
  Expect(rendered.find("Packages Passed: 2/2") != std::string::npos,
         "expected generic matrix pass count");

  fs::remove_all(root);
}

void TestWaydroidPackageVerificationSuccessPath() {
  const auto runtime_runner = [](const std::string& command) -> wfa::CommandResult {
    if (command == "waydroid status") {
      return {0, "Session:\tRUNNING\n"};
    }
    if (command == "waydroid app launch com.android.calculator2") {
      return {0, ""};
    }
    throw std::runtime_error("unexpected runtime command in waydroid verification test");
  };

  const auto launcher_runner = [](const std::string& command) -> wfa::CommandResult {
    if (command.find("com.android.calculator2.sh") != std::string::npos) {
      return {0, ""};
    }
    throw std::runtime_error("unexpected launcher command in waydroid verification test");
  };

  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "linuxoid-waydroid-verify";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path compatctl_path = root / "compatctl";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }

  const auto report = wfa::VerifyWaydroidPackageWithRunners(
      {.app_name = "Calculator",
       .package_name = "com.android.calculator2",
       .compatctl_path = compatctl_path.string(),
       .desktop_root = (root / "applications").string(),
       .launcher_root = (root / "launchers").string()},
      runtime_runner, launcher_runner);

  Expect(report.direct_launch_ok, "expected direct waydroid launch success");
  Expect(report.launcher_generation_ok,
         "expected launcher generation success");
  Expect(report.generated_launcher_ok,
         "expected generated launcher execution success");

  const auto rendered = wfa::RenderWaydroidPackageVerificationReport(report);
  Expect(rendered.find("Preflight OK: yes") != std::string::npos,
         "expected waydroid preflight success line");
  Expect(rendered.find("Verification Loading: [##########] 100/100") !=
             std::string::npos,
         "expected full verification loading");
  Expect(rendered.find("Generated Launcher OK: yes") != std::string::npos,
         "expected generated launcher success line");

  fs::remove_all(root);
}

void TestWaydroidPackageMatrixSuccessPath() {
  const auto runtime_runner = [](const std::string& command) -> wfa::CommandResult {
    if (command == "waydroid status") {
      return {0, "Session:\tRUNNING\n"};
    }
    if (command == "waydroid app launch com.android.calculator2" ||
        command == "waydroid app launch com.android.settings") {
      return {0, ""};
    }
    return {1, "unexpected package"};
  };

  const auto launcher_runner = [](const std::string& command) -> wfa::CommandResult {
    if (command.find("com.android.calculator2.sh") != std::string::npos ||
        command.find("com.android.settings.sh") != std::string::npos) {
      return {0, ""};
    }
    return {1, "unexpected launcher"};
  };

  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "linuxoid-waydroid-matrix";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path compatctl_path = root / "compatctl";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }

  const auto report = wfa::VerifyWaydroidPackageMatrixWithRunners(
      {"com.android.calculator2", "com.android.settings"},
      compatctl_path.string(), root.string(), runtime_runner, launcher_runner);

  Expect(report.entries.size() == 2, "expected two matrix entries");
  Expect(report.entries[0].verification_ok,
         "expected first matrix package to pass");
  Expect(report.entries[1].verification_ok,
         "expected second matrix package to pass");

  const auto rendered = wfa::RenderWaydroidMatrixReport(report);
  Expect(rendered.find("Matrix Loading: [##########] 100/100") !=
             std::string::npos,
         "expected full matrix loading");
  Expect(rendered.find("Packages Passed: 2/2") != std::string::npos,
         "expected package pass count");

  fs::remove_all(root);
}

void TestNativeInstalledPackageVerificationUsesPreflightAndOverrideBackedLaunch() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-installed-package-verify");
  const fs::path native_root = fixture.root / "native";
  const fs::path runtime_probe = fixture.root / "linuxoid-art-runtime-probe";
  {
    std::ofstream output(runtime_probe);
    output << "#!/bin/sh\n";
    output << "case \"$*\" in\n";
    output << "  *linuxoid.bootstrap.mode=application*) printf '%s\\n' "
              "'application-runtime-ok'; exit 0 ;;\n";
    output << "  *linuxoid.bootstrap.mode=activity*) printf '%s\\n' "
              "'activity-runtime-ok'; exit 0 ;;\n";
    output << "  *) printf '%s\\n' 'runtime-fixture-ok'; exit 0 ;;\n";
    output << "esac\n";
  }
  fs::permissions(runtime_probe,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);
  const fs::path compatctl_path = fixture.root / "compatctl";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }

  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());
  ScopedEnvironmentVariable native_root_override("LINUXOID_NATIVE_SPIKE_ROOT",
                                                 native_root.string());
  ScopedEnvironmentVariable runtime_override(
      "LINUXOID_ART_RUNTIME_PROBE_OVERRIDE", runtime_probe.string());
  ScopedEnvironmentVariable allow_override_launch(
      "LINUXOID_NATIVE_ALLOW_RUNTIME_OVERRIDE", "1");

  const auto report = wfa::VerifyInstalledPackageWithRunners(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .app_name = "Native Bridge",
       .package_name = fixture.package_name,
       .compatctl_path = compatctl_path.string(),
       .desktop_root = (fixture.root / "applications").string(),
       .launcher_root = (fixture.root / "launchers").string()},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error(
            "native verification should not shell out through runtime bridge runner");
      },
      [](const std::string& command) -> wfa::CommandResult {
        if (command.find("com.example.nativebridge.sh") != std::string::npos) {
          return {0, "launcher ok\n"};
        }
        throw std::runtime_error(
            "unexpected launcher command in native verification test");
      });

  Expect(report.direct_launch_ok,
         "expected override-backed native verification launch success");
  Expect(report.launcher_generation_ok,
         "expected native verification launcher generation success");
  Expect(report.generated_launcher_ok,
         "expected native verification generated launcher success");

  const auto rendered = wfa::RenderInstalledPackageVerificationReport(report);
  Expect(rendered.find("Runtime Backend: native") != std::string::npos,
         "expected native backend in verification report");
  Expect(rendered.find("Runtime Target: linuxoid-native") != std::string::npos,
         "expected native target label in verification report");
  Expect(rendered.find("Preflight OK: yes") != std::string::npos,
         "expected native preflight success line");
  Expect(rendered.find("Package Visible: yes") != std::string::npos,
         "expected native package visibility line");
  Expect(rendered.find("Component Ready: yes") != std::string::npos,
         "expected native component readiness line");
  Expect(rendered.find("ART Runtime Probe Source: override") !=
             std::string::npos,
         "expected override probe source in native verification preflight");
  Expect(rendered.find("Runtime Probe Ready: yes") != std::string::npos,
         "expected runtime probe readiness in native verification preflight");
  Expect(rendered.find("Bootstrap Planned: yes") != std::string::npos,
         "expected bootstrap planning in native verification preflight");
  Expect(rendered.find("Runtime Health Trace Path: ") != std::string::npos,
         "expected runtime health trace path in native verification preflight");
  Expect(rendered.find("Runtime Diagnostic Events Path: ") !=
             std::string::npos,
         "expected diagnostic events path in native verification preflight");
  Expect(rendered.find("Runtime Trace Bundle Complete: yes") !=
             std::string::npos,
         "expected complete trace bundle in native verification preflight");
  Expect(rendered.find("Component: com.example.nativebridge/.MainActivity") !=
             std::string::npos,
         "expected native launcher component in verification report");

  fs::remove_all(fixture.root);
}

void TestNativeInstalledPackageVerificationReportsNonCandidateFailureHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateNativeRuntimePackageFixture(
      "linuxoid-native-installed-package-verify-noncandidate", true, true);
  const fs::path native_root = fixture.root / "native";
  const fs::path compatctl_path = fixture.root / "compatctl";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }

  ScopedEnvironmentVariable compat_root_override(
      "LINUXOID_NATIVE_COMPAT_ROOT", fixture.compat_root.string());
  ScopedEnvironmentVariable native_root_override("LINUXOID_NATIVE_SPIKE_ROOT",
                                                 native_root.string());

  const auto report = wfa::VerifyInstalledPackageWithRunners(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .app_name = "Native Bridge",
       .package_name = fixture.package_name,
       .compatctl_path = compatctl_path.string(),
       .desktop_root = (fixture.root / "applications").string(),
       .launcher_root = (fixture.root / "launchers").string()},
      [](const std::string&) -> wfa::CommandResult {
        throw std::runtime_error(
            "native verification should not shell out through runtime bridge runner");
      },
      [](const std::string& command) -> wfa::CommandResult {
        if (command.find("com.example.nativebridge.sh") != std::string::npos) {
          return {0, "launcher ok\n"};
        }
        throw std::runtime_error(
            "unexpected launcher command in native verification failure test");
      });

  Expect(!report.preflight_ok,
         "expected native preflight to fail for non-candidate bundle");
  Expect(!report.direct_launch_ok,
         "expected native verification to fail for non-candidate bundle");
  Expect(report.launcher_generation_ok,
         "expected native verification launcher generation success");
  Expect(report.generated_launcher_ok,
         "expected native verification generated launcher success");
  Expect(report.preflight_output.find("native spike candidate") !=
             std::string::npos,
         "expected native candidate failure reason in verification preflight");

  const auto rendered = wfa::RenderInstalledPackageVerificationReport(report);
  Expect(rendered.find("Preflight OK: no") != std::string::npos,
         "expected native preflight failure on non-candidate bundle");
  Expect(rendered.find("Direct Launch OK: no") != std::string::npos,
         "expected native direct launch failure line");

  fs::remove_all(fixture.root);
}

void TestWaydroidPackageMatrixCapturesFailure() {
  const auto runtime_runner = [](const std::string& command) -> wfa::CommandResult {
    if (command == "waydroid status") {
      return {0, "Session:\tRUNNING\n"};
    }
    if (command == "waydroid app launch com.android.calculator2") {
      return {0, ""};
    }
    if (command == "waydroid app launch org.fdroid.fdroid") {
      return {1, "launch failed"};
    }
    return {1, "unexpected package"};
  };

  const auto launcher_runner = [](const std::string& command) -> wfa::CommandResult {
    if (command.find("com.android.calculator2.sh") != std::string::npos ||
        command.find("org.fdroid.fdroid.sh") != std::string::npos) {
      return {0, ""};
    }
    return {1, "unexpected launcher"};
  };

  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / "linuxoid-waydroid-matrix-failure";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path compatctl_path = root / "compatctl";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }

  const auto report = wfa::VerifyWaydroidPackageMatrixWithRunners(
      {"com.android.calculator2", "org.fdroid.fdroid"},
      compatctl_path.string(), root.string(), runtime_runner, launcher_runner);

  Expect(report.entries.size() == 2, "expected two matrix entries");
  Expect(report.entries[0].verification_ok,
         "expected first matrix package to pass");
  Expect(!report.entries[1].verification_ok,
         "expected second matrix package to fail");

  const auto rendered = wfa::RenderWaydroidMatrixReport(report);
  Expect(rendered.find("Packages Passed: 1/2") != std::string::npos,
         "expected partial package pass count");

  fs::remove_all(root);
}

void TestApkHostVerificationImeSuccessPath() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "linuxoid-apk-host-ime";
  fs::remove_all(root);
  fs::create_directories(root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1");

  const fs::path compatctl_path = root / "compatctl";
  const fs::path staged_apk_path =
      root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1/base.apk";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }
  {
    std::ofstream staged_apk_output(staged_apk_path);
    staged_apk_output << "staged apk\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/keyboard.apk",
      .install_id = "vc1",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "keyboard.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 1,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "org.futo.inputmethod.latin",
          .launcher_activity_name =
              "org.futo.inputmethod.latin/.uix.settings.SettingsActivity",
          .input_method_service_name = "org.futo.inputmethod.latin/.LatinIME",
          .declared_components =
              {"org.futo.inputmethod.latin.uix.settings.SettingsActivity",
               "org.futo.inputmethod.latin.LatinIME"},
          .declared_activity_components =
              {"org.futo.inputmethod.latin.uix.settings.SettingsActivity"},
          .has_launcher_activity = true,
          .has_input_method_service = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "org.futo.inputmethod.latin",
          .app_profile = "input_method",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "POST_P6_IME",
      },
      .layout = wfa::BuildPackageLayout(
          {.package_name = "org.futo.inputmethod.latin",
           .install_id = "vc1",
           .version_code = 1},
          (root / "compat").string()),
      .install_root =
          (root / "compat/users/0/packages/org.futo.inputmethod.latin/vc1")
              .string(),
  };

  const auto launcher_runner = [](const std::string& command) -> wfa::CommandResult {
    if (command.find("org.futo.inputmethod.latin.sh") != std::string::npos) {
      return {0, "Ready for typing: yes\n"};
    }
    throw std::runtime_error(
        "unexpected launcher command in apk host ime verification test");
  };

  const auto verification = wfa::VerifyLoadedApkHostLaunchAutoWithRunner(
      report,
      {.serial = "192.168.240.112:5555",
       .compatctl_path = compatctl_path.string(),
       .desktop_root = (root / "applications").string(),
       .launcher_root = (root / "launchers").string()},
      launcher_runner);

  Expect(verification.apk_load_ok, "expected apk load fact to be set");
  Expect(verification.launcher_generation_ok,
         "expected launcher generation success");
  Expect(verification.generated_launcher_ok,
         "expected ime host verification success");
  Expect(verification.artifacts.uses_provision_mode,
         "expected ime flow to use provision mode");

  const auto rendered = wfa::RenderApkHostVerificationReport(verification);
  Expect(rendered.find("Verification Loading: [##########] 100/100") !=
             std::string::npos,
         "expected full apk host verification loading");
  Expect(rendered.find("Launch Mode: provision-ime") != std::string::npos,
         "expected provision-ime launch mode");

  fs::remove_all(root);
}

void TestApkHostVerificationAppSuccessPath() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "linuxoid-apk-host-app";
  fs::remove_all(root);
  fs::create_directories(root / "compat/users/0/packages/com.example.demo/vc7");

  const fs::path compatctl_path = root / "compatctl";
  const fs::path staged_apk_path =
      root / "compat/users/0/packages/com.example.demo/vc7/base.apk";
  {
    std::ofstream compatctl_output(compatctl_path);
    compatctl_output << "#!/bin/sh\nexit 0\n";
  }
  {
    std::ofstream staged_apk_output(staged_apk_path);
    staged_apk_output << "staged apk\n";
  }

  const wfa::LoadedApkReport report{
      .apk_path = "/tmp/demo.apk",
      .install_id = "vc7",
      .metadata = wfa::ApktoolMetadata{
          .apk_file_name = "demo.apk",
          .min_sdk = 24,
          .target_sdk = 35,
          .version_code = 7,
          .version_name = "1.0.0",
      },
      .manifest_profile = wfa::ManifestProfile{
          .package_name = "com.example.demo",
          .launcher_activity_name = "com.example.demo.MainActivity",
          .declared_components = {"com.example.demo.MainActivity"},
          .declared_activity_components = {"com.example.demo.MainActivity"},
          .has_launcher_activity = true,
      },
      .assessment = wfa::ManifestAssessment{
          .package_name = "com.example.demo",
          .app_profile = "foreground_app",
          .earliest_load_phase = "P4",
          .earliest_ui_phase = "P6",
          .earliest_full_use_phase = "P6",
      },
      .layout = wfa::BuildPackageLayout(
          {.package_name = "com.example.demo",
           .install_id = "vc7",
           .version_code = 7},
          (root / "compat").string()),
      .install_root =
          (root / "compat/users/0/packages/com.example.demo/vc7").string(),
  };

  const auto launcher_runner = [](const std::string& command) -> wfa::CommandResult {
    if (command.find("com.example.demo.sh") != std::string::npos) {
      return {0, "Launch OK: yes\n"};
    }
    throw std::runtime_error(
        "unexpected launcher command in apk host app verification test");
  };

  const auto verification = wfa::VerifyLoadedApkHostLaunchAutoWithRunner(
      report,
      {.serial = "192.168.240.112:5555",
       .compatctl_path = compatctl_path.string(),
       .desktop_root = (root / "applications").string(),
       .launcher_root = (root / "launchers").string()},
      launcher_runner);

  Expect(verification.apk_load_ok, "expected apk load fact to be set");
  Expect(verification.launcher_generation_ok,
         "expected launcher generation success");
  Expect(verification.generated_launcher_ok,
         "expected app host verification success");
  Expect(!verification.artifacts.uses_provision_mode,
         "expected non-ime flow to avoid provision mode");

  const auto rendered = wfa::RenderApkHostVerificationReport(verification);
  Expect(rendered.find("Launch Mode: launch-activity") != std::string::npos,
         "expected launch-activity mode");
  Expect(rendered.find("Generated Launcher OK: yes") != std::string::npos,
         "expected generated launcher success line");

  fs::remove_all(root);
}

void TestImeProvisioningSuccessPath() {
  std::vector<std::string> commands;

  const auto runner = [&](const std::string& command) -> wfa::CommandResult {
    commands.push_back(command);

    if (command.find(" install -r ") != std::string::npos) {
      return {0, "Success\n"};
    }
    if (command.find(" shell ime enable ") != std::string::npos) {
      return {0, "Input method org.futo.inputmethod.latin/.LatinIME: now enabled\n"};
    }
    if (command.find(" shell ime set ") != std::string::npos) {
      return {0, "Input method org.futo.inputmethod.latin/.LatinIME selected\n"};
    }
    if (command.find(" shell pm list packages") != std::string::npos) {
      return {0, "package:org.futo.inputmethod.latin\n"};
    }
    if (command.find(" shell ime list -a") != std::string::npos) {
      return {0, "org.futo.inputmethod.latin/.LatinIME:\n"};
    }
    if (command.find(" settings get secure default_input_method") !=
        std::string::npos) {
      return {0, "org.futo.inputmethod.latin/.LatinIME\n"};
    }
    if (command.find(" settings get secure enabled_input_methods") !=
        std::string::npos) {
      return {0,
              "com.example.other/.Ime:org.futo.inputmethod.latin/.LatinIME\n"};
    }
    if (command.find(" shell am start -W -n ") != std::string::npos) {
      return {0, "Status: ok\nComplete\n"};
    }

    throw std::runtime_error("unexpected command in provisioning success path");
  };

  const auto report = wfa::ProvisionAdbImeWithRunner(
      "emulator-5590", "/tmp/keyboard.apk", "org.futo.inputmethod.latin",
      "org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME",
      "org.futo.inputmethod.latin/.uix.settings.SettingsActivity", runner);

  Expect(report.install_ok, "expected install success");
  Expect(report.enable_ok, "expected ime enable success");
  Expect(report.set_ok, "expected ime set success");
  Expect(report.ready_for_typing, "expected ready for typing");
  Expect(report.final_status.package_installed,
         "expected installed package after provisioning");
  Expect(report.final_status.ime_registered,
         "expected registered ime after provisioning");
  Expect(report.final_status.ime_enabled,
         "expected enabled ime after provisioning");
  Expect(report.final_status.is_default_ime,
         "expected default ime after provisioning");
  Expect(commands.size() == 8, "expected eight adb commands in provisioning flow");
  Expect(commands[1].find("org.futo.inputmethod.latin/.LatinIME") !=
             std::string::npos,
         "expected ime enable to use short component form");
  Expect(commands[1].find(
             "org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME") ==
             std::string::npos,
         "did not expect ime enable to use fully qualified component form");
  Expect(commands[2].find("org.futo.inputmethod.latin/.LatinIME") !=
             std::string::npos,
         "expected ime set to use short component form");

  const auto rendered = wfa::RenderAdbProvisioningReport(report);
  Expect(rendered.find("Ready for typing: yes") != std::string::npos,
         "expected ready-for-typing line");
}

void TestImeProvisioningNormalizesFullyQualifiedImeIdForWaydroidStyleMutation() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command.find(" install -r ") != std::string::npos) {
      return {0, "Success\n"};
    }
    if (command.find(" shell ime enable ") != std::string::npos) {
      if (command.find("org.futo.inputmethod.latin/.LatinIME") !=
          std::string::npos) {
        return {0,
                "Input method org.futo.inputmethod.latin/.LatinIME: now enabled\n"};
      }
      return {255,
              "Unknown input method org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME cannot be enabled for user #0\n"};
    }
    if (command.find(" shell ime set ") != std::string::npos) {
      if (command.find("org.futo.inputmethod.latin/.LatinIME") !=
          std::string::npos) {
        return {0, "Input method org.futo.inputmethod.latin/.LatinIME selected\n"};
      }
      return {255,
              "Unknown input method org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME cannot be selected for user #0\n"};
    }
    if (command.find(" shell pm list packages") != std::string::npos) {
      return {0, "package:org.futo.inputmethod.latin\n"};
    }
    if (command.find(" shell ime list -a") != std::string::npos) {
      return {0, "org.futo.inputmethod.latin/.LatinIME:\n"};
    }
    if (command.find(" settings get secure default_input_method") !=
        std::string::npos) {
      return {0, "org.futo.inputmethod.latin/.LatinIME\n"};
    }
    if (command.find(" settings get secure enabled_input_methods") !=
        std::string::npos) {
      return {0, "org.futo.inputmethod.latin/.LatinIME\n"};
    }
    if (command.find(" shell am start -W -n ") != std::string::npos) {
      return {0, "Status: ok\nComplete\n"};
    }

    throw std::runtime_error(
        "unexpected command in waydroid-style provisioning path");
  };

  const auto report = wfa::ProvisionAdbImeWithRunner(
      "192.168.240.112:5555", "/tmp/keyboard.apk",
      "org.futo.inputmethod.latin",
      "org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME",
      "org.futo.inputmethod.latin/.uix.settings.SettingsActivity", runner);

  Expect(report.install_ok, "expected install success in waydroid-style flow");
  Expect(report.enable_ok, "expected enable success in waydroid-style flow");
  Expect(report.set_ok, "expected set success in waydroid-style flow");
  Expect(report.ready_for_typing,
         "expected readiness success after IME normalization");
}

void TestImeProvisioningDetectsIncompleteActivation() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command.find(" install -r ") != std::string::npos) {
      return {0, "Success\n"};
    }
    if (command.find(" shell ime enable ") != std::string::npos) {
      return {0, "Input method org.futo.inputmethod.latin/.LatinIME: now enabled\n"};
    }
    if (command.find(" shell ime set ") != std::string::npos) {
      return {0, "Input method org.futo.inputmethod.latin/.LatinIME selected\n"};
    }
    if (command.find(" shell pm list packages") != std::string::npos) {
      return {0, "package:org.futo.inputmethod.latin\n"};
    }
    if (command.find(" shell ime list -a") != std::string::npos) {
      return {0, "org.futo.inputmethod.latin/.LatinIME:\n"};
    }
    if (command.find(" settings get secure default_input_method") !=
        std::string::npos) {
      return {0, "com.example.other/.Ime\n"};
    }
    if (command.find(" settings get secure enabled_input_methods") !=
        std::string::npos) {
      return {0, "org.futo.inputmethod.latin/.LatinIME\n"};
    }

    throw std::runtime_error(
        "unexpected command in provisioning incomplete-activation path");
  };

  const auto report = wfa::ProvisionAdbImeWithRunner(
      "emulator-5590", "/tmp/keyboard.apk", "org.futo.inputmethod.latin",
      "org.futo.inputmethod.latin/.LatinIME", "", runner);

  Expect(report.install_ok, "expected install success in incomplete flow");
  Expect(report.enable_ok, "expected enable success in incomplete flow");
  Expect(report.set_ok, "expected set success in incomplete flow");
  Expect(!report.ready_for_typing,
         "expected readiness failure when default ime does not match");

  const auto rendered = wfa::RenderAdbProvisioningReport(report);
  Expect(rendered.find("Ready for typing: no") != std::string::npos,
         "expected failed ready-for-typing line");
}

void TestImeProvisioningRejectsWrongApkPackagePairing() {
  std::vector<std::string> commands;
  const auto runner = [&](const std::string& command) -> wfa::CommandResult {
    commands.push_back(command);
    return {0, "unexpected command\n"};
  };

  const auto report = wfa::ProvisionAdbImeWithRunner(
      "emulator-5590", "/tmp/not-keyboard.apk", "org.futo.inputmethod.latin",
      "org.futo.inputmethod.latin/.LatinIME", "", runner,
      "com.example.notkeyboard");

  Expect(!report.apk_matches_requested_package,
         "expected wrong apk package pairing to be detected");
  Expect(commands.empty(),
         "expected wrong apk package pairing to fail before any adb mutation");
  Expect(!report.install_ok,
         "expected no install attempt on wrong apk package pairing");
  Expect(!report.readback_ok,
         "expected no readback success on wrong apk package pairing");
  Expect(!report.ready_for_typing,
         "expected wrong apk package pairing to fail readiness");
  Expect(report.readback_error.find("package mismatch") != std::string::npos,
         "expected package mismatch readback error");

  const auto rendered = wfa::RenderAdbProvisioningReport(report);
  Expect(rendered.find("APK package match: no") != std::string::npos,
         "expected wrong apk package match line");
}

void TestImeProvisioningKeepsPartialEvidenceOnReadbackFailure() {
  const auto runner = [](const std::string& command) -> wfa::CommandResult {
    if (command.find(" install -r ") != std::string::npos) {
      return {0, "Success\n"};
    }
    if (command.find(" shell ime enable ") != std::string::npos) {
      return {0, "Input method org.futo.inputmethod.latin/.LatinIME: now enabled\n"};
    }
    if (command.find(" shell ime set ") != std::string::npos) {
      return {0, "Input method org.futo.inputmethod.latin/.LatinIME selected\n"};
    }
    if (command.find(" shell pm list packages") != std::string::npos) {
      return {0, "package:org.futo.inputmethod.latin\n"};
    }
    if (command.find(" shell ime list -a") != std::string::npos) {
      return {0, "org.futo.inputmethod.latin/.LatinIME:\n"};
    }
    if (command.find(" settings get secure enabled_input_methods") !=
        std::string::npos) {
      return {0, "org.futo.inputmethod.latin/.LatinIME\n"};
    }
    if (command.find(" settings get secure default_input_method") !=
        std::string::npos) {
      return {256, "permission denied\n"};
    }

    throw std::runtime_error(
        "unexpected command in provisioning readback-failure path");
  };

  const auto report = wfa::ProvisionAdbImeWithRunner(
      "emulator-5590", "/tmp/keyboard.apk", "org.futo.inputmethod.latin",
      "org.futo.inputmethod.latin/.LatinIME", "", runner);

  Expect(report.install_ok, "expected install success before readback failure");
  Expect(report.enable_ok, "expected enable success before readback failure");
  Expect(report.set_ok, "expected set success before readback failure");
  Expect(report.final_status.package_installed,
         "expected package-installed fact to survive readback failure");
  Expect(report.final_status.ime_registered,
         "expected ime-registered fact to survive readback failure");
  Expect(report.final_status.ime_enabled,
         "expected ime-enabled fact to survive readback failure");
  Expect(!report.readback_ok,
         "expected readback to fail without throwing away report");
  Expect(!report.ready_for_typing,
         "expected readback failure to prevent ready-for-typing");
  Expect(report.readback_error.find("default_input_method") != std::string::npos,
         "expected readback error to mention the failing command");

  const auto rendered = wfa::RenderAdbProvisioningReport(report);
  Expect(rendered.find("Readback OK: no") != std::string::npos,
         "expected readback failure line");
}

}  // namespace

int main() {
  try {
    TestWeightedCheckpointProgress();
    TestPhaseProgressAverage();
    TestPackageLayoutBuildsExpectedPaths();
    TestPackageValidationRejectsInvalidName();
    TestStatusRenderingContainsLoadingBars();
    TestSimpleLauncherAssessment();
    TestKeyboardAssessmentRequiresPostP6ImeIntegration();
    TestActivityAliasLauncherAssessment();
    TestDisabledLauncherCandidateFallsThroughToEnabledActivity();
    TestAdvancedRuntimeBlockersPushFullUsePastP6();
    TestInvalidManifestRejected();
    TestApktoolMetadataParsing();
    TestLoadedApkReportRendering();
    TestNativeSpikeAssessmentAcceptsSimpleForegroundApp();
    TestNativeSpikeAssessmentAcceptsResolvedLauncherWithMultipleActivities();
    TestNativeSpikeAssessmentRejectsAdvancedRuntimeApp();
    TestNativeLaunchPlanBuildsBundleLayout();
    TestNativeActivityBootstrapWritesArtifacts();
    TestNativeLaunchPlanStagesHostAbiLibrariesAndAssets();
    TestNativeLaunchPlanReportsUnsupportedAbiClearly();
    TestAssetManagerReadsFixtureAsset();
    TestAssetManagerListsZipAssetsAndBlocksTraversal();
    TestApkResourceReadinessReadsManifestAndAssetsFromZipFixture();
    TestApkResourceReadinessHandlesMissingManifest();
    TestOpenedApkArchiveReadsEntriesDeterministically();
    TestApkResourceReadinessUsesStagedManifestFallback();
    TestInspectApkResourcesCommandWritesStableJson();
    TestHeadlessNativeWindowSurfaceTracksMetadataAndLifecycle();
    TestHeadlessFirstPixelFixtureWritesDeterministicMarker();
    TestHeadlessNativeWindowCallbackFixtureWritesJournal();
    TestWaylandSurfaceFixtureWritesDeterministicMetadata();
    TestWaylandSurfaceFixtureReportsAvailabilityHonestly();
    TestEglSmokeFixtureWritesDeterministicMetadata();
    TestEglSmokeFixtureReportsAvailabilityHonestly();
    TestNativeWindowBridgeFixtureWritesStableArtifacts();
    TestNativeWindowBridgeFixtureReportsFallbackHonestly();
    TestNativeInputQueueFixtureWritesStableArtifacts();
    TestNativeInputQueueFixtureReportsFallbackHonestly();
    TestBinderServiceManagerFixtureWritesStableArtifacts();
    TestRuntimeHealthFixtureWritesStableArtifacts();
    TestRuntimeHealthFixtureSelectsMissingArtifactRecovery();
    TestRuntimeHealthFixtureSelectsUnavailableDisplayRecovery();
    TestRuntimeHealthFixtureSelectsFailedServiceLookupRecovery();
    TestRuntimeHealthFixtureIncludesCoreSubsystemRecords();
    TestRuntimeHealthFixtureTreatsDexOnlyNativeLoadingAsNotRequired();
    TestRuntimeHealthFixtureRejectsMissingNativeDependencyWithoutFalseSuccess();
    TestRuntimeHealthFixtureTracksActivityBootstrapReadiness();
    TestRuntimeHealthFixtureCarriesBootstrapExecutionEvidence();
    TestRuntimeHealthFixtureBecomesReadyWithRuntimeOverride();
    TestRuntimeHealthSummaryFieldsStayDeterministic();
    TestRuntimeHealthReplaySummarizesTrace();
    TestNativeArtRuntimeSmokeWritesTraceJsonl();
    TestRuntimeDiagnosticReplayWritesStableArtifacts();
    TestRuntimeDiagnosticReplayHandlesMissingTraceHonestly();
    TestRuntimeDiagnosticReplayReportsMissingNativeDependencyHonestly();
    TestRuntimeDiagnosticReplayCommandWritesStableJson();
    TestRuntimeDiagnosticFixtureCommandWritesStableJson();
    TestRuntimeDiagnosticFixtureMaterializesReplayableTraceBundle();
    TestRuntimeHealthCommandWritesStableJson();
    TestRuntimeHealthCommandOutputIsStableAcrossRepeatedRuns();
    TestRuntimeHealthCommandSupportsLegacyBootstrapManifestWithoutNativeLibrarySummaryFields();
    TestRuntimeHealthCommandReportsMissingNativeDependencyHonestly();
    TestRuntimeHealthCommandMissingNativeContractStaysDeterministic();
    TestRuntimeRecoveryPlanWritesStableArtifacts();
    TestRuntimeRecoveryPlanScenariosSelectDeterministicActions();
    TestRuntimeRecoveryPlanCommandWritesStableJson();
    TestRuntimeRecoveryPlanCommandCoreScenariosStayDeterministic();
    TestSelfHealingRuntimeCliContractMatrix();
    TestSelfHealingRuntimeCliContractMatrixIncludesReplayHonesty();
    TestNativeArtClassloaderFixtureWritesStableArtifacts();
    TestNativeArtClassloaderFixtureHandlesMissingDexHonestly();
    TestNativeArtClassloaderCommandWritesStableJson();
    TestNativeArtRuntimeSmokeWritesStableArtifacts();
    TestNativeArtRuntimeSmokeHandlesRuntimeAvailabilityHonestly();
    TestNativeArtRuntimeSmokeCommandWritesStableJson();
    TestNativeArtRuntimeSmokeUsesFixtureRuntimeOverride();
    TestNativeArtRuntimeSmokeRecordsProbeInventoryAndReason();
    TestNativeArtRuntimeSmokeClassifiesHostAppProcessAsDetectionOnly();
    TestNativeArtActivityBootstrapFixtureWritesStableArtifacts();
    TestNativeArtActivityBootstrapTraceCapturesApplicationBootstrapSequence();
    TestNativeArtActivityBootstrapFixtureHandlesRuntimeAvailabilityHonestly();
    TestNativeArtActivityBootstrapCommandWritesStableJson();
    TestNativeArtBootstrapExecutionFixtureWritesStableArtifacts();
    TestNativeArtBootstrapExecutionFixtureCanReuseActivityBootstrapReport();
    TestNativeArtBootstrapExecutionFixtureHandlesRuntimeAvailabilityHonestly();
    TestNativeArtBootstrapExecutionFixtureRunsThroughSupervisedRunner();
    TestNativeArtBootstrapExecutionFixtureAttemptsRuntimeThroughOverride();
    TestNativeArtBootstrapExecutionCommandWritesStableJson();
    TestNativeArtClassResolutionFixtureResolvesManifestTargets();
    TestNativeArtClassResolutionFixtureHandlesMissingDexTargetsHonestly();
    TestNativeArtClassResolutionCommandWritesStableJson();
    TestNativeLifecycleShimWritesSessionArtifacts();
    TestNativeProcessBootstrapRunsFixtureAndWritesSessionState();
    TestNativeExecuteStubReportsMissingNativeLibraryPayload();
    TestNativeExecuteStubRunsFixtureNativeActivity();
    TestRuntimeBridgeOutputParsers();
    TestActivityLaunchReportRendering();
    TestNativeRuntimeDiscoveryUsesCompatRootOverride();
    TestNativeRuntimeMetadataReadsStagedPackage();
    TestNativeRuntimePreflightUsesStagedMetadata();
    TestNativeRuntimePreflightBlocksWithoutHostArt();
    TestNativeRuntimePreflightSurfacesProbeInventoryAndReason();
    TestNativeRuntimePreflightReportsHostAppProcessCapabilityHonestly();
    TestNativeRuntimeLaunchCanUseOverrideBackedBootstrapExecution();
    TestNativeRuntimeLaunchRejectsOverrideBackedBootstrapByDefault();
    TestNativeRuntimeLaunchReportsNonCandidateFailureHonestly();
    TestNativeRuntimeLaunchSurfacesBlockedSubsystemsWithoutHostArt();
    TestNativeRuntimeLaunchSurfacesProbeInventoryAndReason();
    TestNativeRuntimeLaunchReportsHostAppProcessCapabilityHonestly();
    TestDesktopLaunchArtifactsForImeApp();
    TestDesktopLaunchArtifactsForLoadedApkUseStagedPath();
    TestDesktopLaunchArtifactsRejectCrossPackageComponent();
    TestDesktopLaunchArtifactsRejectUnknownDeclaredComponent();
    TestDesktopLaunchArtifactsRejectServiceLaunchTarget();
    TestAutoDesktopLaunchArtifactsInferLauncherAndSplitRoots();
    TestAutoDesktopLaunchArtifactsRejectHeadlessApp();
    TestWaydroidAppLaunchReportRendering();
    TestInstalledAppLaunchReportRendering();
    TestInstalledAppLaunchReportRenderingIncludesRuntimeTraceBundle();
    TestAttachedAdbInstalledAppLaunchUsesExplicitComponent();
    TestAttachedAdbInstalledAppLaunchAutoResolvesComponent();
    TestAttachedAdbPackageMetadataLookupExtractsLauncherAndVersion();
    TestAttachedAdbRuntimeDiscoveryParsesTargets();
    TestAttachedAdbPreflightAutoSelectsSingleTarget();
    TestAttachedAdbPreflightResolvesComponentWhenOmitted();
    TestAttachedAdbPreflightRequiresSerialWhenMultipleTargetsExist();
    TestAttachedAdbDiscoveryTimeoutReturnsUnavailable();
    TestNativeRuntimePreflightReportsMissingStagedPackageHonestly();
    TestWaydroidDesktopLaunchArtifacts();
    TestInstalledPackageDesktopLaunchArtifactsRequireAttachedAdbFields();
    TestWaydroidDesktopLaunchArtifactsRejectInvalidPackage();
    TestInstalledPackageVerificationSuccessPath();
    TestInstalledPackageMatrixSuccessPath();
    TestWaydroidPackageVerificationSuccessPath();
    TestWaydroidPackageMatrixSuccessPath();
    TestNativeInstalledPackageVerificationUsesPreflightAndOverrideBackedLaunch();
    TestNativeInstalledPackageVerificationReportsNonCandidateFailureHonestly();
    TestWaydroidPackageMatrixCapturesFailure();
    TestApkHostVerificationImeSuccessPath();
    TestApkHostVerificationAppSuccessPath();
    TestImeProvisioningSuccessPath();
    TestImeProvisioningNormalizesFullyQualifiedImeIdForWaydroidStyleMutation();
    TestImeProvisioningDetectsIncompleteActivation();
    TestImeProvisioningRejectsWrongApkPackagePairing();
    TestImeProvisioningKeepsPartialEvidenceOnReadbackFailure();
  } catch (const std::exception& error) {
    std::cerr << "Test failure: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "All tests passed.\n";
  return EXIT_SUCCESS;
}
