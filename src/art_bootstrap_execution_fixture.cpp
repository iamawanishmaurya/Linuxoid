#include "wfa/art_bootstrap_execution_fixture.hpp"

#include "wfa/art_activity_bootstrap_fixture.hpp"

#include <filesystem>
#include <fstream>
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

std::string BuildExecutionPlanJson(
    const NativeArtBootstrapExecutionFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id)
         << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"activity_bootstrap_result_json_path\": \""
         << EscapeJson(report.activity_bootstrap_result_json_path)
         << "\",\n"
         << "  \"selected_application_class_name\": \""
         << EscapeJson(report.selected_application_class_name) << "\",\n"
         << "  \"selected_application_class_descriptor\": \""
         << EscapeJson(report.selected_application_class_descriptor)
         << "\",\n"
         << "  \"selected_activity_class_name\": \""
         << EscapeJson(report.selected_activity_class_name) << "\",\n"
         << "  \"selected_activity_class_descriptor\": \""
         << EscapeJson(report.selected_activity_class_descriptor)
         << "\",\n"
         << "  \"application_bootstrap_command\": \""
         << EscapeJson(report.application_bootstrap_command) << "\",\n"
         << "  \"activity_bootstrap_command\": \""
         << EscapeJson(report.activity_bootstrap_command) << "\",\n"
         << "  \"execution_attempt_planned\": "
         << (report.execution_attempt_planned ? "true" : "false") << ",\n"
         << "  \"bootstrap_sequence\": "
         << RenderJsonArray(report.bootstrap_sequence) << "\n"
         << "}\n";
  return output.str();
}

std::string BuildExecutionTraceJsonl(
    const NativeArtBootstrapExecutionFixtureReport& report) {
  std::ostringstream output;
  output << "{\"event_type\": \"bootstrap_execution_started\", "
         << "\"selected_application_class_name\": \""
         << EscapeJson(report.selected_application_class_name) << "\", "
         << "\"selected_activity_class_name\": \""
         << EscapeJson(report.selected_activity_class_name) << "\", "
         << "\"execution_attempt_planned\": "
         << (report.execution_attempt_planned ? "true" : "false") << "}\n";

  output << "{\"event_type\": \"bootstrap_execution_application_phase\", "
         << "\"attempted\": "
         << (report.application_execution_attempted ? "true" : "false")
         << ", "
         << "\"succeeded\": "
         << (report.application_execution_succeeded ? "true" : "false")
         << ", "
         << "\"command\": \""
         << EscapeJson(report.application_bootstrap_command) << "\"}\n";

  output << "{\"event_type\": \"bootstrap_execution_activity_phase\", "
         << "\"attempted\": "
         << (report.activity_execution_attempted ? "true" : "false") << ", "
         << "\"succeeded\": "
         << (report.activity_execution_succeeded ? "true" : "false")
         << ", "
         << "\"command\": \"" << EscapeJson(report.activity_bootstrap_command)
         << "\"}\n";

  output << "{\"event_type\": \"bootstrap_execution_complete\", "
         << "\"execution_attempted\": "
         << (report.execution_attempted ? "true" : "false") << ", "
         << "\"execution_succeeded\": "
         << (report.execution_succeeded ? "true" : "false") << ", "
         << "\"dependency_blocked\": "
         << (report.dependency_blocked ? "true" : "false") << ", "
         << "\"exit_reason\": \"" << EscapeJson(report.exit_reason) << "\"}\n";
  return output.str();
}

std::string BuildExecutionResultJson(
    const NativeArtBootstrapExecutionFixtureReport& report) {
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
         << "  \"activity_bootstrap_result_json_path\": \""
         << EscapeJson(report.activity_bootstrap_result_json_path)
         << "\",\n"
         << "  \"execution_plan_path\": \""
         << EscapeJson(report.execution_plan_path) << "\",\n"
         << "  \"trace_jsonl_path\": \"" << EscapeJson(report.trace_jsonl_path)
         << "\",\n"
         << "  \"result_json_path\": \"" << EscapeJson(report.result_json_path)
         << "\",\n"
         << "  \"selected_application_class_name\": \""
         << EscapeJson(report.selected_application_class_name) << "\",\n"
         << "  \"selected_application_class_descriptor\": \""
         << EscapeJson(report.selected_application_class_descriptor)
         << "\",\n"
         << "  \"selected_activity_class_name\": \""
         << EscapeJson(report.selected_activity_class_name) << "\",\n"
         << "  \"selected_activity_class_descriptor\": \""
         << EscapeJson(report.selected_activity_class_descriptor)
         << "\",\n"
         << "  \"application_bootstrap_command\": \""
         << EscapeJson(report.application_bootstrap_command) << "\",\n"
         << "  \"activity_bootstrap_command\": \""
         << EscapeJson(report.activity_bootstrap_command) << "\",\n"
         << "  \"art_runtime_detected\": "
         << (report.art_runtime_detected ? "true" : "false") << ",\n"
         << "  \"safe_runtime_probe_available\": "
         << (report.safe_runtime_probe_available ? "true" : "false")
         << ",\n"
         << "  \"runtime_class_resolution_succeeded\": "
         << (report.runtime_class_resolution_succeeded ? "true" : "false")
         << ",\n"
         << "  \"execution_attempt_planned\": "
         << (report.execution_attempt_planned ? "true" : "false") << ",\n"
         << "  \"execution_attempted\": "
         << (report.execution_attempted ? "true" : "false") << ",\n"
         << "  \"execution_succeeded\": "
         << (report.execution_succeeded ? "true" : "false") << ",\n"
         << "  \"application_execution_attempted\": "
         << (report.application_execution_attempted ? "true" : "false")
         << ",\n"
         << "  \"application_execution_succeeded\": "
         << (report.application_execution_succeeded ? "true" : "false")
         << ",\n"
         << "  \"activity_execution_attempted\": "
         << (report.activity_execution_attempted ? "true" : "false") << ",\n"
         << "  \"activity_execution_succeeded\": "
         << (report.activity_execution_succeeded ? "true" : "false")
         << ",\n"
         << "  \"dependency_blocked\": "
         << (report.dependency_blocked ? "true" : "false") << ",\n"
         << "  \"dependency_count\": " << report.dependency_count << ",\n"
         << "  \"missing_dependencies\": "
         << RenderJsonArray(report.missing_dependencies) << ",\n"
         << "  \"bootstrap_sequence\": "
         << RenderJsonArray(report.bootstrap_sequence) << ",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\"\n"
         << "}\n";
  return output.str();
}

}  // namespace

