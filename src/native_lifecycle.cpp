#include "wfa/native_lifecycle.hpp"

#include "wfa/native_execute_stub.hpp"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

std::string ReadFile(const fs::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to read file: " + path.string());
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string EscapeJson(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (const char character : value) {
    switch (character) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      default:
        escaped.push_back(character);
        break;
    }
  }
  return escaped;
}

std::string ExtractJsonString(const std::string& json,
                              const std::string& key) {
  const std::regex pattern("\"" + key + R"(\"\s*:\s*\"([^\"]*)\")");
  std::smatch match;
  if (!std::regex_search(json, match, pattern) || match.size() != 2) {
    throw std::runtime_error("unable to extract string field from json: " +
                             key);
  }
  return match[1].str();
}

std::string ExtractJsonArrayLiteral(const std::string& json,
                                    const std::string& key) {
  const std::string token = "\"" + key + "\"";
  const std::size_t key_position = json.find(token);
  if (key_position == std::string::npos) {
    throw std::runtime_error("unable to extract array field from json: " +
                             key);
  }

  const std::size_t array_start = json.find('[', key_position);
  if (array_start == std::string::npos) {
    throw std::runtime_error("unable to locate array start in json: " + key);
  }

  int depth = 0;
  for (std::size_t index = array_start; index < json.size(); ++index) {
    if (json[index] == '[') {
      ++depth;
    } else if (json[index] == ']') {
      --depth;
      if (depth == 0) {
        return json.substr(array_start, index - array_start + 1);
      }
    }
  }

  throw std::runtime_error("unterminated array in json: " + key);
}

std::vector<std::string> ExtractJsonStringArray(const std::string& json,
                                                const std::string& key) {
  const std::string literal = ExtractJsonArrayLiteral(json, key);
  const std::regex pattern("\"([^\"]*)\"");
  std::vector<std::string> values;
  for (auto it = std::sregex_iterator(literal.begin(), literal.end(), pattern);
       it != std::sregex_iterator(); ++it) {
    values.push_back((*it)[1].str());
  }
  return values;
}

bool ExtractJsonBool(const std::string& json, const std::string& key) {
  const std::regex pattern("\"" + key + R"(\"\s*:\s*(true|false))");
  std::smatch match;
  if (!std::regex_search(json, match, pattern) || match.size() != 2) {
    throw std::runtime_error("unable to extract boolean field from json: " +
                             key);
  }
  return match[1].str() == "true";
}

bool ExtractJsonBoolOrDefault(const std::string& json, const std::string& key,
                              bool fallback) {
  const std::regex pattern("\"" + key + R"(\"\s*:\s*(true|false))");
  std::smatch match;
  if (!std::regex_search(json, match, pattern) || match.size() != 2) {
    return fallback;
  }
  return match[1].str() == "true";
}

int ExtractJsonInt(const std::string& json, const std::string& key) {
  const std::regex pattern("\"" + key + R"(\"\s*:\s*(-?\d+))");
  std::smatch match;
  if (!std::regex_search(json, match, pattern) || match.size() != 2) {
    throw std::runtime_error("unable to extract integer field from json: " +
                             key);
  }
  return std::stoi(match[1].str());
}

int ExtractJsonIntOrDefault(const std::string& json, const std::string& key,
                            int fallback) {
  const std::regex pattern("\"" + key + R"(\"\s*:\s*(-?\d+))");
  std::smatch match;
  if (!std::regex_search(json, match, pattern) || match.size() != 2) {
    return fallback;
  }
  return std::stoi(match[1].str());
}

std::string RenderJsonArray(const std::vector<std::string>& values) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    output << "\"" << EscapeJson(values[index]) << "\"";
  }
  output << "]";
  return output.str();
}

