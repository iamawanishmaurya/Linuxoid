#include "wfa/manifest_assessment.hpp"
#include "wfa/package_layout.hpp"
#include "wfa/project_status.hpp"

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
