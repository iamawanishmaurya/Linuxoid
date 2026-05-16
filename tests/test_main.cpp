#include "wfa/apk_host_integration.hpp"
#include "wfa/apk_loader.hpp"
#include "wfa/checkpoint.hpp"
#include "wfa/desktop_integration.hpp"
#include "wfa/manifest_assessment.hpp"
#include "wfa/package_layout.hpp"
#include "wfa/project_status.hpp"
#include "wfa/runtime_bridge.hpp"
#include "wfa/waydroid_integration.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
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
  Expect(wfa::CalculateAveragePhaseProgress(phases) == 91,
         "expected average phase progress to equal 91");
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

  Expect(report.find("Phase Loading") != std::string::npos,
         "expected phase loading heading");
  Expect(report.find("91/100") != std::string::npos,
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
    TestWaydroidDesktopLaunchArtifacts();
    TestInstalledPackageDesktopLaunchArtifactsRequireAttachedAdbFields();
    TestWaydroidDesktopLaunchArtifactsRejectInvalidPackage();
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