NativeActivityBootstrap ReadNativeActivityBootstrapManifest(
    const std::string& bootstrap_manifest_path) {
  if (bootstrap_manifest_path.empty()) {
    throw std::invalid_argument("bootstrap_manifest_path must not be empty");
  }

  const fs::path manifest_path(bootstrap_manifest_path);
  const std::string json = ReadFile(manifest_path);
  const fs::path bootstrap_root = manifest_path.parent_path();
  const fs::path package_root = bootstrap_root.parent_path();

  NativeActivityBootstrap bootstrap;
  bootstrap.plan.assessment.package_name =
      ExtractJsonString(json, "package_name");
  bootstrap.plan.assessment.install_id = ExtractJsonString(json, "install_id");
  bootstrap.plan.apk_path = ExtractJsonString(json, "apk_path");
  bootstrap.plan.assessment.launcher_component =
      ExtractJsonString(json, "launcher_component");
  bootstrap.plan.bundle_apk_path = ExtractJsonString(json, "bundle_apk_path");
  bootstrap.plan.sandbox_root = ExtractJsonString(json, "sandbox_root");
  bootstrap.plan.dex_cache_root = ExtractJsonString(json, "dex_cache_root");
  bootstrap.plan.resource_root = ExtractJsonString(json, "resource_root");
  bootstrap.plan.asset_root = ExtractJsonString(json, "asset_root");
  bootstrap.plan.library_root = ExtractJsonString(json, "library_root");
  bootstrap.plan.selected_abi = ExtractJsonString(json, "selected_abi");
  bootstrap.plan.host_abi_supported =
      ExtractJsonBool(json, "host_abi_supported");
  bootstrap.plan.native_libraries_declared =
      ExtractJsonBoolOrDefault(json, "native_libraries_declared", false);
  bootstrap.plan.discovered_native_library_count =
      ExtractJsonIntOrDefault(json, "discovered_native_library_count", 0);
  bootstrap.plan.staged_native_libraries =
      ExtractJsonStringArray(json, "staged_native_libraries");
  bootstrap.plan.unsupported_native_libraries =
      ExtractJsonStringArray(json, "unsupported_native_libraries");
  bootstrap.plan.bootstrap_spec_path =
      ExtractJsonString(json, "bootstrap_spec_path");
  bootstrap.plan.package_root = package_root.string();
  bootstrap.plan.bootstrap_root = bootstrap_root.string();
  bootstrap.plan.plan_written = true;
  bootstrap.bootstrap_manifest_path = manifest_path.string();
  bootstrap.env_script_path = (bootstrap_root / "native-env.sh").string();
  bootstrap.entrypoint_script_path =
      (bootstrap_root / "launch-native-activity.sh").string();
  bootstrap.report_path = (bootstrap_root / "bootstrap-report.txt").string();
  bootstrap.command_line = ExtractJsonString(json, "command_line");
  bootstrap.execution_engine_ready =
      ExtractJsonBool(json, "execution_engine_ready");
  bootstrap.bootstrap_ready = fs::exists(bootstrap.bootstrap_manifest_path) &&
                              fs::exists(bootstrap.env_script_path) &&
                              fs::exists(bootstrap.entrypoint_script_path) &&
                              fs::exists(bootstrap.plan.bundle_apk_path);
  return bootstrap;
}

std::vector<NativeServiceBinding> BuildDefaultServiceBindings() {
  return {
      {"activity_manager", "lifecycle", "ready",
       "Tracks activity creation, start, resume, and stop transitions for the native bootstrap session."},
      {"package_manager", "metadata", "ready",
       "Exposes package identity, launcher resolution, and staged APK metadata to the native bootstrap session."},
      {"resource_loader", "resources", "stub",
       "Owns resource-path handoff until the real asset and graphics pipeline is attached."},
      {"binder_registry", "ipc", "stub",
       "Reserves the service-discovery seam for a future Binder-compatible implementation."},
  };
}

std::vector<NativeServiceBinding> BuildServiceBindingsFromBinderFixture(
    const BinderServiceManagerFixtureReport& binder_fixture) {
  std::vector<NativeServiceBinding> services;
  services.reserve(binder_fixture.services.size());
  for (const auto& registration : binder_fixture.services) {
    services.push_back({registration.service_name, registration.interface_name,
                        registration.status, registration.notes});
  }
  return services;
}

bool FileExists(const std::string& path) {
  return !path.empty() && fs::exists(path);
}

