#include "wfa/art_bootstrap_execution_fixture.hpp"

#include "wfa/art_activity_bootstrap_fixture.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>

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

std::string ReadTextFile(const fs::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to read file: " + path.string());
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void WriteExecutableFile(const fs::path& path, const std::string& contents) {
  WriteTextFile(path, contents);
  fs::permissions(path,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);
}

struct CommandCaptureResult {
  int exit_code = -1;
  std::string output;
};

std::string QuoteForShell(const std::string& value) {
  std::string quoted = "'";
  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(character);
    }
  }
  quoted += "'";
  return quoted;
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

bool LooksLikeResolvedClassWithoutMain(const std::string& output) {
  return output.find("main") != std::string::npos &&
         (output.find("No static") != std::string::npos ||
          output.find("main method") != std::string::npos ||
          output.find("Main method") != std::string::npos);
}

bool LooksLikeMissingClassFailure(const std::string& output) {
  return output.find("ClassNotFoundException") != std::string::npos ||
         output.find("Didn't find class") != std::string::npos ||
         output.find("Could not find class") != std::string::npos;
}

bool ProbeCommandSucceeded(const CommandCaptureResult& result) {
  if (result.exit_code == 0) {
    return true;
  }
  if (LooksLikeMissingClassFailure(result.output)) {
    return false;
  }
  return LooksLikeResolvedClassWithoutMain(result.output);
}

