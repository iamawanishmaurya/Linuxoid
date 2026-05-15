#include "wfa/package_layout.hpp"

#include <regex>
#include <stdexcept>

namespace wfa {

namespace {

std::string TrimTrailingSlash(std::string value) {
  while (value.size() > 1 && value.back() == '/') {
    value.pop_back();
  }
  return value;
}

std::string JoinPath(const std::string& left, const std::string& right) {
  if (left.empty()) {
    return right;
  }
  if (right.empty()) {
    return left;
  }
  if (left.back() == '/') {
    return left + right;
  }
  return left + "/" + right;
}

bool IsValidPackageName(const std::string& package_name) {
  static const std::regex pattern(
      R"(^[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+$)");
  return std::regex_match(package_name, pattern);
}

bool IsValidInstallId(const std::string& install_id) {
  static const std::regex pattern(R"(^[A-Za-z0-9._-]+$)");
  return std::regex_match(install_id, pattern);
}

}  // namespace

PackageLayout BuildPackageLayout(const PackageInstallRequest& request,
                                 const std::string& compat_root) {
  if (!IsValidPackageName(request.package_name)) {
    throw std::invalid_argument("package_name must look like a Java package");
  }

  if (!IsValidInstallId(request.install_id)) {
    throw std::invalid_argument(
        "install_id must contain only letters, digits, dot, underscore, or dash");
  }

  if (request.version_code <= 0) {
    throw std::invalid_argument("version_code must be positive");
  }

  const std::string root = TrimTrailingSlash(compat_root);
  const std::string package_root = JoinPath(
      root, "users/0/packages/" + request.package_name + "/" + request.install_id);
  const std::string data_root =
      JoinPath(root, "users/0/data/" + request.package_name);
  const std::string external_data_root = JoinPath(
      root, "users/0/storage/emulated/0/Android/data/" + request.package_name);
  const std::string obb_root = JoinPath(
      root, "users/0/storage/emulated/0/Android/obb/" + request.package_name);

  return PackageLayout{
      .host_package_root = package_root,
      .host_manifest_path = JoinPath(package_root, "manifest.json"),
      .host_data_root = data_root,
      .host_external_data_root = external_data_root,
      .host_obb_root = obb_root,
      .guest_base_apk = "/data/app/" + request.package_name + "/base.apk",
      .guest_private_data_root = "/data/user/0/" + request.package_name,
      .guest_external_data_root =
          "/storage/emulated/0/Android/data/" + request.package_name,
      .guest_obb_root = "/storage/emulated/0/Android/obb/" + request.package_name,
  };
}

}  // namespace wfa