void WriteLifecycleArtifacts(const NativeLifecycleShim& lifecycle) {
  std::ostringstream session_manifest;
  session_manifest << "{\n"
                   << "  \"package_name\": \""
                   << EscapeJson(lifecycle.bootstrap.plan.assessment.package_name)
                   << "\",\n"
                   << "  \"install_id\": \""
                   << EscapeJson(lifecycle.bootstrap.plan.assessment.install_id)
                   << "\",\n"
                   << "  \"launcher_component\": \""
                   << EscapeJson(
                          lifecycle.bootstrap.plan.assessment.launcher_component)
                   << "\",\n"
                   << "  \"bootstrap_manifest_path\": \""
                   << EscapeJson(lifecycle.bootstrap.bootstrap_manifest_path)
                   << "\",\n"
                   << "  \"apk_path\": \""
                   << EscapeJson(lifecycle.bootstrap.plan.apk_path)
                   << "\",\n"
                   << "  \"selected_abi\": \""
                   << EscapeJson(lifecycle.bootstrap.plan.selected_abi)
                   << "\",\n"
                   << "  \"host_abi_supported\": "
                   << (lifecycle.bootstrap.plan.host_abi_supported ? "true"
                                                                   : "false")
                   << ",\n"
                   << "  \"native_libraries_declared\": "
                   << (lifecycle.bootstrap.plan.native_libraries_declared
                           ? "true"
                           : "false")
                   << ",\n"
                   << "  \"discovered_native_library_count\": "
                   << lifecycle.bootstrap.plan.discovered_native_library_count
                   << ",\n"
                   << "  \"staged_native_libraries\": "
                   << RenderJsonArray(
                          lifecycle.bootstrap.plan.staged_native_libraries)
                   << ",\n"
                   << "  \"unsupported_native_libraries\": "
                   << RenderJsonArray(
                          lifecycle.bootstrap.plan.unsupported_native_libraries)
                   << ",\n"
                   << "  \"binder_service_manager_ready\": "
                   << (lifecycle.binder_service_manager_ready ? "true"
                                                              : "false")
                   << ",\n"
                   << "  \"binder_manager_metadata_path\": \""
                   << EscapeJson(lifecycle.binder_manager_metadata_path)
                   << "\",\n"
                   << "  \"binder_lookup_log_path\": \""
                   << EscapeJson(lifecycle.binder_lookup_log_path) << "\",\n"
                   << "  \"binder_transaction_log_path\": \""
                   << EscapeJson(lifecycle.binder_transaction_log_path)
                   << "\",\n"
                   << "  \"binder_transport_log_path\": \""
                   << EscapeJson(lifecycle.binder_transport_log_path)
                   << "\",\n"
                   << "  \"activity_state\": \""
                   << EscapeJson(lifecycle.current_activity_state) << "\",\n"
                   << "  \"process_state\": \""
                   << EscapeJson(lifecycle.process_state) << "\",\n"
                   << "  \"pid\": " << lifecycle.process_id << ",\n"
                   << "  \"exit_code\": " << lifecycle.exit_code << ",\n"
                   << "  \"execution_engine_ready\": "
                   << (lifecycle.execution_engine_ready ? "true" : "false")
                   << ",\n"
                   << "  \"native_library_found\": "
                   << (lifecycle.native_library_found ? "true" : "false")
                   << ",\n"
                   << "  \"dlopen_ok\": "
                   << (lifecycle.dlopen_ok ? "true" : "false") << ",\n"
                   << "  \"entrypoint_found\": "
                   << (lifecycle.entrypoint_found ? "true" : "false")
                   << ",\n"
                   << "  \"activity_called\": "
                   << (lifecycle.activity_called ? "true" : "false")
                   << ",\n"
                   << "  \"asset_root\": \""
                   << EscapeJson(lifecycle.bootstrap.plan.asset_root)
                   << "\",\n"
                   << "  \"resource_root\": \""
                   << EscapeJson(lifecycle.bootstrap.plan.resource_root)
                   << "\",\n"
                   << "  \"selected_library_path\": \""
                   << EscapeJson(lifecycle.selected_library_path) << "\",\n"
                   << "  \"exit_reason\": \""
                   << EscapeJson(lifecycle.exit_reason) << "\",\n"
                   << "  \"failure_reason\": \""
                   << EscapeJson(lifecycle.failure_reason) << "\",\n"
                   << "  \"runner_log_path\": \""
                   << EscapeJson(lifecycle.runner_log_path) << "\",\n"
                   << "  \"runner_report_path\": \""
                   << EscapeJson(lifecycle.runner_report_path) << "\"\n"
                   << "}\n";
  WriteTextFile(lifecycle.session_manifest_path, session_manifest.str());

  const bool created = lifecycle.current_activity_state != "NOT_CREATED";
  const bool started = lifecycle.current_activity_state == "STARTED" ||
                       lifecycle.current_activity_state == "RESUMED";
  const bool resumed = lifecycle.current_activity_state == "RESUMED";

  std::ostringstream activity_state;
  activity_state << "package_name="
                 << lifecycle.bootstrap.plan.assessment.package_name << "\n";
  activity_state << "launcher_component="
                 << lifecycle.bootstrap.plan.assessment.launcher_component
                 << "\n";
  activity_state << "created=" << (created ? "true" : "false") << "\n";
  activity_state << "started=" << (started ? "true" : "false") << "\n";
  activity_state << "resumed=" << (resumed ? "true" : "false") << "\n";
  activity_state << "current_state=" << lifecycle.current_activity_state
                 << "\n";
  activity_state << "process_state=" << lifecycle.process_state << "\n";
  activity_state << "pid=" << lifecycle.process_id << "\n";
  activity_state << "exit_code=" << lifecycle.exit_code << "\n";
  WriteTextFile(lifecycle.activity_state_path, activity_state.str());

  std::ostringstream services;
  for (const auto& service : lifecycle.services) {
    services << service.service_name << "|" << service.service_kind << "|"
             << service.status << "|" << service.notes << "\n";
  }
  if (!lifecycle.binder_service_manager_ready) {
    WriteTextFile(lifecycle.service_registry_path, services.str());
  }

  WriteTextFile(lifecycle.report_path,
                RenderNativeLifecycleShimReport(lifecycle));
}

