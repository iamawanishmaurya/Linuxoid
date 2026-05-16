#include "wfa/apk_host_integration.hpp"
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

RuntimeHealthBootstrapFixture CreateRuntimeHealthBootstrapFixture(
    const std::string& fixture_name, bool include_classes_dex,
    bool include_native_library) {
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
  Expect(report.records.size() == 6, "expected six subsystem health records");
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
    Expect(report.exit_reason == "art_runtime_not_detected",
           "expected absent-art exit reason");
  } else if (!report.safe_runtime_probe_available) {
    Expect(!report.runtime_probe_attempted,
           "expected no unsafe runtime probe attempt");
    Expect(report.exit_reason ==
               "art_runtime_detected_without_safe_probe",
           "expected no-safe-probe exit reason");
  } else {
    Expect(report.runtime_probe_attempted,
           "expected runtime probe attempt when safe ART probe exists");
    Expect(report.runtime_exit_code >= 0,
           "expected concrete runtime probe exit code");
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

void TestRuntimeHealthFixtureRejectsMissingNativeDependencyWithoutFalseSuccess() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-missing-native", true, false);

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

void TestRuntimeHealthReplaySummarizesTrace() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-replay", true, true);

  const auto report = wfa::RunRuntimeHealthFixture(
      fixture.bootstrap.bootstrap_manifest_path, "baseline");
  const auto replay = wfa::ReplayRuntimeHealthTrace(report.trace_jsonl_path);

  Expect(replay.events_read >= 6, "expected runtime health trace events");
  Expect(replay.subsystems_observed == 6,
         "expected six subsystems in replay");
  Expect(std::find(replay.failing_subsystems.begin(),
                   replay.failing_subsystems.end(),
                   "dex_classloader_readiness") !=
             replay.failing_subsystems.end(),
         "expected dex classloader replay failure");
  Expect(std::find(replay.selected_actions.begin(),
                   replay.selected_actions.end(),
                   "attempt_host_art_class_resolution") !=
             replay.selected_actions.end(),
         "expected dex recovery action in replay");

  const auto rendered = wfa::RenderRuntimeHealthReplayJson(replay);
  Expect(rendered.find("\"overall_state\": \"recovery_needed\"") !=
             std::string::npos,
         "expected replay overall state in json");

  fs::remove_all(fixture.root);
}

void TestRuntimeDiagnosticReplayReportsMissingNativeDependencyHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-diagnostic-missing-native", true, false);

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
  Expect(fs::exists(replay.result_json_path),
         "expected diagnostic replay json artifact");
  Expect(fs::exists(replay.merged_trace_jsonl_path),
         "expected merged diagnostic trace artifact");
  Expect(replay.total_events_read >= replay.trace_sources_found,
         "expected trace events across diagnostic sources");
  Expect(std::find(replay.selected_actions.begin(),
                   replay.selected_actions.end(),
                   "attempt_host_art_class_resolution") !=
             replay.selected_actions.end(),
         "expected dex recovery action in diagnostic replay");

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
  Expect(replay.exit_reason == "missing_trace_artifact",
         "expected missing-trace exit reason");
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
  Expect(output.find("\"merged_trace_jsonl_path\": ") != std::string::npos,
         "expected merged trace path in diagnostic replay json");

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

  fs::remove_all(fixture.root);
}

