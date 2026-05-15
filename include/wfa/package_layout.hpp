#ifndef WFA_PACKAGE_LAYOUT_HPP
#define WFA_PACKAGE_LAYOUT_HPP

#include <string>

namespace wfa {

struct PackageInstallRequest {
  std::string package_name;
  std::string install_id;
  int version_code;
};

struct PackageLayout {
  std::string host_package_root;
  std::string host_manifest_path;
  std::string host_data_root;
  std::string host_external_data_root;
  std::string host_obb_root;
  std::string guest_base_apk;
  std::string guest_private_data_root;
  std::string guest_external_data_root;
  std::string guest_obb_root;
};

PackageLayout BuildPackageLayout(const PackageInstallRequest& request,
                                 const std::string& compat_root);

}  // namespace wfa

#endif  // WFA_PACKAGE_LAYOUT_HPP