NativeExecuteReport ReadNativeExecuteReportFile(const fs::path& path) {
  const std::string json = ReadFile(path);
  NativeExecuteReport report;
  report.package_name = ExtractJsonString(json, "package_name");
  report.launcher_component = ExtractJsonString(json, "launcher_component");
  report.bundle_apk_path = ExtractJsonString(json, "bundle_apk_path");
  report.sandbox_root = ExtractJsonString(json, "sandbox_root");
  report.dex_cache_root = ExtractJsonString(json, "dex_cache_root");
  report.resource_root = ExtractJsonString(json, "resource_root");
  report.library_root = ExtractJsonString(json, "library_root");
  report.bootstrap_manifest_path =
      ExtractJsonString(json, "bootstrap_manifest_path");
  report.bundle_present = ExtractJsonBool(json, "bundle_present");
  report.bootstrap_manifest_present =
      ExtractJsonBool(json, "bootstrap_manifest_present");
  report.native_library_found =
      ExtractJsonBool(json, "native_library_found");
  report.dlopen_ok = ExtractJsonBool(json, "dlopen_ok");
  report.entrypoint_found = ExtractJsonBool(json, "entrypoint_found");
  report.activity_called = ExtractJsonBool(json, "activity_called");
  report.execution_engine_ready =
      ExtractJsonBool(json, "execution_engine_ready");
  report.exit_code = ExtractJsonInt(json, "exit_code");
  report.selected_library_path =
      ExtractJsonString(json, "selected_library_path");
  report.working_directory = ExtractJsonString(json, "working_directory");
  report.exit_reason = ExtractJsonString(json, "exit_reason");
  return report;
}

void ApplyNativeExecuteReport(NativeLifecycleShim& lifecycle,
                              const NativeExecuteReport& report) {
  lifecycle.native_library_found = report.native_library_found;
  lifecycle.dlopen_ok = report.dlopen_ok;
  lifecycle.entrypoint_found = report.entrypoint_found;
  lifecycle.activity_called = report.activity_called;
  lifecycle.execution_engine_ready = report.execution_engine_ready;
  lifecycle.selected_library_path = report.selected_library_path;
  lifecycle.exit_code = report.exit_code;
  lifecycle.exit_reason = report.exit_reason;
  if (report.activity_called) {
    lifecycle.current_activity_state = "CREATED";
  }
  if (report.exit_code != 0 && lifecycle.failure_reason.empty()) {
    lifecycle.failure_reason = "native runner exited with code " +
                               std::to_string(report.exit_code);
  }
}

