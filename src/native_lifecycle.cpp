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

bool ExtractJsonBool(const std::string& json, const std::string& key) {
  const std::regex pattern("\"" + key + R"(\"\s*:\s*(true|false))");
  std::smatch match;
  if (!std::regex_search(json, match, pattern) || match.size() != 2) {
    throw std::runtime_error("unable to extract boolean field from json: " +
                             key);
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
  bootstrap.plan.assessment.launcher_component =
      ExtractJsonString(json, "launcher_component");
  bootstrap.plan.bundle_apk_path = ExtractJsonString(json, "bundle_apk_path");
  bootstrap.plan.sandbox_root = ExtractJsonString(json, "sandbox_root");
  bootstrap.plan.dex_cache_root = ExtractJsonString(json, "dex_cache_root");
  bootstrap.plan.resource_root = ExtractJsonString(json, "resource_root");
  bootstrap.plan.library_root = ExtractJsonString(json, "library_root");
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
                   << "  \"selected_library_path\": \""
                   << EscapeJson(lifecycle.selected_library_path) << "\",\n"
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
  WriteTextFile(lifecycle.service_registry_path, services.str());

  WriteTextFile(lifecycle.report_path,
                RenderNativeLifecycleShimReport(lifecycle));
}

NativeExecuteRequest BuildNativeExecuteRequest(
    const NativeActivityBootstrap& bootstrap) {
  return {.package_name = bootstrap.plan.assessment.package_name,
          .launcher_component = bootstrap.plan.assessment.launcher_component,
          .bundle_apk_path = bootstrap.plan.bundle_apk_path,
          .sandbox_root = bootstrap.plan.sandbox_root,
          .dex_cache_root = bootstrap.plan.dex_cache_root,
          .resource_root = bootstrap.plan.resource_root,
          .library_root = bootstrap.plan.library_root,
          .bootstrap_manifest_path = bootstrap.bootstrap_manifest_path};
}

void WriteNativeExecuteReportFile(const fs::path& path,
                                  const NativeExecuteReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"launcher_component\": \""
         << EscapeJson(report.launcher_component) << "\",\n"
         << "  \"bundle_apk_path\": \"" << EscapeJson(report.bundle_apk_path)
         << "\",\n"
         << "  \"sandbox_root\": \"" << EscapeJson(report.sandbox_root)
         << "\",\n"
         << "  \"dex_cache_root\": \"" << EscapeJson(report.dex_cache_root)
         << "\",\n"
         << "  \"resource_root\": \"" << EscapeJson(report.resource_root)
         << "\",\n"
         << "  \"library_root\": \"" << EscapeJson(report.library_root)
         << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"bundle_present\": "
         << (report.bundle_present ? "true" : "false") << ",\n"
         << "  \"bootstrap_manifest_present\": "
         << (report.bootstrap_manifest_present ? "true" : "false")
         << ",\n"
         << "  \"native_library_found\": "
         << (report.native_library_found ? "true" : "false") << ",\n"
         << "  \"dlopen_ok\": " << (report.dlopen_ok ? "true" : "false")
         << ",\n"
         << "  \"entrypoint_found\": "
         << (report.entrypoint_found ? "true" : "false") << ",\n"
         << "  \"activity_called\": "
         << (report.activity_called ? "true" : "false") << ",\n"
         << "  \"execution_engine_ready\": "
         << (report.execution_engine_ready ? "true" : "false") << ",\n"
         << "  \"exit_code\": " << report.exit_code << ",\n"
         << "  \"selected_library_path\": \""
         << EscapeJson(report.selected_library_path) << "\"\n"
         << "}\n";
  WriteTextFile(path, output.str());
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
  lifecycle.report_path =
      (fs::path(lifecycle.session_root) / "lifecycle-report.txt").string();
  lifecycle.runner_log_path =
      (fs::path(lifecycle.session_root) / "runner.log").string();
  lifecycle.runner_report_path =
      (fs::path(lifecycle.session_root) / "runner-report.json").string();
  lifecycle.current_activity_state = "NOT_CREATED";
  lifecycle.process_state = "BOOTSTRAPPED";
  lifecycle.services = BuildDefaultServiceBindings();
  lifecycle.execution_engine_ready = false;

  fs::create_directories(lifecycle.session_root);
  WriteLifecycleArtifacts(lifecycle);

  lifecycle.lifecycle_handoff_ready =
      FileExists(lifecycle.session_manifest_path) &&
      FileExists(lifecycle.activity_state_path) &&
      FileExists(lifecycle.service_registry_path) &&
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

  const pid_t child = fork();
  if (child < 0) {
    lifecycle.process_state = "FORK_FAILED";
    lifecycle.exit_code = EXIT_FAILURE;
    lifecycle.failure_reason =
        "fork failed: " + std::string(std::strerror(errno));
    WriteLifecycleArtifacts(lifecycle);
    return lifecycle;
  }

  if (child == 0) {
    const int log_fd = OpenRunnerLog(lifecycle.runner_log_path);
    if (log_fd < 0) {
      _exit(127);
    }

    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
    if (log_fd > STDERR_FILENO) {
      close(log_fd);
    }

    NativeExecuteReport report =
        ExecuteNativeStub(BuildNativeExecuteRequest(bootstrap));
    try {
      WriteNativeExecuteReportFile(lifecycle.runner_report_path, report);
    } catch (...) {
    }
    std::cout << report.output;
    std::cout.flush();
    _exit(report.exit_code);
  }

  lifecycle.process_id = static_cast<int>(child);
  lifecycle.process_state = "RUNNING";
  WriteLifecycleArtifacts(lifecycle);

  int wait_status = 0;
  if (waitpid(child, &wait_status, 0) < 0) {
    lifecycle.process_state = "WAIT_FAILED";
    lifecycle.exit_code = EXIT_FAILURE;
    lifecycle.failure_reason =
        "waitpid failed: " + std::string(std::strerror(errno));
    WriteLifecycleArtifacts(lifecycle);
    return lifecycle;
  }

  if (WIFEXITED(wait_status)) {
    lifecycle.process_state = "EXITED";
    lifecycle.exit_code = WEXITSTATUS(wait_status);
  } else if (WIFSIGNALED(wait_status)) {
    lifecycle.process_state = "SIGNALED";
    lifecycle.exit_code = 128 + WTERMSIG(wait_status);
    lifecycle.failure_reason =
        "native runner terminated by signal " +
        std::to_string(WTERMSIG(wait_status));
  } else {
    lifecycle.process_state = "UNKNOWN_EXIT";
    lifecycle.exit_code = EXIT_FAILURE;
    lifecycle.failure_reason = "native runner ended in an unknown state";
  }

  if (FileExists(lifecycle.runner_report_path)) {
    ApplyNativeExecuteReport(lifecycle,
                             ReadNativeExecuteReportFile(
                                 lifecycle.runner_report_path));
  } else if (lifecycle.failure_reason.empty()) {
    lifecycle.failure_reason =
        "native runner exited before writing structured report";
  }

  WriteLifecycleArtifacts(lifecycle);
  lifecycle.lifecycle_handoff_ready =
      FileExists(lifecycle.session_manifest_path) &&
      FileExists(lifecycle.activity_state_path) &&
      FileExists(lifecycle.service_registry_path) &&
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
  output << "Bootstrap Manifest: " << lifecycle.bootstrap.bootstrap_manifest_path
         << "\n";
  output << "Session Root: " << lifecycle.session_root << "\n";
  output << "Session Manifest: " << lifecycle.session_manifest_path << "\n";
  output << "Activity State File: " << lifecycle.activity_state_path << "\n";
  output << "Service Registry: " << lifecycle.service_registry_path << "\n";
  output << "Runner Log: " << lifecycle.runner_log_path << "\n";
  output << "Runner Report: " << lifecycle.runner_report_path << "\n";
  output << "Lifecycle Handoff Ready: "
         << (lifecycle.lifecycle_handoff_ready ? "yes" : "no") << "\n";
  output << "Execution Engine Ready: "
         << (lifecycle.execution_engine_ready ? "yes" : "no") << "\n";
  output << "Current Activity State: " << lifecycle.current_activity_state
         << "\n";
  output << "Process State: " << lifecycle.process_state << "\n";
  output << "PID: " << lifecycle.process_id << "\n";
  output << "Exit Code: " << lifecycle.exit_code << "\n";
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
  output << "  - Attach DEX/class loading, resource lookup, and Binder-compatible services.\n";
  output << "  - Grow from bootstrap truth into real Android process execution.\n";
  return output.str();
}

}  // namespace wfa
