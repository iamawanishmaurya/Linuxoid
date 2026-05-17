#include "wfa/art_activity_bootstrap_fixture.hpp"

#include "wfa/art_runtime_smoke.hpp"
#include "wfa/native_lifecycle.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

struct CommandCaptureResult {
  int exit_code = -1;
  std::string output;
};

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
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

std::string ResolveActivityClassName(const std::string& launcher_component) {
  const auto slash = launcher_component.find('/');
  if (slash == std::string::npos || slash == 0 ||
      slash + 1 >= launcher_component.size()) {
    return "";
  }

  const std::string package_name = launcher_component.substr(0, slash);
  std::string class_name = launcher_component.substr(slash + 1);
  if (!class_name.empty() && class_name.front() == '.') {
    class_name = package_name + class_name;
  }
  return class_name;
}

std::string ClassNameToDescriptor(const std::string& class_name) {
  if (class_name.empty()) {
    return "";
  }

  std::string descriptor = "L";
  descriptor.reserve(class_name.size() + 3);
  for (const char character : class_name) {
    descriptor.push_back(character == '.' ? '/' : character);
  }
  descriptor.push_back(';');
  return descriptor;
}

CommandCaptureResult RunCommandCapture(const std::string& command) {
  const std::string wrapped_command = command + " 2>&1";
  FILE* pipe = popen(wrapped_command.c_str(), "r");
  if (pipe == nullptr) {
    throw std::runtime_error("unable to open activity bootstrap probe command");
  }

  std::string output;
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }

  const int raw_status = pclose(pipe);
  CommandCaptureResult result;
  if (WIFEXITED(raw_status)) {
    result.exit_code = WEXITSTATUS(raw_status);
  } else {
    result.exit_code = raw_status;
  }
  result.output = output;
  return result;
}

std::string BuildActivityBootstrapPlanJson(
    const NativeArtActivityBootstrapFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id)
         << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"launcher_component\": \""
         << EscapeJson(report.launcher_component) << "\",\n"
         << "  \"selected_activity_class_name\": \""
         << EscapeJson(report.selected_activity_class_name) << "\",\n"
         << "  \"selected_activity_class_descriptor\": \""
         << EscapeJson(report.selected_activity_class_descriptor) << "\",\n"
         << "  \"runtime_smoke_result_json_path\": \""
         << EscapeJson(report.runtime_smoke_result_json_path) << "\",\n"
         << "  \"class_resolution_result_json_path\": \""
         << EscapeJson(report.class_resolution_result_json_path) << "\",\n"
         << "  \"manifest_targets_ready\": "
         << (report.manifest_targets_ready ? "true" : "false") << ",\n"
         << "  \"classpath_plan_ready\": "
         << (report.classpath_plan_ready ? "true" : "false") << ",\n"
         << "  \"offline_resolution_ready\": "
         << (report.offline_resolution_ready ? "true" : "false") << ",\n"
         << "  \"runtime_class_resolution_succeeded\": "
         << (report.runtime_class_resolution_succeeded ? "true" : "false")
         << ",\n"
         << "  \"runtime_bootstrap_planned\": "
         << (report.runtime_bootstrap_planned ? "true" : "false") << ",\n"
         << "  \"runtime_bootstrap_attempted\": "
         << (report.runtime_bootstrap_attempted ? "true" : "false") << ",\n"
         << "  \"activity_bootstrap_command\": \""
         << EscapeJson(report.activity_bootstrap_command) << "\",\n"
         << "  \"dependency_count\": " << report.dependency_count << ",\n"
         << "  \"bootstrap_dependencies\": "
         << RenderJsonArray(report.bootstrap_dependencies) << ",\n"
         << "  \"missing_dependencies\": "
         << RenderJsonArray(report.missing_dependencies) << ",\n"
         << "  \"planned_bootstrap_steps\": "
         << RenderJsonArray(report.planned_bootstrap_steps) << "\n"
         << "}\n";
  return output.str();
}