int OpenRunnerLog(const std::string& path) {
  return open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

void CloseExtraFileDescriptors() {
  std::error_code error;
  const fs::path proc_fd("/proc/self/fd");
  if (fs::exists(proc_fd, error)) {
    std::vector<int> fds_to_close;
    for (const auto& entry : fs::directory_iterator(proc_fd, error)) {
      if (error) {
        break;
      }
      const std::string name = entry.path().filename().string();
      try {
        const int descriptor = std::stoi(name);
        if (descriptor > STDERR_FILENO) {
          fds_to_close.push_back(descriptor);
        }
      } catch (...) {
      }
    }
    for (const int descriptor : fds_to_close) {
      close(descriptor);
    }
    if (!error) {
      return;
    }
  }

  const long open_max = sysconf(_SC_OPEN_MAX);
  const int fallback_max = open_max > 0 ? static_cast<int>(open_max) : 256;
  for (int descriptor = STDERR_FILENO + 1; descriptor < fallback_max;
       ++descriptor) {
    close(descriptor);
  }
}

std::vector<std::string> BuildChildArguments(
    const std::string& compatctl_path,
    const NativeActivityBootstrap& bootstrap) {
  return {
      compatctl_path,
      "native-execute-stub",
      bootstrap.plan.assessment.package_name,
      bootstrap.plan.assessment.launcher_component,
      bootstrap.plan.bundle_apk_path,
      bootstrap.plan.sandbox_root,
      bootstrap.plan.dex_cache_root,
      bootstrap.plan.resource_root,
      bootstrap.plan.library_root,
      bootstrap.bootstrap_manifest_path,
  };
}

std::vector<std::string> BuildChildEnvironment(
    const NativeLifecycleShim& lifecycle,
    const NativeActivityBootstrap& bootstrap) {
  return {
      "PATH=/usr/bin:/bin",
      "HOME=" + bootstrap.plan.sandbox_root,
      "PWD=" + bootstrap.plan.sandbox_root,
      "LD_LIBRARY_PATH=" + bootstrap.plan.library_root,
      "LINUXOID_PACKAGE_NAME=" + bootstrap.plan.assessment.package_name,
      "LINUXOID_INSTALL_ID=" + bootstrap.plan.assessment.install_id,
      "LINUXOID_LAUNCHER_COMPONENT=" +
          bootstrap.plan.assessment.launcher_component,
      "LINUXOID_BUNDLE_APK=" + bootstrap.plan.bundle_apk_path,
      "LINUXOID_SANDBOX_ROOT=" + bootstrap.plan.sandbox_root,
      "LINUXOID_DEX_CACHE_ROOT=" + bootstrap.plan.dex_cache_root,
      "LINUXOID_RESOURCE_ROOT=" + bootstrap.plan.resource_root,
      "LINUXOID_ASSET_ROOT=" + bootstrap.plan.asset_root,
      "LINUXOID_LIBRARY_ROOT=" + bootstrap.plan.library_root,
      "LINUXOID_SELECTED_ABI=" + bootstrap.plan.selected_abi,
      "LINUXOID_BOOTSTRAP_MANIFEST=" + bootstrap.bootstrap_manifest_path,
      "LINUXOID_RUNNER_REPORT_PATH=" + lifecycle.runner_report_path,
  };
}

std::vector<char*> BuildExecPointers(std::vector<std::string>& values) {
  std::vector<char*> pointers;
  pointers.reserve(values.size() + 1);
  for (auto& value : values) {
    pointers.push_back(value.data());
  }
  pointers.push_back(nullptr);
  return pointers;
}

std::string BuildExitReason(const NativeLifecycleShim& lifecycle) {
  if (!lifecycle.exit_reason.empty()) {
    return lifecycle.exit_reason;
  }
  if (!lifecycle.failure_reason.empty()) {
    return lifecycle.failure_reason;
  }
  if (lifecycle.execution_engine_ready) {
    return "bootstrap_completed";
  }
  return "bootstrap_completed_without_engine_ready";
}

std::string ResolveCompatctlExecutablePath() {
  std::error_code error;
  const fs::path executable_path = fs::read_symlink("/proc/self/exe", error);
  if (error || executable_path.empty()) {
    throw std::runtime_error("unable to resolve compatctl executable path");
  }
  return executable_path.string();
}

}  // namespace

NativeLifecycleShim BuildNativeLifecycleShim(
    const NativeActivityBootstrap& bootstrap) {
  if (!bootstrap.bootstrap_ready) {
    throw std::invalid_argument(
        "native lifecycle shim requires a ready native activity bootstrap");
  }

  NativeLifecycleShim lifecycle;
  lifecycle.bootstrap = bootstrap;
  lifecycle.session_id = bootstrap.plan.assessment.install_id + "-default";
  lifecycle.session_root =
      (fs::path(bootstrap.plan.package_root) / "lifecycle" / lifecycle.session_id)
          .string();
  lifecycle.session_manifest_path =
      (fs::path(lifecycle.session_root) / "session.json").string();
  lifecycle.activity_state_path =
      (fs::path(lifecycle.session_root) / "activity-state.txt").string();
  lifecycle.service_registry_path =
      (fs::path(lifecycle.session_root) / "services.txt").string();
  lifecycle.binder_manager_metadata_path =
      (fs::path(lifecycle.session_root) / "binder" / "service-manager.json")
          .string();
  lifecycle.binder_lookup_log_path =
      (fs::path(lifecycle.session_root) / "binder" / "service-lookups.jsonl")
          .string();
  lifecycle.binder_transaction_log_path =
      (fs::path(lifecycle.session_root) / "binder" / "service-transactions.jsonl")
          .string();
  lifecycle.binder_transport_log_path =
      (fs::path(lifecycle.session_root) / "binder" / "transport-messages.jsonl")
          .string();
  lifecycle.report_path =
      (fs::path(lifecycle.session_root) / "lifecycle-report.txt").string();
  lifecycle.runner_log_path =
      (fs::path(lifecycle.session_root) / "runner.log").string();
  lifecycle.runner_report_path =
      (fs::path(lifecycle.session_root) / "runner-report.json").string();
  lifecycle.current_activity_state = "NOT_CREATED";
  lifecycle.process_state = "BOOTSTRAPPED";
  lifecycle.exit_reason = "bootstrap_not_started";
  lifecycle.execution_engine_ready = false;

  fs::create_directories(lifecycle.session_root);
  lifecycle.binder_service_manager = RunBinderServiceManagerFixture(
      {.package_name = bootstrap.plan.assessment.package_name,
       .launcher_component = bootstrap.plan.assessment.launcher_component,
       .apk_path = bootstrap.plan.apk_path,
       .artifact_root = lifecycle.session_root});
  lifecycle.binder_service_manager_ready =
      lifecycle.binder_service_manager.manager_ready;
  lifecycle.binder_manager_metadata_path =
      lifecycle.binder_service_manager.metadata_path;
  lifecycle.service_registry_path = lifecycle.binder_service_manager.registry_path;
  lifecycle.binder_lookup_log_path =
      lifecycle.binder_service_manager.lookup_log_path;
  lifecycle.binder_transaction_log_path =
      lifecycle.binder_service_manager.transaction_log_path;
  lifecycle.binder_transport_log_path =
      lifecycle.binder_service_manager.transport_log_path;
  lifecycle.services = lifecycle.binder_service_manager_ready
                           ? BuildServiceBindingsFromBinderFixture(
                                 lifecycle.binder_service_manager)
                           : BuildDefaultServiceBindings();
  WriteLifecycleArtifacts(lifecycle);

  lifecycle.lifecycle_handoff_ready =
      FileExists(lifecycle.session_manifest_path) &&
      FileExists(lifecycle.activity_state_path) &&
      FileExists(lifecycle.service_registry_path) &&
      FileExists(lifecycle.binder_manager_metadata_path) &&
      FileExists(lifecycle.binder_lookup_log_path) &&
      FileExists(lifecycle.binder_transaction_log_path) &&
      FileExists(lifecycle.binder_transport_log_path) &&
      FileExists(lifecycle.report_path) && !lifecycle.services.empty();
  return lifecycle;
}

