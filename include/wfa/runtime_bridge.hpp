#ifndef WFA_RUNTIME_BRIDGE_HPP
#define WFA_RUNTIME_BRIDGE_HPP

#include <string>

namespace wfa {

struct AdbImeStatus {
  std::string serial;
  std::string package_name;
  std::string ime_id;
  std::string settings_component;
  bool package_installed = false;
  bool ime_registered = false;
  bool is_default_ime = false;
  bool settings_launch_ok = false;
  std::string default_input_method;
};

bool OutputContainsInstalledPackage(const std::string& output,
                                    const std::string& package_name);
bool OutputContainsImeId(const std::string& output, const std::string& ime_id);
bool LaunchOutputLooksSuccessful(const std::string& output);
std::string RenderAdbImeStatusReport(const AdbImeStatus& status);
AdbImeStatus QueryAdbImeStatus(const std::string& serial,
                               const std::string& package_name,
                               const std::string& ime_id,
                               const std::string& settings_component = "");

}  // namespace wfa

#endif  // WFA_RUNTIME_BRIDGE_HPP
