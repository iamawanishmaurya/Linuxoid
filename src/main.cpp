#include "wfa/apk_host_integration.hpp"
#include "wfa/apk_loader.hpp"
#include "wfa/art_classloader_fixture.hpp"
#include "wfa/art_class_resolution_fixture.hpp"
#include "wfa/art_runtime_smoke.hpp"
#include "wfa/binder_service_manager.hpp"
#include "wfa/desktop_integration.hpp"
#include "wfa/egl_smoke_fixture.hpp"
#include "wfa/manifest_assessment.hpp"
#include "wfa/native_execute_stub.hpp"
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

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void PrintUsage() {
  std::cout
      << "Usage:\n"
      << "  compatctl status\n"
      << "  compatctl foundation\n"
      << "  compatctl assess-manifest <decoded-manifest.xml>\n"
      << "  compatctl load-apk <apk-path> [compat-root]\n"
      << "  compatctl inspect-apk-resources <apk-path> [resource-root-or-dash]\n"
      << "  compatctl plan-native-spike <apk-path> [compat-root] [native-root]\n"
      << "  compatctl bootstrap-native-spike <apk-path> [compat-root] [native-root]\n"
      << "  compatctl native-art-classloader-fixture <bootstrap-manifest>\n"
      << "  compatctl native-art-class-resolution-fixture <bootstrap-manifest>\n"
      << "  compatctl native-art-runtime-smoke <bootstrap-manifest>\n"
      << "  compatctl native-service-manager-fixture <bootstrap-manifest>\n"
      << "  compatctl native-runtime-health-fixture <bootstrap-manifest> [scenario]\n"
      << "  compatctl native-runtime-health-replay <trace-jsonl-path>\n"
      << "  compatctl native-lifecycle-shim <bootstrap-manifest>\n"
      << "  compatctl native-process-bootstrap <bootstrap-manifest>\n"
      << "  compatctl native-execute-stub <bootstrap-manifest>\n"
      << "  compatctl native-execute-stub <package> <launcher-component> <bundle-apk> <sandbox-root> <dex-cache-root> <resource-root> <library-root> <bootstrap-manifest>\n"
      << "  compatctl native-first-pixel-fixture <session-root> [width] [height] [format]\n"
      << "  compatctl native-egl-smoke-fixture <session-root> [width] [height]\n"
      << "  compatctl native-wayland-surface-fixture <session-root> [width] [height]\n"
      << "  compatctl native-window-bridge-fixture <session-root> [width] [height] [format]\n"
      << "  compatctl native-input-queue-fixture <session-root> [width] [height] [format]\n"
      << "  compatctl native-window-callback-fixture <session-root> [width] [height] [format]\n"
      << "  compatctl discover-runtime <backend>\n"
      << "  compatctl preflight-runtime <backend> [serial] [package] [component]\n"
      << "  compatctl inspect-package <backend> <serial-or-dash> <package>\n"
      << "  compatctl launch-activity <serial> <component>\n"
      << "  compatctl launch-package <backend> <package> [serial] [component]\n"
      << "  compatctl verify-package <backend> <package> [serial-or-dash] [component-or-dash] [desktop-entry-root] [launcher-root]\n"
      << "  compatctl verify-package-matrix <backend> <artifact-root> <serial-or-dash> <package-spec> [package-spec...]\n"
      << "  compatctl verify-apk-host-launch-auto <serial> <apk-path> [compat-root] [desktop-entry-root] [launcher-root]\n"
      << "  compatctl launch-waydroid-package <package>\n"
      << "  compatctl verify-waydroid-package <package> [desktop-entry-root] [launcher-root]\n"
      << "  compatctl verify-waydroid-matrix <artifact-root> <package> [package...]\n"
      << "  compatctl adb-ime-status <serial> <package> <ime-id> [settings-component]\n"
      << "  compatctl provision-ime <serial> <apk-path> <package> <ime-id> [settings-component]\n"
      << "  compatctl desktopify-apk <serial> <apk-path> <component> [compat-root] [desktop-entry-root] [launcher-root]\n"
      << "  compatctl desktopify-apk-auto <serial> <apk-path> [compat-root] [desktop-entry-root] [launcher-root]\n"
      << "  compatctl desktopify-waydroid-package <package> [desktop-entry-root] [launcher-root]\n"
      << "  compatctl layout <package> <install-id> <version-code> [compat-root]\n";
}