NativeLifecycleShim BuildNativeLifecycleShimFromManifest(
    const std::string& bootstrap_manifest_path) {
  return BuildNativeLifecycleShim(
      ReadNativeActivityBootstrapManifest(bootstrap_manifest_path));
}

NativeLifecycleShim RunNativeProcessBootstrap(
    const NativeActivityBootstrap& bootstrap) {
  NativeLifecycleShim lifecycle = BuildNativeLifecycleShim(bootstrap);
  const std::string compatctl_path = ResolveCompatctlExecutablePath();

  const pid_t child = fork();
  if (child < 0) {
    lifecycle.process_state = "FORK_FAILED";
    lifecycle.exit_code = 2;
    lifecycle.exit_reason = "fork_failed";
    lifecycle.failure_reason =
        "fork failed: " + std::string(std::strerror(errno));
    WriteLifecycleArtifacts(lifecycle);
    return lifecycle;
  }

  if (child == 0) {
    const int log_fd = OpenRunnerLog(lifecycle.runner_log_path);
    if (log_fd < 0) {
      _exit(2);
    }

    const int null_fd = open("/dev/null", O_RDONLY);
    if (null_fd >= 0) {
      dup2(null_fd, STDIN_FILENO);
      if (null_fd > STDERR_FILENO) {
        close(null_fd);
      }
    }
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
    if (log_fd > STDERR_FILENO) {
      close(log_fd);
    }

    if (chdir(bootstrap.plan.sandbox_root.c_str()) != 0) {
      std::cerr << "linuxoid child chdir failed: "
                << std::strerror(errno) << "\n";
      _exit(2);
    }

    std::vector<std::string> arguments =
        BuildChildArguments(compatctl_path, bootstrap);
    std::vector<char*> argv = BuildExecPointers(arguments);

    std::vector<std::string> environment =
        BuildChildEnvironment(lifecycle, bootstrap);
    std::vector<char*> envp = BuildExecPointers(environment);

    CloseExtraFileDescriptors();
    execve(compatctl_path.c_str(), argv.data(), envp.data());
    std::cerr << "linuxoid child execve failed: " << std::strerror(errno)
              << "\n";
    _exit(2);
  }

  lifecycle.process_id = static_cast<int>(child);
  lifecycle.process_state = "RUNNING";
  lifecycle.exit_reason = "child_runner_active";
  WriteLifecycleArtifacts(lifecycle);

  int wait_status = 0;
  if (waitpid(child, &wait_status, 0) < 0) {
    lifecycle.process_state = "WAIT_FAILED";
    lifecycle.exit_code = 2;
    lifecycle.exit_reason = "waitpid_failed";
    lifecycle.failure_reason =
        "waitpid failed: " + std::string(std::strerror(errno));
    WriteLifecycleArtifacts(lifecycle);
    return lifecycle;
  }

  if (WIFEXITED(wait_status)) {
    lifecycle.process_state = "EXITED";
    lifecycle.exit_code = WEXITSTATUS(wait_status);
    lifecycle.exit_reason = lifecycle.exit_code == 0
                                ? "child_runner_completed"
                                : "child_runner_nonzero_exit";
  } else if (WIFSIGNALED(wait_status)) {
    lifecycle.process_state = "SIGNALED";
    lifecycle.exit_code = 128 + WTERMSIG(wait_status);
    lifecycle.exit_reason = "child_runner_signaled";
    lifecycle.failure_reason =
        "native runner terminated by signal " +
        std::to_string(WTERMSIG(wait_status));
  } else {
    lifecycle.process_state = "UNKNOWN_EXIT";
    lifecycle.exit_code = 2;
    lifecycle.exit_reason = "child_runner_unknown_exit";
    lifecycle.failure_reason = "native runner ended in an unknown state";
  }

  if (FileExists(lifecycle.runner_report_path)) {
    lifecycle.runner_report_json = ReadFile(lifecycle.runner_report_path);
    ApplyNativeExecuteReport(lifecycle,
                             ReadNativeExecuteReportFile(
                                 lifecycle.runner_report_path));
  } else if (lifecycle.failure_reason.empty()) {
    lifecycle.exit_code = 2;
    lifecycle.exit_reason = "runner_report_missing";
    lifecycle.failure_reason =
        "native runner exited before writing structured report";
  }

  if (lifecycle.exit_code != 0 && lifecycle.failure_reason.empty()) {
    lifecycle.failure_reason = "native runner exited before completing bootstrap";
  }
  lifecycle.exit_reason = BuildExitReason(lifecycle);
  WriteLifecycleArtifacts(lifecycle);
  lifecycle.lifecycle_handoff_ready =
      FileExists(lifecycle.session_manifest_path) &&
      FileExists(lifecycle.activity_state_path) &&
      FileExists(lifecycle.service_registry_path) &&
      FileExists(lifecycle.binder_manager_metadata_path) &&
      FileExists(lifecycle.binder_lookup_log_path) &&
      FileExists(lifecycle.binder_transaction_log_path) &&
      FileExists(lifecycle.binder_transport_log_path) &&
      FileExists(lifecycle.report_path) &&
      FileExists(lifecycle.runner_log_path) && !lifecycle.services.empty();
  return lifecycle;
}