std::string BuildActivityBootstrapTraceJsonl(
    const NativeArtActivityBootstrapFixtureReport& report,
    const CommandCaptureResult& capture) {
  std::ostringstream output;
  output << "{\"event_type\": \"activity_bootstrap_started\", "
         << "\"launcher_component\": \""
         << EscapeJson(report.launcher_component) << "\", "
         << "\"selected_activity_class_name\": \""
         << EscapeJson(report.selected_activity_class_name) << "\", "
         << "\"runtime_bootstrap_planned\": "
         << (report.runtime_bootstrap_planned ? "true" : "false") << ", "
         << "\"dependency_count\": " << report.dependency_count << "}\n";

  if (!report.runtime_bootstrap_attempted) {
    output << "{\"event_type\": \"activity_bootstrap_skipped\", "
           << "\"exit_reason\": \"" << EscapeJson(report.exit_reason)
           << "\", "
           << "\"missing_dependencies\": "
           << RenderJsonArray(report.missing_dependencies) << "}\n";
  } else {
    output << "{\"event_type\": \"activity_bootstrap_attempted\", "
           << "\"activity_bootstrap_command\": \""
           << EscapeJson(report.activity_bootstrap_command) << "\"}\n";
    output << "{\"event_type\": \"activity_bootstrap_result\", "
           << "\"runtime_bootstrap_succeeded\": "
           << (report.runtime_bootstrap_succeeded ? "true" : "false") << ", "
           << "\"captured_output\": \"" << EscapeJson(capture.output) << "\"}\n";
  }

  output << "{\"event_type\": \"activity_bootstrap_complete\", "
         << "\"dependency_blocked\": "
         << (report.dependency_blocked ? "true" : "false") << ", "
         << "\"exit_reason\": \"" << EscapeJson(report.exit_reason) << "\"}\n";
  return output.str();
}

std::string BuildActivityBootstrapResultJson(
    const NativeArtActivityBootstrapFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id)
         << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"session_root\": \"" << EscapeJson(report.session_root)
         << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"launcher_component\": \""
         << EscapeJson(report.launcher_component) << "\",\n"
         << "  \"selected_activity_class_name\": \""
         << EscapeJson(report.selected_activity_class_name) << "\",\n"
         << "  \"selected_activity_class_descriptor\": \""
         << EscapeJson(report.selected_activity_class_descriptor) << "\",\n"
         << "  \"class_resolution_result_json_path\": \""
         << EscapeJson(report.class_resolution_result_json_path) << "\",\n"
         << "  \"runtime_smoke_result_json_path\": \""
         << EscapeJson(report.runtime_smoke_result_json_path) << "\",\n"
         << "  \"activity_bootstrap_plan_path\": \""
         << EscapeJson(report.activity_bootstrap_plan_path) << "\",\n"
         << "  \"trace_jsonl_path\": \"" << EscapeJson(report.trace_jsonl_path)
         << "\",\n"
         << "  \"result_json_path\": \"" << EscapeJson(report.result_json_path)
         << "\",\n"
         << "  \"activity_bootstrap_command\": \""
         << EscapeJson(report.activity_bootstrap_command) << "\",\n"
         << "  \"manifest_targets_ready\": "
         << (report.manifest_targets_ready ? "true" : "false") << ",\n"
         << "  \"classpath_plan_ready\": "
         << (report.classpath_plan_ready ? "true" : "false") << ",\n"
         << "  \"offline_resolution_ready\": "
         << (report.offline_resolution_ready ? "true" : "false") << ",\n"
         << "  \"art_runtime_detected\": "
         << (report.art_runtime_detected ? "true" : "false") << ",\n"
         << "  \"safe_runtime_probe_available\": "
         << (report.safe_runtime_probe_available ? "true" : "false")
         << ",\n"
         << "  \"runtime_class_resolution_succeeded\": "
         << (report.runtime_class_resolution_succeeded ? "true" : "false")
         << ",\n"
         << "  \"runtime_bootstrap_planned\": "
         << (report.runtime_bootstrap_planned ? "true" : "false") << ",\n"
         << "  \"runtime_bootstrap_attempted\": "
         << (report.runtime_bootstrap_attempted ? "true" : "false") << ",\n"
         << "  \"runtime_bootstrap_succeeded\": "
         << (report.runtime_bootstrap_succeeded ? "true" : "false") << ",\n"
         << "  \"dependency_blocked\": "
         << (report.dependency_blocked ? "true" : "false") << ",\n"
         << "  \"dependency_count\": " << report.dependency_count << ",\n"
         << "  \"bootstrap_dependencies\": "
         << RenderJsonArray(report.bootstrap_dependencies) << ",\n"
         << "  \"missing_dependencies\": "
         << RenderJsonArray(report.missing_dependencies) << ",\n"
         << "  \"planned_bootstrap_steps\": "
         << RenderJsonArray(report.planned_bootstrap_steps) << ",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\"\n"
         << "}\n";
  return output.str();
}

}  // namespace