std::string ReadFile(const std::string& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to open file: " + path);
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void WriteFile(const std::string& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to open file for write: " + path);
  }
  output << contents;
}

std::string ResolveCompatctlPath(const char* argv0) {
  namespace fs = std::filesystem;
  std::error_code error;

  const fs::path proc_self_exe = fs::read_symlink("/proc/self/exe", error);
  if (!error && !proc_self_exe.empty()) {
    return proc_self_exe.string();
  }

  if (argv0 != nullptr && *argv0 != '\0') {
    return fs::absolute(argv0).string();
  }

  throw std::runtime_error("unable to resolve compatctl executable path");
}

std::string DefaultDesktopEntryRoot() {
  if (const char* home = std::getenv("HOME"); home != nullptr) {
    return std::string(home) + "/.local/share/applications";
  }
  return "/tmp/linuxoid-applications";
}

std::string DefaultLauncherRoot() {
  if (const char* home = std::getenv("HOME"); home != nullptr) {
    return std::string(home) + "/.local/share/linuxoid/launchers";
  }
  return "/tmp/linuxoid-launchers";
}

std::string DefaultVerificationCompatRoot() {
  return "/tmp/linuxoid-apk-verify";
}

std::string DefaultNativeSpikeCompatRoot() {
  return "/tmp/linuxoid-native-compat";
}

std::string DefaultNativeSpikeRoot() {
  return "/tmp/linuxoid-native-spike";
}

std::string OptionalArgOrEmpty(const char* value) {
  if (value == nullptr) {
    return "";
  }

  const std::string parsed = value;
  return parsed == "-" ? "" : parsed;
}

struct MatrixPackageSpec {
  std::string package_name;
  std::string component;
};