NativeLifecycleShim RunNativeProcessBootstrapFromManifest(
    const std::string& bootstrap_manifest_path) {
  return RunNativeProcessBootstrap(
      ReadNativeActivityBootstrapManifest(bootstrap_manifest_path));
}

std::string RenderNativeLifecycleShimReport(
    const NativeLifecycleShim& lifecycle) {
  std::ostringstream output;
  output << "Package: " << lifecycle.bootstrap.plan.assessment.package_name
         << "\n";
  output << "Install ID: " << lifecycle.bootstrap.plan.assessment.install_id
         << "\n";
  output << "Launcher Component: "
         << lifecycle.bootstrap.plan.assessment.launcher_component << "\n";
  output << "APK Path: " << lifecycle.bootstrap.plan.apk_path << "\n";
  output << "Bootstrap Manifest: " << lifecycle.bootstrap.bootstrap_manifest_path
         << "\n";
  output << "Session Root: " << lifecycle.session_root << "\n";
  output << "Session Manifest: " << lifecycle.session_manifest_path << "\n";
  output << "Activity State File: " << lifecycle.activity_state_path << "\n";
  output << "Service Registry: " << lifecycle.service_registry_path << "\n";
  output << "Binder Manager Metadata: " << lifecycle.binder_manager_metadata_path
         << "\n";
  output << "Binder Lookup Log: " << lifecycle.binder_lookup_log_path << "\n";
  output << "Binder Transaction Log: " << lifecycle.binder_transaction_log_path
         << "\n";
  output << "Binder Transport Log: " << lifecycle.binder_transport_log_path
         << "\n";
  output << "Runner Log: " << lifecycle.runner_log_path << "\n";
  output << "Runner Report: " << lifecycle.runner_report_path << "\n";
  output << "Lifecycle Handoff Ready: "
         << (lifecycle.lifecycle_handoff_ready ? "yes" : "no") << "\n";
  output << "Binder Service Manager Ready: "
         << (lifecycle.binder_service_manager_ready ? "yes" : "no") << "\n";
  output << "Execution Engine Ready: "
         << (lifecycle.execution_engine_ready ? "yes" : "no") << "\n";
  output << "Current Activity State: " << lifecycle.current_activity_state
         << "\n";
  output << "Process State: " << lifecycle.process_state << "\n";
  output << "PID: " << lifecycle.process_id << "\n";
  output << "Exit Code: " << lifecycle.exit_code << "\n";
  output << "Exit Reason: " << BuildExitReason(lifecycle) << "\n";
  output << "Selected ABI: "
         << (lifecycle.bootstrap.plan.selected_abi.empty()
                 ? "unsupported"
                 : lifecycle.bootstrap.plan.selected_abi)
         << "\n";
  output << "Asset Root: " << lifecycle.bootstrap.plan.asset_root << "\n";
  output << "Resource Root: " << lifecycle.bootstrap.plan.resource_root
         << "\n";
  output << "Staged Native Libraries: "
         << lifecycle.bootstrap.plan.staged_native_libraries.size() << "\n";
  output << "Unsupported Native Libraries: "
         << lifecycle.bootstrap.plan.unsupported_native_libraries.size()
         << "\n";
  output << "Native Library Found: "
         << (lifecycle.native_library_found ? "yes" : "no") << "\n";
  output << "dlopen OK: " << (lifecycle.dlopen_ok ? "yes" : "no") << "\n";
  output << "Entrypoint Found: "
         << (lifecycle.entrypoint_found ? "yes" : "no") << "\n";
  output << "Activity Called: "
         << (lifecycle.activity_called ? "yes" : "no") << "\n";
  output << "Selected Library: "
         << (lifecycle.selected_library_path.empty()
                 ? "not selected"
                 : lifecycle.selected_library_path)
         << "\n";
  if (!lifecycle.failure_reason.empty()) {
    output << "Failure Reason: " << lifecycle.failure_reason << "\n";
  }
  output << "Services:\n";
  for (const auto& service : lifecycle.services) {
    output << "  - " << service.service_name << " [" << service.service_kind
           << "] " << service.status << ": " << service.notes << "\n";
  }
  output << "Next Steps:\n";
  output << "  - Keep process state truthful and machine-readable for MCP clients and validation harnesses.\n";
  output << "  - Attach DEX/class loading, resource lookup, and real Binder-compatible transport behind the local service-manager contract.\n";
  output << "  - Grow from bootstrap truth into real Android process execution.\n";
  return output.str();
}