CommandCaptureResult RunCommandCapture(const std::string& command) {
  const std::string wrapped_command = command + " 2>&1";
  FILE* pipe = popen(wrapped_command.c_str(), "r");
  if (pipe == nullptr) {
    throw std::runtime_error(
        "unable to open bootstrap execution probe command");
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

std::string BuildExecutionContextJson(
    const NativeArtBootstrapExecutionFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id)
         << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"selected_application_class_name\": \""
         << EscapeJson(report.selected_application_class_name) << "\",\n"
         << "  \"selected_activity_class_name\": \""
         << EscapeJson(report.selected_activity_class_name) << "\",\n"
         << "  \"application_bootstrap_command\": \""
         << EscapeJson(report.application_bootstrap_command) << "\",\n"
         << "  \"activity_bootstrap_command\": \""
         << EscapeJson(report.activity_bootstrap_command) << "\",\n"
         << "  \"runner_state_json_path\": \""
         << EscapeJson(report.runner_state_json_path) << "\",\n"
         << "  \"application_execution_log_path\": \""
         << EscapeJson(report.application_execution_log_path) << "\",\n"
         << "  \"activity_execution_log_path\": \""
         << EscapeJson(report.activity_execution_log_path) << "\",\n"
         << "  \"bootstrap_sequence\": "
         << RenderJsonArray(report.bootstrap_sequence) << "\n"
         << "}\n";
  return output.str();
}

std::string BuildRunnerScript(
    const NativeArtBootstrapExecutionFixtureReport& report) {
  std::ostringstream output;
  output << "#!/bin/sh\n"
         << "set +e\n"
         << "APP_LOG=" << "'" << report.application_execution_log_path << "'"
         << "\n"
         << "ACTIVITY_LOG=" << "'" << report.activity_execution_log_path << "'"
         << "\n"
         << "STATE_JSON=" << "'" << report.runner_state_json_path << "'"
         << "\n"
         << "app_attempted=false\n"
         << "activity_attempted=false\n"
         << "app_exit=-1\n"
         << "activity_exit=-1\n";
  if (!report.application_bootstrap_command.empty()) {
    output << "app_attempted=true\n"
           << report.application_bootstrap_command << " > \"$APP_LOG\" 2>&1\n"
           << "app_exit=$?\n";
  } else {
    output
        << "printf '%s\\n' 'application phase not required' > \"$APP_LOG\"\n";
  }
  if (!report.activity_bootstrap_command.empty()) {
    output << "activity_attempted=true\n"
           << report.activity_bootstrap_command
           << " > \"$ACTIVITY_LOG\" 2>&1\n"
           << "activity_exit=$?\n";
  } else {
    output << "printf '%s\\n' 'activity phase not configured' > \"$ACTIVITY_LOG\"\n";
  }
  output << "runner_exit=0\n"
         << "if [ \"$app_exit\" -ne -1 ] && [ \"$app_exit\" -ne 0 ]; then\n"
         << "  runner_exit=$app_exit\n"
         << "fi\n"
         << "if [ \"$runner_exit\" -eq 0 ] && [ \"$activity_exit\" -ne -1 ] && [ \"$activity_exit\" -ne 0 ]; then\n"
         << "  runner_exit=$activity_exit\n"
         << "fi\n"
         << "cat > \"$STATE_JSON\" <<EOF\n"
         << "{\n"
         << "  \"application_attempted\": $app_attempted,\n"
         << "  \"application_exit_code\": $app_exit,\n"
         << "  \"activity_attempted\": $activity_attempted,\n"
         << "  \"activity_exit_code\": $activity_exit,\n"
         << "  \"runner_exit_code\": $runner_exit\n"
         << "}\n"
         << "EOF\n"
         << "exit \"$runner_exit\"\n";
  return output.str();
}

std::string BuildRunnerStateJson(
    const NativeArtBootstrapExecutionFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"runner_invoked\": "
         << (report.runner_invoked ? "true" : "false") << ",\n"
         << "  \"runner_exit_code\": " << report.runner_exit_code << ",\n"
         << "  \"application_attempted\": "
         << (report.application_execution_attempted ? "true" : "false")
         << ",\n"
         << "  \"application_exit_code\": " << report.application_exit_code
         << ",\n"
         << "  \"activity_attempted\": "
         << (report.activity_execution_attempted ? "true" : "false")
         << ",\n"
         << "  \"activity_exit_code\": " << report.activity_exit_code << "\n"
         << "}\n";
  return output.str();
}

void ApplyRunnerStateJson(const std::string& json,
                          NativeArtBootstrapExecutionFixtureReport* report) {
  const auto parse_bool = [&](const std::string& key, bool* value) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() == 2) {
      *value = match[1].str() == "true";
      return true;
    }
    return false;
  };
  const auto parse_int = [&](const std::string& key, int* value) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() == 2) {
      *value = std::stoi(match[1].str());
      return true;
    }
    return false;
  };

  parse_bool("application_attempted", &report->application_execution_attempted);
  parse_bool("activity_attempted", &report->activity_execution_attempted);
  parse_int("application_exit_code", &report->application_exit_code);
  parse_int("activity_exit_code", &report->activity_exit_code);
  parse_int("runner_exit_code", &report->runner_exit_code);
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
         << "  \"execution_context_json_path\": \""
         << EscapeJson(report.execution_context_json_path) << "\",\n"
         << "  \"runner_script_path\": \""
         << EscapeJson(report.runner_script_path) << "\",\n"
         << "  \"runner_state_json_path\": \""
         << EscapeJson(report.runner_state_json_path) << "\",\n"
         << "  \"application_execution_log_path\": \""
         << EscapeJson(report.application_execution_log_path) << "\",\n"
         << "  \"activity_execution_log_path\": \""
         << EscapeJson(report.activity_execution_log_path) << "\",\n"
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
         << "  \"runner_invoked\": "
         << (report.runner_invoked ? "true" : "false") << ",\n"
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
         << "\"runner_invoked\": "
         << (report.runner_invoked ? "true" : "false") << ", "
         << "\"runner_exit_code\": " << report.runner_exit_code << ", "
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
         << "  \"execution_context_json_path\": \""
         << EscapeJson(report.execution_context_json_path) << "\",\n"
         << "  \"runner_script_path\": \""
         << EscapeJson(report.runner_script_path) << "\",\n"
         << "  \"runner_state_json_path\": \""
         << EscapeJson(report.runner_state_json_path) << "\",\n"
         << "  \"application_execution_log_path\": \""
         << EscapeJson(report.application_execution_log_path) << "\",\n"
         << "  \"activity_execution_log_path\": \""
         << EscapeJson(report.activity_execution_log_path) << "\",\n"
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
         << "  \"runner_invoked\": "
         << (report.runner_invoked ? "true" : "false") << ",\n"
         << "  \"runner_exit_code\": " << report.runner_exit_code << ",\n"
        << "  \"execution_attempted\": "
         << (report.execution_attempted ? "true" : "false") << ",\n"
         << "  \"execution_succeeded\": "
         << (report.execution_succeeded ? "true" : "false") << ",\n"
         << "  \"application_execution_attempted\": "
         << (report.application_execution_attempted ? "true" : "false")
         << ",\n"
         << "  \"application_exit_code\": " << report.application_exit_code
         << ",\n"
         << "  \"application_execution_succeeded\": "
         << (report.application_execution_succeeded ? "true" : "false")
         << ",\n"
         << "  \"activity_execution_attempted\": "
         << (report.activity_execution_attempted ? "true" : "false") << ",\n"
         << "  \"activity_exit_code\": " << report.activity_exit_code
         << ",\n"
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
  report.execution_context_json_path =
      (fs::path(report.artifact_root) / "bootstrap-execution-context.json")
          .string();
  report.runner_script_path =
      (fs::path(report.artifact_root) / "bootstrap-execution-runner.sh")
          .string();
  report.runner_state_json_path =
      (fs::path(report.artifact_root) / "bootstrap-execution-runner-state.json")
          .string();
  report.application_execution_log_path =
      (fs::path(report.artifact_root) / "bootstrap-execution-application.log")
          .string();
  report.activity_execution_log_path =
      (fs::path(report.artifact_root) / "bootstrap-execution-activity.log")
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
      false;
  report.application_execution_succeeded =
      activity.selected_application_class_name.empty();
  report.activity_execution_attempted = false;
  report.activity_execution_succeeded = false;
  report.runner_invoked = false;
  report.runner_exit_code = -1;
  report.application_exit_code = -1;
  report.activity_exit_code = -1;
  report.dependency_blocked = activity.dependency_blocked;
  report.dependency_count = activity.dependency_count;
  report.missing_dependencies = activity.missing_dependencies;
  report.bootstrap_sequence = {
      "application_bootstrap_probe",
      "launcher_activity_bootstrap_probe"};

  CommandCaptureResult application_capture;
  CommandCaptureResult activity_capture;
  if (!report.execution_attempt_planned) {
    report.exit_reason = "bootstrap_execution_not_planned";
  } else if (!report.art_runtime_detected) {
    report.exit_reason = "bootstrap_execution_runtime_not_detected";
  } else if (!report.safe_runtime_probe_available) {
    report.exit_reason = "bootstrap_execution_runtime_probe_unsafe";
  } else if (!report.runtime_class_resolution_succeeded) {
    report.exit_reason = "bootstrap_execution_class_resolution_incomplete";
  } else {
    report.runner_invoked = true;
  }

  fs::create_directories(report.artifact_root);
  WriteTextFile(report.execution_context_json_path,
                BuildExecutionContextJson(report));
  WriteExecutableFile(report.runner_script_path, BuildRunnerScript(report));
  if (report.runner_invoked) {
    static_cast<void>(
        RunCommandCapture(QuoteForShell(report.runner_script_path)));
    if (fs::exists(report.runner_state_json_path)) {
      ApplyRunnerStateJson(ReadTextFile(report.runner_state_json_path), &report);
    } else {
      report.runner_exit_code = -2;
      report.exit_reason = "bootstrap_execution_runner_state_missing";
    }

    application_capture.exit_code = report.application_exit_code;
    activity_capture.exit_code = report.activity_exit_code;
    application_capture.output = ReadTextFile(report.application_execution_log_path);
    activity_capture.output = ReadTextFile(report.activity_execution_log_path);
    report.application_execution_succeeded =
        !report.application_execution_attempted ||
        ProbeCommandSucceeded(application_capture);
    report.activity_execution_succeeded =
        !report.activity_execution_attempted ||
        ProbeCommandSucceeded(activity_capture);
    report.execution_attempted = report.application_execution_attempted ||
                                 report.activity_execution_attempted;
    report.execution_succeeded = report.application_execution_succeeded &&
                                 report.activity_execution_succeeded;
    report.dependency_blocked = !report.execution_succeeded;
    if (!report.execution_attempted) {
      report.exit_reason = "bootstrap_execution_not_attempted";
    } else if (!report.application_execution_succeeded) {
      report.exit_reason = "application_bootstrap_execution_failed";
    } else if (!report.activity_execution_succeeded) {
      report.exit_reason = "activity_bootstrap_execution_failed";
    } else {
      report.exit_reason = "bootstrap_execution_succeeded";
    }
  } else {
    WriteTextFile(report.application_execution_log_path,
                  "application phase not attempted\n");
    WriteTextFile(report.activity_execution_log_path,
                  "activity phase not attempted\n");
  }
  WriteTextFile(report.runner_state_json_path, BuildRunnerStateJson(report));
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