MatrixPackageSpec ParseMatrixPackageSpec(const std::string& value) {
  if (value.empty()) {
    throw std::invalid_argument("matrix package spec must not be empty");
  }

  const auto separator = value.find('=');
  if (separator == std::string::npos) {
    return {.package_name = value, .component = ""};
  }
  if (separator == 0 || separator == value.size() - 1) {
    throw std::invalid_argument(
        "matrix package spec must use package=component when a component is provided");
  }

  return {.package_name = value.substr(0, separator),
          .component = value.substr(separator + 1)};
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    PrintUsage();
    return EXIT_FAILURE;
  }

  const std::string command = argv[1];

  try {
    const std::string compatctl_path = ResolveCompatctlPath(argv[0]);

    if (command == "status") {
      std::cout << wfa::RenderProjectStatusReport();
      return EXIT_SUCCESS;
    }

    if (command == "foundation") {
      std::cout << wfa::DescribeMvpFoundation();
      return EXIT_SUCCESS;
    }

    if (command == "assess-manifest") {
      if (argc != 3) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto profile = wfa::ParseDecodedManifest(ReadFile(argv[2]));
      const auto assessment = wfa::AssessRuntimeRequirements(profile);
      std::cout << wfa::RenderManifestAssessmentReport(assessment);
      return EXIT_SUCCESS;
    }

    if (command == "load-apk") {
      if (argc < 3 || argc > 4) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string compat_root =
          argc == 4 ? argv[3] : "/var/lib/wfa";
      const auto report = wfa::LoadApkToCompatRoot(argv[2], compat_root);
      std::cout << wfa::RenderLoadedApkReport(report);
      return EXIT_SUCCESS;
    }

    if (command == "inspect-apk-resources") {
      if (argc < 3 || argc > 4) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string resource_root =
          argc == 4 ? OptionalArgOrEmpty(argv[3]) : "";
      const auto report = wfa::InspectApkResourceReadiness(argv[2], resource_root);
      std::cout << wfa::RenderApkResourceReadinessJson(report);
      return report.manifest.manifest_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "plan-native-spike") {
      if (argc < 3 || argc > 5) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string compat_root =
          argc >= 4 ? argv[3] : DefaultNativeSpikeCompatRoot();
      const std::string native_root =
          argc == 5 ? argv[4] : DefaultNativeSpikeRoot();
      const auto plan =
          wfa::PlanNativeLaunchSpike(argv[2], compat_root, native_root);
      std::cout << wfa::RenderNativeLaunchPlanReport(plan);
      return plan.plan_written ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "bootstrap-native-spike") {
      if (argc < 3 || argc > 5) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string compat_root =
          argc >= 4 ? argv[3] : DefaultNativeSpikeCompatRoot();
      const std::string native_root =
          argc == 5 ? argv[4] : DefaultNativeSpikeRoot();
      const auto bootstrap = wfa::BootstrapNativeLaunchSpike(
          argv[2], compat_root, native_root, compatctl_path);
      std::cout << wfa::RenderNativeActivityBootstrapReport(bootstrap);
      return bootstrap.bootstrap_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "native-art-classloader-fixture") {
      if (argc != 3) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto report = wfa::RunNativeArtClassloaderFixture(argv[2]);
      std::cout << wfa::RenderNativeArtClassloaderFixtureJson(report);
      return report.classpath_plan_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "native-art-class-resolution-fixture") {
      if (argc != 3) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto report = wfa::RunNativeArtClassResolutionFixture(argv[2]);
      std::cout << wfa::RenderNativeArtClassResolutionFixtureJson(report);
      return report.classpath_plan_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "native-art-runtime-smoke") {
      if (argc != 3) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto report = wfa::RunNativeArtRuntimeSmokeFixture(argv[2]);
      std::cout << wfa::RenderNativeArtRuntimeSmokeFixtureJson(report);
      return report.classpath_plan_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "native-service-manager-fixture") {
      if (argc != 3) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto lifecycle =
          wfa::BuildNativeLifecycleShimFromManifest(argv[2]);
      std::cout << wfa::RenderBinderServiceManagerFixtureJson(
          lifecycle.binder_service_manager);
      return lifecycle.binder_service_manager.manager_ready ? EXIT_SUCCESS
                                                            : EXIT_FAILURE;
    }

    if (command == "native-runtime-health-fixture") {
      if (argc < 3 || argc > 4) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string scenario = argc == 4 ? argv[3] : "baseline";
      const auto report = wfa::RunRuntimeHealthFixture(argv[2], scenario);
      std::cout << wfa::RenderRuntimeHealthReportJson(report);
      return report.self_healing_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "native-runtime-health-replay") {
      if (argc != 3) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto report = wfa::ReplayRuntimeHealthTrace(argv[2]);
      std::cout << wfa::RenderRuntimeHealthReplayJson(report);
      return EXIT_SUCCESS;
    }

    if (command == "native-lifecycle-shim") {
      if (argc != 3) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto lifecycle =
          wfa::BuildNativeLifecycleShimFromManifest(argv[2]);
      std::cout << wfa::RenderNativeLifecycleShimReport(lifecycle);
      return lifecycle.execution_engine_ready ? EXIT_SUCCESS : 2;
    }

    if (command == "native-process-bootstrap") {
      if (argc != 3) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto lifecycle =
          wfa::RunNativeProcessBootstrapFromManifest(argv[2]);
      std::cout << wfa::RenderNativeProcessBootstrapJson(lifecycle);
      return lifecycle.exit_code == -1 ? EXIT_FAILURE : lifecycle.exit_code;
    }

    if (command == "native-execute-stub") {
      if (argc == 3) {
        const auto lifecycle =
            wfa::RunNativeProcessBootstrapFromManifest(argv[2]);
        std::cout << wfa::RenderNativeProcessBootstrapJson(lifecycle);
        return lifecycle.exit_code == -1 ? EXIT_FAILURE : lifecycle.exit_code;
      }

      if (argc != 10) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto report = wfa::ExecuteNativeStub(
          {.package_name = argv[2],
           .launcher_component = argv[3],
           .bundle_apk_path = argv[4],
           .sandbox_root = argv[5],
           .dex_cache_root = argv[6],
           .resource_root = argv[7],
           .library_root = argv[8],
           .bootstrap_manifest_path = argv[9]});
      const std::string json = wfa::RenderNativeExecuteReportJson(report);
      if (const char* report_path = std::getenv("LINUXOID_RUNNER_REPORT_PATH");
          report_path != nullptr && *report_path != '\0') {
        WriteFile(report_path, json);
        std::cout << report.output;
      } else {
        std::cout << json;
      }
      return report.exit_code;
    }

    if (command == "native-first-pixel-fixture") {
      if (argc < 3 || argc > 6) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      wfa::NativeWindowMetadata metadata{
          .width = argc >= 4 ? std::stoi(argv[3]) : 64,
          .height = argc >= 5 ? std::stoi(argv[4]) : 48,
          .format = argc == 6 ? std::stoi(argv[5])
                              : wfa::kNativeWindowFormatRgba8888,
          .stride = argc >= 4 ? std::stoi(argv[3]) : 64,
      };
      const auto report = wfa::RunHeadlessFirstPixelFixture(
          argv[2], metadata, 0xff336699u);
      std::cout << wfa::RenderFirstPixelFixtureJson(report);
      return report.render_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "native-egl-smoke-fixture") {
      if (argc < 3 || argc > 5) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      wfa::NativeWindowMetadata metadata{
          .width = argc >= 4 ? std::stoi(argv[3]) : 64,
          .height = argc == 5 ? std::stoi(argv[4]) : 48,
          .format = wfa::kNativeWindowFormatRgba8888,
          .stride = argc >= 4 ? std::stoi(argv[3]) : 64,
      };
      const auto report = wfa::RunEglSmokeFixture(argv[2], metadata);
      std::cout << wfa::RenderEglSmokeFixtureJson(report);
      return report.context_created && report.pbuffer_created ? EXIT_SUCCESS
                                                              : EXIT_FAILURE;
    }

    if (command == "native-wayland-surface-fixture") {
      if (argc < 3 || argc > 5) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      wfa::NativeWindowMetadata metadata{
          .width = argc >= 4 ? std::stoi(argv[3]) : 64,
          .height = argc == 5 ? std::stoi(argv[4]) : 48,
          .format = wfa::kNativeWindowFormatRgba8888,
          .stride = argc >= 4 ? std::stoi(argv[3]) : 64,
      };
      const auto report = wfa::RunWaylandSurfaceFixture(argv[2], metadata);
      std::cout << wfa::RenderWaylandSurfaceFixtureJson(report);
      return report.surface_created ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "native-window-bridge-fixture") {
      if (argc < 3 || argc > 6) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      wfa::NativeWindowMetadata metadata{
          .width = argc >= 4 ? std::stoi(argv[3]) : 64,
          .height = argc >= 5 ? std::stoi(argv[4]) : 48,
          .format = argc == 6 ? std::stoi(argv[5])
                              : wfa::kNativeWindowFormatRgba8888,
          .stride = argc >= 4 ? std::stoi(argv[3]) : 64,
      };
      const auto report =
          wfa::RunNativeWindowBridgeFixture(argv[2], metadata);
      std::cout << wfa::RenderNativeWindowBridgeFixtureJson(report);
      return report.native_window_bridge_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "native-input-queue-fixture") {
      if (argc < 3 || argc > 6) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      wfa::NativeWindowMetadata metadata{
          .width = argc >= 4 ? std::stoi(argv[3]) : 64,
          .height = argc >= 5 ? std::stoi(argv[4]) : 48,
          .format = argc == 6 ? std::stoi(argv[5])
                              : wfa::kNativeWindowFormatRgba8888,
          .stride = argc >= 4 ? std::stoi(argv[3]) : 64,
      };
      const auto report = wfa::RunNativeInputQueueFixture(argv[2], metadata);
      std::cout << wfa::RenderNativeInputQueueFixtureJson(report);
      return report.input_queue_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "native-window-callback-fixture") {
      if (argc < 3 || argc > 6) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      wfa::NativeWindowMetadata metadata{
          .width = argc >= 4 ? std::stoi(argv[3]) : 64,
          .height = argc >= 5 ? std::stoi(argv[4]) : 48,
          .format = argc == 6 ? std::stoi(argv[5])
                              : wfa::kNativeWindowFormatRgba8888,
          .stride = argc >= 4 ? std::stoi(argv[3]) : 64,
      };
      const auto report =
          wfa::RunHeadlessNativeWindowCallbackFixture(argv[2], metadata);
      std::cout << wfa::RenderNativeWindowCallbackFixtureJson(report);
      return report.callbacks_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "discover-runtime") {
      if (argc != 3) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto report =
          wfa::DiscoverRuntimeTargets(wfa::ParseRuntimeBackendKind(argv[2]));
      std::cout << wfa::RenderRuntimeDiscoveryReport(report);
      return EXIT_SUCCESS;
    }

    if (command == "preflight-runtime") {
      if (argc < 3 || argc > 6) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string serial = argc >= 4 ? OptionalArgOrEmpty(argv[3]) : "";
      const std::string package_name = argc >= 5 ? OptionalArgOrEmpty(argv[4]) : "";
      const std::string component = argc == 6 ? OptionalArgOrEmpty(argv[5]) : "";
      const auto report = wfa::PreflightRuntime(
          {.backend = wfa::ParseRuntimeBackendKind(argv[2]),
           .serial = serial,
           .package_name = package_name,
           .component = component});
      std::cout << wfa::RenderRuntimePreflightReport(report);
      return report.ready_for_launch ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "inspect-package") {
      if (argc != 5) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto report = wfa::QueryInstalledPackageMetadata(
          {.backend = wfa::ParseRuntimeBackendKind(argv[2]),
           .serial = OptionalArgOrEmpty(argv[3]),
           .package_name = argv[4]});
      std::cout << wfa::RenderInstalledPackageMetadataReport(report);
      return report.package_visible ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "adb-ime-status") {
      if (argc != 5 && argc != 6) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string settings_component = argc == 6 ? argv[5] : "";
      const auto status =
          wfa::QueryAdbImeStatus(argv[2], argv[3], argv[4], settings_component);
      std::cout << wfa::RenderAdbImeStatusReport(status);
      return EXIT_SUCCESS;
    }

    if (command == "launch-activity") {
      if (argc != 4) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto report = wfa::LaunchAdbActivity(argv[2], argv[3]);
      std::cout << wfa::RenderAdbActivityLaunchReport(report);
      return report.launch_ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "launch-package") {
      if (argc < 4 || argc > 6) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string serial = argc >= 5 ? OptionalArgOrEmpty(argv[4]) : "";
      const std::string component = argc == 6 ? OptionalArgOrEmpty(argv[5]) : "";
      const auto report = wfa::LaunchInstalledApp(
          {.backend = wfa::ParseRuntimeBackendKind(argv[2]),
           .serial = serial,
           .package_name = argv[3],
           .component = component});
      std::cout << wfa::RenderInstalledAppLaunchReport(report);
      return report.launch_ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "launch-waydroid-package") {
      if (argc != 3) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto report = wfa::LaunchInstalledApp(
          {.backend = wfa::RuntimeBackendKind::kWaydroid,
           .package_name = argv[2]});
      std::cout << wfa::RenderInstalledAppLaunchReport(report);
      return report.launch_ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "verify-apk-host-launch-auto") {
      if (argc < 4 || argc > 7) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string compat_root =
          argc >= 5 ? argv[4] : DefaultVerificationCompatRoot();
      const std::string desktop_root =
          argc >= 6 ? argv[5] : DefaultDesktopEntryRoot();
      const std::string launcher_root =
          argc == 7 ? argv[6]
                    : (argc >= 6 ? argv[5] : DefaultLauncherRoot());
      const auto report = wfa::VerifyApkHostLaunchAuto(
          argv[3], compat_root,
          {.serial = argv[2],
           .compatctl_path = compatctl_path,
           .desktop_root = desktop_root,
           .launcher_root = launcher_root});
      std::cout << wfa::RenderApkHostVerificationReport(report);
      return report.apk_load_ok && report.launcher_generation_ok &&
                     report.generated_launcher_ok
                 ? EXIT_SUCCESS
                 : EXIT_FAILURE;
    }

    if (command == "verify-package") {
      if (argc < 4 || argc > 8) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto backend = wfa::ParseRuntimeBackendKind(argv[2]);
      const std::string serial = argc >= 5 ? OptionalArgOrEmpty(argv[4]) : "";
      const std::string component =
          argc >= 6 ? OptionalArgOrEmpty(argv[5]) : "";
      const std::string desktop_root =
          argc >= 7 ? argv[6] : DefaultDesktopEntryRoot();
      const std::string launcher_root =
          argc == 8 ? argv[7]
                    : (argc >= 7 ? argv[6] : DefaultLauncherRoot());
      const auto report = wfa::VerifyInstalledPackage(
          {.backend = backend,
           .app_name = argv[3],
           .serial = serial,
           .package_name = argv[3],
           .component = component,
           .compatctl_path = compatctl_path,
           .desktop_root = desktop_root,
           .launcher_root = launcher_root});
      std::cout << wfa::RenderInstalledPackageVerificationReport(report);
      return report.direct_launch_ok && report.launcher_generation_ok &&
                     report.generated_launcher_ok
                 ? EXIT_SUCCESS
                 : EXIT_FAILURE;
    }

    if (command == "verify-package-matrix") {
      if (argc < 6) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto backend = wfa::ParseRuntimeBackendKind(argv[2]);
      const std::string serial = OptionalArgOrEmpty(argv[4]);
      std::vector<wfa::InstalledPackageVerificationSpec> specs;
      specs.reserve(static_cast<std::size_t>(argc - 5));
      for (int index = 5; index < argc; ++index) {
        const auto parsed = ParseMatrixPackageSpec(argv[index]);
        specs.push_back({.backend = backend,
                         .app_name = parsed.package_name,
                         .serial = serial,
                         .package_name = parsed.package_name,
                         .component = parsed.component,
                         .compatctl_path = compatctl_path});
      }

      const auto report = wfa::VerifyInstalledPackageMatrix(specs, argv[3]);
      std::cout << wfa::RenderInstalledPackageMatrixReport(report);
      for (const auto& entry : report.entries) {
        if (!entry.verification_ok) {
          return EXIT_FAILURE;
        }
      }
      return EXIT_SUCCESS;
    }

    if (command == "verify-waydroid-package") {
      if (argc < 3 || argc > 5) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string desktop_root =
          argc >= 4 ? argv[3] : DefaultDesktopEntryRoot();
      const std::string launcher_root =
          argc == 5 ? argv[4]
                    : (argc >= 4 ? argv[3] : DefaultLauncherRoot());
      const auto report = wfa::VerifyWaydroidPackage(
          {.app_name = argv[2],
           .package_name = argv[2],
           .compatctl_path = compatctl_path,
           .desktop_root = desktop_root,
           .launcher_root = launcher_root});
      std::cout << wfa::RenderWaydroidPackageVerificationReport(report);
      return report.direct_launch_ok && report.launcher_generation_ok &&
                     report.generated_launcher_ok
                 ? EXIT_SUCCESS
                 : EXIT_FAILURE;
    }

    if (command == "verify-waydroid-matrix") {
      if (argc < 4) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      std::vector<std::string> packages;
      packages.reserve(static_cast<std::size_t>(argc - 3));
      for (int index = 3; index < argc; ++index) {
        packages.push_back(argv[index]);
      }

      const auto report = wfa::VerifyWaydroidPackageMatrix(
          packages, compatctl_path, argv[2]);
      std::cout << wfa::RenderWaydroidMatrixReport(report);
      for (const auto& entry : report.entries) {
        if (!entry.verification_ok) {
          return EXIT_FAILURE;
        }
      }
      return EXIT_SUCCESS;
    }

    if (command == "provision-ime") {
      if (argc != 6 && argc != 7) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string settings_component = argc == 7 ? argv[6] : "";
      const auto report = wfa::ProvisionAdbIme(
          argv[2], argv[3], argv[4], argv[5], settings_component);
      std::cout << wfa::RenderAdbProvisioningReport(report);
      return report.ready_for_typing ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "desktopify-apk") {
      if (argc < 5 || argc > 8) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string compat_root = argc >= 6 ? argv[5] : "/var/lib/wfa";
      const std::string desktop_root =
          argc >= 7 ? argv[6] : DefaultDesktopEntryRoot();
      const std::string launcher_root =
          argc == 8 ? argv[7]
                    : (argc >= 7 ? argv[6] : DefaultLauncherRoot());
      const auto artifacts = wfa::DesktopifyApk(
          argv[2], argv[3], argv[4], compat_root, desktop_root, launcher_root,
          compatctl_path);
      std::cout << wfa::RenderDesktopLaunchArtifactsReport(artifacts);
      return artifacts.host_launch_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "desktopify-apk-auto") {
      if (argc < 4 || argc > 7) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string compat_root = argc >= 5 ? argv[4] : "/var/lib/wfa";
      const std::string desktop_root =
          argc >= 6 ? argv[5] : DefaultDesktopEntryRoot();
      const std::string launcher_root =
          argc == 7 ? argv[6]
                    : (argc >= 6 ? argv[5] : DefaultLauncherRoot());
      const auto artifacts = wfa::DesktopifyApkAuto(
          argv[2], argv[3], compat_root, desktop_root, launcher_root,
          compatctl_path);
      std::cout << wfa::RenderDesktopLaunchArtifactsReport(artifacts);
      return artifacts.host_launch_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "desktopify-waydroid-package") {
      if (argc < 3 || argc > 5) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string desktop_root =
          argc >= 4 ? argv[3] : DefaultDesktopEntryRoot();
      const std::string launcher_root =
          argc == 5 ? argv[4]
                    : (argc >= 4 ? argv[3] : DefaultLauncherRoot());
      const auto artifacts = wfa::DesktopifyWaydroidPackage(
          argv[2], desktop_root, launcher_root, compatctl_path);
      std::cout << wfa::RenderWaydroidDesktopLaunchArtifactsReport(artifacts);
      return artifacts.host_launch_ready ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (command == "layout") {
      if (argc < 5 || argc > 6) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const std::string compat_root =
          argc == 6 ? argv[5] : "/var/lib/wfa";
      const wfa::PackageInstallRequest request{
          .package_name = argv[2],
          .install_id = argv[3],
          .version_code = std::stoi(argv[4]),
      };

      const auto layout = wfa::BuildPackageLayout(request, compat_root);
      std::cout << wfa::RenderPackageLayoutReport(layout);
      return EXIT_SUCCESS;
    }
  } catch (const std::exception& error) {
    std::cerr << "compatctl error: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  PrintUsage();
  return EXIT_FAILURE;
}