NativeArtBootstrapExecutionFixtureReport
BuildNativeArtBootstrapExecutionFixture(
    const NativeArtActivityBootstrapFixtureReport& activity) {
  NativeArtBootstrapExecutionFixtureReport report;
  report.package_name = activity.package_name;
  report.install_id = activity.install_id;
  report.bootstrap_manifest_path = activity.bootstrap_manifest_path;
  report.session_root = activity.session_root;
  report.artifact_root = activity.artifact_root;
  report.activity_bootstrap_result_json_path = activity.result_json_path;
  report.execution_plan_path =
      (fs::path(report.artifact_root) / "bootstrap-execution-plan.json")
          .string();
  report.trace_jsonl_path =
      (fs::path(report.artifact_root) / "bootstrap-execution-trace.jsonl")
          .string();
  report.result_json_path =
      (fs::path(report.artifact_root) / "bootstrap-execution-result.json")
          .string();
  report.selected_application_class_name =
      activity.selected_application_class_name;
  report.selected_application_class_descriptor =
      activity.selected_application_class_descriptor;
  report.selected_activity_class_name = activity.selected_activity_class_name;
  report.selected_activity_class_descriptor =
      activity.selected_activity_class_descriptor;
  report.application_bootstrap_command =
      activity.application_bootstrap_command;
  report.activity_bootstrap_command = activity.activity_bootstrap_command;
  report.art_runtime_detected = activity.art_runtime_detected;
  report.safe_runtime_probe_available = activity.safe_runtime_probe_available;
  report.runtime_class_resolution_succeeded =
      activity.runtime_class_resolution_succeeded;
  report.execution_attempt_planned = activity.runtime_bootstrap_planned;
  report.execution_attempted = activity.runtime_bootstrap_attempted;
  report.execution_succeeded = activity.runtime_bootstrap_succeeded;
  report.application_execution_attempted =
      activity.application_probe_attempted;
  report.application_execution_succeeded =
      activity.application_probe_succeeded;
  report.activity_execution_attempted = activity.activity_probe_attempted;
  report.activity_execution_succeeded = activity.activity_probe_succeeded;
  report.dependency_blocked = activity.dependency_blocked;
  report.dependency_count = activity.dependency_count;
  report.missing_dependencies = activity.missing_dependencies;
  report.bootstrap_sequence = {
      "application_bootstrap_probe",
      "launcher_activity_bootstrap_probe"};

  if (!report.execution_attempt_planned) {
    report.exit_reason = "bootstrap_execution_not_planned";
  } else if (!report.art_runtime_detected) {
    report.exit_reason = "bootstrap_execution_runtime_not_detected";
  } else if (!report.safe_runtime_probe_available) {
    report.exit_reason = "bootstrap_execution_runtime_probe_unsafe";
  } else if (!report.runtime_class_resolution_succeeded) {
    report.exit_reason = "bootstrap_execution_class_resolution_incomplete";
  } else if (!report.execution_attempted) {
    report.exit_reason = "bootstrap_execution_not_attempted";
  } else if (!report.application_execution_succeeded) {
    report.exit_reason = "application_bootstrap_execution_failed";
  } else if (!report.activity_execution_succeeded) {
    report.exit_reason = "activity_bootstrap_execution_failed";
  } else {
    report.exit_reason = "bootstrap_execution_succeeded";
  }

  fs::create_directories(report.artifact_root);
  WriteTextFile(report.execution_plan_path, BuildExecutionPlanJson(report));
  WriteTextFile(report.trace_jsonl_path, BuildExecutionTraceJsonl(report));
  WriteTextFile(report.result_json_path, BuildExecutionResultJson(report));
  return report;
}

NativeArtBootstrapExecutionFixtureReport
RunNativeArtBootstrapExecutionFixture(
    const std::string& bootstrap_manifest_path) {
  const auto activity = RunNativeArtActivityBootstrapFixture(
      bootstrap_manifest_path);
  return BuildNativeArtBootstrapExecutionFixture(activity);
}

std::string RenderNativeArtBootstrapExecutionFixtureJson(
    const NativeArtBootstrapExecutionFixtureReport& report) {
  return BuildExecutionResultJson(report);
}

}  // namespace wfa
