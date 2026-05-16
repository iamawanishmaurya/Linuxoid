#include "wfa/apk_loader.hpp"
#include "wfa/manifest_assessment.hpp"
#include "wfa/package_layout.hpp"
#include "wfa/project_status.hpp"
#include "wfa/runtime_bridge.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
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
      << "  compatctl adb-ime-status <serial> <package> <ime-id> [settings-component]\n"
      << "  compatctl provision-ime <serial> <apk-path> <package> <ime-id> [settings-component]\n"
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

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    PrintUsage();
    return EXIT_FAILURE;
  }

  const std::string command = argv[1];

  try {
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
