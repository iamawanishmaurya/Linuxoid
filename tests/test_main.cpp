#include "wfa/apk_host_integration.hpp"
#include "wfa/apk_loader.hpp"
#include "wfa/checkpoint.hpp"
#include "wfa/desktop_integration.hpp"
#include "wfa/manifest_assessment.hpp"
#include "wfa/native_lifecycle.hpp"
#include "wfa/native_spike.hpp"
#include "wfa/package_layout.hpp"
#include "wfa/project_status.hpp"
#include "wfa/runtime_bridge.hpp"
#include "wfa/waydroid_integration.hpp"

#include <cstdlib>
#include <cstdio>
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

std::filesystem::path ResolveBuildDirFromTestBinary() {
  namespace fs = std::filesystem;
  std::error_code error;
  const fs::path self = fs::read_symlink("/proc/self/exe", error);
  if (error || self.empty()) {
    throw std::runtime_error("unable to resolve test binary path");
  }
  return self.parent_path();
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
  Expect(wfa::CalculateAveragePhaseProgress(phases) == 95,
         "expected average phase progress to equal 95");
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
  Expect(report.find("95/100") != std::string::npos,
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

  const auto rendered = wfa::RenderNativeLifecycleShimReport(lifecycle);
  Expect(rendered.find("Lifecycle Handoff Ready: yes") !=
             std::string::npos,
         "expected lifecycle handoff line");
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