void TestRuntimeHealthCommandReportsMissingNativeDependencyHonestly() {
  namespace fs = std::filesystem;
  auto fixture = CreateRuntimeHealthBootstrapFixture(
      "linuxoid-runtime-health-command-missing-native", true, false);
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

  fs::remove_all(fixture.root);
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

void TestNativeRuntimePreflightReportsNotImplemented() {
  const auto runner = [](const std::string&) -> wfa::CommandResult {
    throw std::runtime_error("native preflight should not execute commands");
  };

  const auto report = wfa::PreflightRuntimeWithRunner(
      {.backend = wfa::RuntimeBackendKind::kNative,
       .package_name = "com.example.demo"},
      runner);

  Expect(!report.backend_available,
         "expected native backend to report unavailable");
  Expect(!report.ready_for_launch,
         "expected native preflight to fail honestly");
  Expect(report.notes.find("not implemented") != std::string::npos,
         "expected native preflight note");
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
    if (command ==
        "adb -s 'device-01' shell am start -W -n 'com.example.demo/.MainActivity'") {
      return {0,
              "Starting: Intent { cmp=com.example.demo/.MainActivity }\nStatus: ok\nComplete\n"};
    }
    throw std::runtime_error(
        "unexpected runtime command in installed package verification test");
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
  Expect(rendered.find("Verification Loading: [##########] 100/100") !=
             std::string::npos,
         "expected full generic verification loading");

  fs::remove_all(root);
}

void TestInstalledPackageMatrixSuccessPath() {
  const auto runtime_runner = [](const std::string& command)
      -> wfa::CommandResult {
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
    return {1, "unexpected package"};
  };

  const auto launcher_runner = [](const std::string& command)
      -> wfa::CommandResult {
    if (command.find("com.example.demo.sh") != std::string::npos ||
        command.find("com.example.tools.sh") != std::string::npos) {
      return {0, "launcher ok\n"};
    }
    return {1, "unexpected launcher"};
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
  Expect(report.entries[0].verification_ok,
         "expected first generic matrix package to pass");
  Expect(report.entries[1].verification_ok,
         "expected second generic matrix package to pass");

  const auto rendered = wfa::RenderInstalledPackageMatrixReport(report);
  Expect(rendered.find("Runtime Backend: attached-adb") !=
             std::string::npos,
         "expected backend name in generic matrix report");
  Expect(rendered.find("Packages Passed: 2/2") != std::string::npos,
         "expected generic matrix pass count");

  fs::remove_all(root);
}

void TestWaydroidPackageVerificationSuccessPath() {
  const auto runtime_runner = [](const std::string& command) -> wfa::CommandResult {
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
  Expect(rendered.find("Verification Loading: [##########] 100/100") !=
             std::string::npos,
         "expected full verification loading");
  Expect(rendered.find("Generated Launcher OK: yes") != std::string::npos,
         "expected generated launcher success line");

  fs::remove_all(root);
}

void TestWaydroidPackageMatrixSuccessPath() {
  const auto runtime_runner = [](const std::string& command) -> wfa::CommandResult {
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

void TestWaydroidPackageMatrixCapturesFailure() {
  const auto runtime_runner = [](const std::string& command) -> wfa::CommandResult {
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
    TestRuntimeHealthFixtureRejectsMissingNativeDependencyWithoutFalseSuccess();
    TestRuntimeHealthReplaySummarizesTrace();
    TestNativeArtRuntimeSmokeWritesTraceJsonl();
    TestRuntimeDiagnosticReplayWritesStableArtifacts();
    TestRuntimeDiagnosticReplayHandlesMissingTraceHonestly();
    TestRuntimeDiagnosticReplayReportsMissingNativeDependencyHonestly();
    TestRuntimeDiagnosticReplayCommandWritesStableJson();
    TestRuntimeHealthCommandWritesStableJson();
    TestRuntimeHealthCommandReportsMissingNativeDependencyHonestly();
    TestRuntimeRecoveryPlanWritesStableArtifacts();
    TestRuntimeRecoveryPlanScenariosSelectDeterministicActions();
    TestRuntimeRecoveryPlanCommandWritesStableJson();
    TestNativeArtClassloaderFixtureWritesStableArtifacts();
    TestNativeArtClassloaderFixtureHandlesMissingDexHonestly();
    TestNativeArtClassloaderCommandWritesStableJson();
    TestNativeArtRuntimeSmokeWritesStableArtifacts();
    TestNativeArtRuntimeSmokeHandlesRuntimeAvailabilityHonestly();
    TestNativeArtRuntimeSmokeCommandWritesStableJson();
    TestNativeArtClassResolutionFixtureResolvesManifestTargets();
    TestNativeArtClassResolutionFixtureHandlesMissingDexTargetsHonestly();
    TestNativeArtClassResolutionCommandWritesStableJson();
    TestNativeLifecycleShimWritesSessionArtifacts();
    TestNativeProcessBootstrapRunsFixtureAndWritesSessionState();
    TestNativeExecuteStubReportsMissingNativeLibraryPayload();
    TestNativeExecuteStubRunsFixtureNativeActivity();
    TestRuntimeBridgeOutputParsers();
    TestActivityLaunchReportRendering();
    TestDesktopLaunchArtifactsForImeApp();
    TestDesktopLaunchArtifactsForLoadedApkUseStagedPath();
    TestDesktopLaunchArtifactsRejectCrossPackageComponent();
    TestDesktopLaunchArtifactsRejectUnknownDeclaredComponent();
    TestDesktopLaunchArtifactsRejectServiceLaunchTarget();
    TestAutoDesktopLaunchArtifactsInferLauncherAndSplitRoots();
    TestAutoDesktopLaunchArtifactsRejectHeadlessApp();
    TestWaydroidAppLaunchReportRendering();
    TestInstalledAppLaunchReportRendering();
    TestAttachedAdbInstalledAppLaunchUsesExplicitComponent();
    TestAttachedAdbInstalledAppLaunchAutoResolvesComponent();
    TestAttachedAdbPackageMetadataLookupExtractsLauncherAndVersion();
    TestAttachedAdbRuntimeDiscoveryParsesTargets();
    TestAttachedAdbPreflightAutoSelectsSingleTarget();
    TestAttachedAdbPreflightResolvesComponentWhenOmitted();
    TestAttachedAdbPreflightRequiresSerialWhenMultipleTargetsExist();
    TestAttachedAdbDiscoveryTimeoutReturnsUnavailable();
    TestNativeRuntimePreflightReportsNotImplemented();
    TestWaydroidDesktopLaunchArtifacts();
    TestInstalledPackageDesktopLaunchArtifactsRequireAttachedAdbFields();
    TestWaydroidDesktopLaunchArtifactsRejectInvalidPackage();
    TestInstalledPackageVerificationSuccessPath();
    TestInstalledPackageMatrixSuccessPath();
    TestWaydroidPackageVerificationSuccessPath();
    TestWaydroidPackageMatrixSuccessPath();
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
