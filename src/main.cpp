#include "wfa/package_layout.hpp"
#include "wfa/project_status.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void PrintUsage() {
  std::cout
      << "Usage:\n"
      << "  compatctl status\n"
      << "  compatctl foundation\n"
      << "  compatctl layout <package> <install-id> <version-code> [compat-root]\n";
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
