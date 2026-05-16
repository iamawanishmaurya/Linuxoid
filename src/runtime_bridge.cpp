#include "wfa/runtime_bridge.hpp"

#include <array>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace wfa {

namespace {

std::string QuoteForShell(const std::string& value) {
  std::string quoted = "'";
  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(character);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

std::string RunCommandCapture(const std::string& command) {
  std::array<char, 4096> buffer{};
  std::string output;

  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    throw std::runtime_error("failed to start command: " + command);
  }

  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }

  const int rc = pclose(pipe);
  if (rc != 0) {
    throw std::runtime_error("command failed: " + command + "\n" + output);
  }

  return output;
}

struct CommandResult {
  int exit_code;
  std::string output;
};

CommandResult RunCommandCaptureAllowFailure(const std::string& command) {
  std::array<char, 4096> buffer{};
  std::string output;

  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    throw std::runtime_error("failed to start command: " + command);
  }

  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }

  const int rc = pclose(pipe);
  return CommandResult{rc, output};
}

std::string BuildAdbPrefix(const std::string& serial) {
  return "adb -s " + QuoteForShell(serial);
}

std::vector<std::string> SplitLines(const std::string& output) {
  std::vector<std::string> lines;
  std::istringstream input(output);
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(line);
  }
  return lines;
}

}  // namespace

bool OutputContainsInstalledPackage(const std::string& output,
                                    const std::string& package_name) {
  const std::string expected = "package:" + package_name;
  for (const auto& line : SplitLines(output)) {
    if (line == expected) {
      return true;
    }
  }
  return false;
}

bool OutputContainsImeId(const std::string& output, const std::string& ime_id) {
  for (const auto& line : SplitLines(output)) {
    if (line.rfind(ime_id + ":", 0) == 0 || line == ime_id) {
      return true;
    }
  }
  return false;
}

bool LaunchOutputLooksSuccessful(const std::string& output) {
  return output.find("Status: ok") != std::string::npos &&
         output.find("Complete") != std::string::npos;
}

std::string RenderAdbImeStatusReport(const AdbImeStatus& status) {
  std::ostringstream output;
  output << "ADB Serial: " << status.serial << '\n';
  output << "Package: " << status.package_name << '\n';
  output << "IME ID: " << status.ime_id << '\n';
  output << "Settings Component: " << status.settings_component << '\n';
  output << "Package installed: " << (status.package_installed ? "yes" : "no")
         << '\n';
  output << "IME registered: " << (status.ime_registered ? "yes" : "no")
         << '\n';
  output << "Default IME: " << status.default_input_method << '\n';
  output << "Default matches target: "
         << (status.is_default_ime ? "yes" : "no") << '\n';
  output << "Settings launch OK: ";
  if (status.settings_component.empty()) {
    output << "not checked\n";
  } else {
    output << (status.settings_launch_ok ? "yes" : "no") << '\n';
  }
  return output.str();
}

AdbImeStatus QueryAdbImeStatus(const std::string& serial,
                               const std::string& package_name,
                               const std::string& ime_id,
                               const std::string& settings_component) {
  const std::string prefix = BuildAdbPrefix(serial);
  const std::string package_output =
      RunCommandCapture(prefix + " shell pm list packages");
  const std::string ime_output =
      RunCommandCapture(prefix + " shell ime list -a");
  const std::string default_ime =
      RunCommandCapture(prefix + " shell settings get secure default_input_method");

  AdbImeStatus status;
  status.serial = serial;
  status.package_name = package_name;
  status.ime_id = ime_id;
  status.settings_component = settings_component;
  status.package_installed =
      OutputContainsInstalledPackage(package_output, package_name);
  status.ime_registered = OutputContainsImeId(ime_output, ime_id);
  status.default_input_method = default_ime;
  while (!status.default_input_method.empty() &&
         (status.default_input_method.back() == '\n' ||
          status.default_input_method.back() == '\r')) {
    status.default_input_method.pop_back();
  }
  status.is_default_ime = status.default_input_method == ime_id;
  if (!settings_component.empty()) {
    const auto launch_result = RunCommandCaptureAllowFailure(
        prefix + " shell am start -W -n " + QuoteForShell(settings_component));
    status.settings_launch_ok =
        launch_result.exit_code == 0 &&
        LaunchOutputLooksSuccessful(launch_result.output);
  }
  return status;
}

}  // namespace wfa
