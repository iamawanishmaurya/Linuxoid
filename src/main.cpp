#include "wfa/apk_loader.hpp"
#include "wfa/desktop_integration.hpp"
#include "wfa/manifest_assessment.hpp"
#include "wfa/package_layout.hpp"
#include "wfa/project_status.hpp"
#include "wfa/runtime_bridge.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void PrintUsage() {
  std::cout
      << "Usage:\n"
      << "  compatctl status\n"
      << "  compatctl foundation\n"
      << "  compatctl assess-manifest <decoded-manifest.xml>\n"
      << "  compatctl load-apk <apk-path> [compat-root]\n"
      << "  compatctl launch-activity <serial> <component>\n"
      << "  compatctl launch-waydroid-package <package>\n"
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

    if (command == "launch-waydroid-package") {
      if (argc != 3) {
        PrintUsage();
        return EXIT_FAILURE;
      }

      const auto report = wfa::LaunchWaydroidApp(argv[2]);
      std::cout << wfa::RenderWaydroidAppLaunchReport(report);
      return report.launch_ok ? EXIT_SUCCESS : EXIT_FAILURE;
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