NativeArtActivityBootstrapFixtureReport RunNativeArtActivityBootstrapFixture(
    const std::string& bootstrap_manifest_path) {
  const NativeLifecycleShim lifecycle =
      BuildNativeLifecycleShimFromManifest(bootstrap_manifest_path);
  const NativeArtRuntimeSmokeReport runtime_smoke =
      RunNativeArtRuntimeSmokeFixture(bootstrap_manifest_path);

  NativeArtActivityBootstrapFixtureReport report;
  report.package_name = runtime_smoke.package_name;
  report.install_id = runtime_smoke.install_id;
  report.bootstrap_manifest_path = bootstrap_manifest_path;
  report.session_root = lifecycle.session_root;
  report.artifact_root = runtime_smoke.artifact_root;
  report.launcher_component = lifecycle.bootstrap.plan.assessment.launcher_component;
  report.selected_activity_class_name =
      ResolveActivityClassName(report.launcher_component);
  report.selected_activity_class_descriptor =
      ClassNameToDescriptor(report.selected_activity_class_name);
  report.class_resolution_result_json_path =
      runtime_smoke.class_resolution_result_json_path;
  report.runtime_smoke_result_json_path = runtime_smoke.result_json_path;
  report.activity_bootstrap_plan_path =
      (fs::path(report.artifact_root) / "activity-bootstrap-plan.json").string();
  report.trace_jsonl_path =
      (fs::path(report.artifact_root) / "activity-bootstrap-trace.jsonl")
          .string();
  report.result_json_path =
      (fs::path(report.artifact_root) / "activity-bootstrap-result.json")
          .string();
  report.manifest_targets_ready = runtime_smoke.manifest_targets_ready;
  report.classpath_plan_ready = runtime_smoke.classpath_plan_ready;
  report.offline_resolution_ready = runtime_smoke.offline_resolution_ready;
  report.art_runtime_detected = runtime_smoke.art_runtime_detected;
  report.safe_runtime_probe_available =
      runtime_smoke.safe_runtime_probe_available;
  report.runtime_class_resolution_succeeded =
      runtime_smoke.runtime_class_resolution_succeeded;
  report.bootstrap_dependencies = {
      lifecycle.bootstrap.bootstrap_manifest_path,
      runtime_smoke.classloader_plan_path,
      runtime_smoke.class_resolution_result_json_path,
      runtime_smoke.result_json_path,
      lifecycle.binder_manager_metadata_path,
      lifecycle.binder_transport_log_path};
  report.planned_bootstrap_steps = {
      "load_application_class",
      "resolve_launcher_activity_target",
      "bind_linuxoid_service_manager",
      "prepare_activity_bootstrap_probe"};

  if (!report.manifest_targets_ready) {
    report.missing_dependencies.push_back("manifest_targets");
  }
  if (!report.classpath_plan_ready) {
    report.missing_dependencies.push_back("classpath_plan");
  }
  if (!report.offline_resolution_ready) {
    report.missing_dependencies.push_back("offline_class_resolution");
  }
  if (report.selected_activity_class_name.empty()) {
    report.missing_dependencies.push_back("activity_target");
  }
  if (!lifecycle.binder_service_manager_ready) {
    report.missing_dependencies.push_back("binder_service_manager");
  }
  if (!report.art_runtime_detected) {
    report.missing_dependencies.push_back("art_runtime_probe");
  } else if (!report.safe_runtime_probe_available) {
    report.missing_dependencies.push_back("safe_runtime_probe");
  } else if (!report.runtime_class_resolution_succeeded) {
    report.missing_dependencies.push_back("runtime_class_resolution");
  }

  report.dependency_count = report.missing_dependencies.size();
  report.dependency_blocked = report.dependency_count != 0;
  report.runtime_bootstrap_planned =
      report.manifest_targets_ready && report.classpath_plan_ready &&
      report.offline_resolution_ready &&
      !report.selected_activity_class_name.empty() &&
      lifecycle.binder_service_manager_ready;

  if (report.safe_runtime_probe_available &&
      !report.selected_activity_class_name.empty()) {
    report.activity_bootstrap_command =
        QuoteForShell(runtime_smoke.art_runtime_probe) +
        " -Dlinuxoid.bootstrap.activity=" +
        QuoteForShell(report.launcher_component) + " -cp " +
        QuoteForShell(lifecycle.bootstrap.plan.bundle_apk_path) + " " +
        QuoteForShell(report.selected_activity_class_name);
  }

  CommandCaptureResult capture;
  if (!report.manifest_targets_ready) {
    report.exit_reason = "activity_bootstrap_manifest_targets_not_ready";
  } else if (!report.classpath_plan_ready) {
    report.exit_reason = "activity_bootstrap_classpath_plan_not_ready";
  } else if (!report.offline_resolution_ready) {
    report.exit_reason = "activity_bootstrap_offline_resolution_incomplete";
  } else if (report.selected_activity_class_name.empty()) {
    report.exit_reason = "activity_bootstrap_target_missing";
  } else if (!lifecycle.binder_service_manager_ready) {
    report.exit_reason = "activity_bootstrap_binder_unavailable";
  } else if (!report.art_runtime_detected) {
    report.exit_reason = "activity_bootstrap_runtime_not_detected";
  } else if (!report.safe_runtime_probe_available) {
    report.exit_reason = "activity_bootstrap_runtime_probe_unsafe";
  } else if (!report.runtime_class_resolution_succeeded) {
    report.exit_reason = "activity_bootstrap_class_resolution_probe_incomplete";
  } else {
    report.runtime_bootstrap_attempted = true;
    capture = RunCommandCapture(report.activity_bootstrap_command);
    report.runtime_bootstrap_succeeded = capture.exit_code == 0;
    report.dependency_blocked = !report.runtime_bootstrap_succeeded;
    report.exit_reason = report.runtime_bootstrap_succeeded
                             ? "activity_bootstrap_probe_succeeded"
                             : "activity_bootstrap_probe_failed";
  }

  fs::create_directories(report.artifact_root);
  WriteTextFile(report.activity_bootstrap_plan_path,
                BuildActivityBootstrapPlanJson(report));
  WriteTextFile(report.trace_jsonl_path,
                BuildActivityBootstrapTraceJsonl(report, capture));
  WriteTextFile(report.result_json_path,
                BuildActivityBootstrapResultJson(report));
  return report;
}

std::string RenderNativeArtActivityBootstrapFixtureJson(
    const NativeArtActivityBootstrapFixtureReport& report) {
  return BuildActivityBootstrapResultJson(report);
}

}  // namespace wfa