std::string RenderNativeProcessBootstrapJson(
    const NativeLifecycleShim& lifecycle) {
  std::string libraries_loaded_json = "[]";
  std::string jni_onload_results_json = "[]";
  if (!lifecycle.runner_report_json.empty()) {
    try {
      libraries_loaded_json =
          ExtractJsonArrayLiteral(lifecycle.runner_report_json,
                                  "libraries_loaded");
      jni_onload_results_json =
          ExtractJsonArrayLiteral(lifecycle.runner_report_json,
                                  "jni_onload_results");
    } catch (...) {
      libraries_loaded_json = "[]";
      jni_onload_results_json = "[]";
    }
  }

  std::ostringstream output;
  output << "{\n"
         << "  \"execution_engine_ready\": "
         << (lifecycle.execution_engine_ready ? "true" : "false") << ",\n"
         << "  \"libraries_loaded\": " << libraries_loaded_json << ",\n"
         << "  \"jni_onload_results\": " << jni_onload_results_json
         << ",\n"
         << "  \"exit_reason\": \""
         << EscapeJson(BuildExitReason(lifecycle)) << "\",\n"
         << "  \"artifact_paths\": {\n"
         << "    \"bootstrap_manifest_path\": \""
         << EscapeJson(lifecycle.bootstrap.bootstrap_manifest_path) << "\",\n"
         << "    \"session_root\": \"" << EscapeJson(lifecycle.session_root)
         << "\",\n"
         << "    \"session_manifest_path\": \""
         << EscapeJson(lifecycle.session_manifest_path) << "\",\n"
         << "    \"activity_state_path\": \""
         << EscapeJson(lifecycle.activity_state_path) << "\",\n"
         << "    \"service_registry_path\": \""
         << EscapeJson(lifecycle.service_registry_path) << "\",\n"
         << "    \"binder_manager_metadata_path\": \""
         << EscapeJson(lifecycle.binder_manager_metadata_path) << "\",\n"
         << "    \"binder_lookup_log_path\": \""
         << EscapeJson(lifecycle.binder_lookup_log_path) << "\",\n"
         << "    \"binder_transaction_log_path\": \""
         << EscapeJson(lifecycle.binder_transaction_log_path) << "\",\n"
         << "    \"binder_transport_log_path\": \""
         << EscapeJson(lifecycle.binder_transport_log_path) << "\",\n"
         << "    \"runner_log_path\": \""
         << EscapeJson(lifecycle.runner_log_path) << "\",\n"
         << "    \"runner_report_path\": \""
         << EscapeJson(lifecycle.runner_report_path) << "\"\n"
         << "  }\n"
         << "}\n";
  return output.str();
}

}  // namespace wfa
