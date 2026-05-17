#include "wfa/art_runtime_smoke.hpp"

#include "wfa/art_classloader_fixture.hpp"
#include "wfa/art_class_resolution_fixture.hpp"
#include "wfa/native_lifecycle.hpp"

#include <sys/wait.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
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

bool IsSafeRuntimeProbe(const std::string& runtime_probe_path) {
  if (runtime_probe_path.empty()) {
    return false;
  }
  const char* override_path = std::getenv("LINUXOID_ART_RUNTIME_PROBE_OVERRIDE");
  if (override_path != nullptr && override_path[0] != '\0' &&
      runtime_probe_path == override_path) {
    return true;
  }
  const fs::path runtime_path(runtime_probe_path);
  return runtime_path.filename() == "dalvikvm";
}

std::string ClassifyArtRuntimeProbeSource(const std::string& runtime_probe_path) {
  if (runtime_probe_path.empty() ||
      runtime_probe_path == "art_runtime_not_detected") {
    return "missing";
  }
  const char* override_path = std::getenv("LINUXOID_ART_RUNTIME_PROBE_OVERRIDE");
  if (override_path != nullptr && override_path[0] != '\0' &&
      runtime_probe_path == override_path) {
    return "override";
  }
  return "host";
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

CommandCaptureResult RunCommandCapture(const std::string& command) {
  const std::string wrapped_command = command + " 2>&1";
  FILE* pipe = popen(wrapped_command.c_str(), "r");
  if (pipe == nullptr) {
    throw std::runtime_error("unable to open runtime probe command");
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

std::string BuildInvocationPlanJson(const NativeArtRuntimeSmokeReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id)
         << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"dex_inventory_path\": \""
         << EscapeJson(report.dex_inventory_path) << "\",\n"
         << "  \"classloader_plan_path\": \""
         << EscapeJson(report.classloader_plan_path) << "\",\n"
         << "  \"class_resolution_map_path\": \""
         << EscapeJson(report.class_resolution_map_path) << "\",\n"
         << "  \"class_resolution_result_json_path\": \""
         << EscapeJson(report.class_resolution_result_json_path) << "\",\n"
         << "  \"art_runtime_detected\": "
         << (report.art_runtime_detected ? "true" : "false") << ",\n"
         << "  \"offline_resolution_ready\": "
         << (report.offline_resolution_ready ? "true" : "false") << ",\n"
         << "  \"safe_runtime_probe_available\": "
         << (report.safe_runtime_probe_available ? "true" : "false") << ",\n"
         << "  \"art_runtime_probe_source\": \""
         << EscapeJson(report.art_runtime_probe_source) << "\",\n"
         << "  \"runtime_probe_command\": \""
         << EscapeJson(report.runtime_probe_command) << "\",\n"
         << "  \"resolved_target_class_name\": \""
         << EscapeJson(report.resolved_target_class_name) << "\",\n"
         << "  \"resolved_target_class_descriptor\": \""
         << EscapeJson(report.resolved_target_class_descriptor) << "\",\n"
         << "  \"pathclassloader_resolution_planned\": "
         << (report.pathclassloader_resolution_planned ? "true" : "false")
         << ",\n"
         << "  \"pathclassloader_resolution_attempted\": "
         << (report.pathclassloader_resolution_attempted ? "true" : "false")
         << ",\n"
         << "  \"runtime_class_resolution_succeeded\": "
         << (report.runtime_class_resolution_succeeded ? "true" : "false")
         << ",\n"
         << "  \"resolved_target_count\": " << report.resolved_target_count
         << ",\n"
         << "  \"missing_target_count\": " << report.missing_target_count
         << ",\n"
         << "  \"target_class_names\": "
         << RenderJsonArray(report.target_class_names) << ",\n"
         << "  \"target_class_descriptors\": "
         << RenderJsonArray(report.target_class_descriptors) << ",\n"
         << "  \"dex_entry_paths\": " << RenderJsonArray(report.dex_entry_paths)
         << "\n"
         << "}\n";
  return output.str();
}

std::string BuildRuntimeLog(const NativeArtRuntimeSmokeReport& report,
                            const CommandCaptureResult& command_result) {
  std::ostringstream output;
  output << "Package: " << report.package_name << "\n";
  output << "Install ID: " << report.install_id << "\n";
  output << "Runtime Probe: " << report.art_runtime_probe << "\n";
  output << "Runtime Probe Command: " << report.runtime_probe_command << "\n";
  output << "Resolved Target Class Name: "
         << report.resolved_target_class_name << "\n";
  output << "Resolved Target Class Descriptor: "
         << report.resolved_target_class_descriptor << "\n";
  output << "Offline Resolution Ready: "
         << (report.offline_resolution_ready ? "yes" : "no") << "\n";
  output << "Resolved Target Count: " << report.resolved_target_count << "\n";
  output << "Missing Target Count: " << report.missing_target_count << "\n";
  output << "Runtime Probe Attempted: "
         << (report.runtime_probe_attempted ? "yes" : "no") << "\n";
  output << "Runtime Probe Succeeded: "
         << (report.runtime_probe_succeeded ? "yes" : "no") << "\n";
  output << "Runtime Exit Code: " << report.runtime_exit_code << "\n";
  output << "Exit Reason: " << report.exit_reason << "\n";
  output << "PathClassLoader Resolution Planned: "
         << (report.pathclassloader_resolution_planned ? "yes" : "no") << "\n";
  output << "PathClassLoader Resolution Attempted: "
         << (report.pathclassloader_resolution_attempted ? "yes" : "no")
         << "\n";
  output << "Runtime Class Resolution Succeeded: "
         << (report.runtime_class_resolution_succeeded ? "yes" : "no")
         << "\n";
  output << "Captured Output:\n" << command_result.output;
  return output.str();
}

std::string BuildRuntimeTraceJsonl(const NativeArtRuntimeSmokeReport& report,
                                   const CommandCaptureResult& command_result) {
  std::ostringstream output;
  output << "{\"event_type\": \"runtime_smoke_started\", "
         << "\"classpath_plan_ready\": "
         << (report.classpath_plan_ready ? "true" : "false") << ", "
         << "\"offline_resolution_ready\": "
         << (report.offline_resolution_ready ? "true" : "false") << ", "
         << "\"art_runtime_detected\": "
         << (report.art_runtime_detected ? "true" : "false") << ", "
         << "\"safe_runtime_probe_available\": "
         << (report.safe_runtime_probe_available ? "true" : "false") << ", "
         << "\"art_runtime_probe_source\": \""
         << EscapeJson(report.art_runtime_probe_source) << "\", "
         << "\"resolved_target_class_name\": \""
         << EscapeJson(report.resolved_target_class_name) << "\", "
         << "\"resolved_target_class_descriptor\": \""
         << EscapeJson(report.resolved_target_class_descriptor) << "\", "
         << "\"target_class_descriptors\": "
         << RenderJsonArray(report.target_class_descriptors) << "}\n";

  if (!report.runtime_probe_attempted) {
    output << "{\"event_type\": \"runtime_probe_skipped\", "
           << "\"runtime_probe_command\": \""
           << EscapeJson(report.runtime_probe_command) << "\", "
           << "\"exit_reason\": \"" << EscapeJson(report.exit_reason) << "\"}\n";
  } else {
    output << "{\"event_type\": \"runtime_probe_attempted\", "
           << "\"runtime_probe_command\": \""
           << EscapeJson(report.runtime_probe_command) << "\", "
           << "\"resolved_target_class_name\": \""
           << EscapeJson(report.resolved_target_class_name) << "\"}\n";
    output << "{\"event_type\": \"runtime_probe_result\", "
           << "\"runtime_exit_code\": " << report.runtime_exit_code << ", "
           << "\"runtime_probe_succeeded\": "
           << (report.runtime_probe_succeeded ? "true" : "false") << ", "
           << "\"runtime_class_resolution_succeeded\": "
           << (report.runtime_class_resolution_succeeded ? "true" : "false")
           << ", "
           << "\"captured_output\": \"" << EscapeJson(command_result.output)
           << "\"}\n";
  }

  output << "{\"event_type\": \"runtime_smoke_complete\", "
         << "\"pathclassloader_resolution_planned\": "
         << (report.pathclassloader_resolution_planned ? "true" : "false")
         << ", "
         << "\"pathclassloader_resolution_attempted\": "
         << (report.pathclassloader_resolution_attempted ? "true" : "false")
         << ", "
         << "\"runtime_class_resolution_succeeded\": "
         << (report.runtime_class_resolution_succeeded ? "true" : "false")
         << ", "
         << "\"resolved_target_count\": " << report.resolved_target_count << ", "
         << "\"missing_target_count\": " << report.missing_target_count << ", "
         << "\"runtime_probe_attempted\": "
         << (report.runtime_probe_attempted ? "true" : "false") << ", "
         << "\"runtime_probe_succeeded\": "
         << (report.runtime_probe_succeeded ? "true" : "false") << ", "
         << "\"exit_reason\": \"" << EscapeJson(report.exit_reason) << "\"}\n";
  return output.str();
}

}  // namespace

NativeArtRuntimeSmokeReport BuildNativeArtRuntimeSmokeFixture(
    const NativeArtClassResolutionFixtureReport& resolution_report) {
  const NativeLifecycleShim lifecycle =
      BuildNativeLifecycleShimFromManifest(
          resolution_report.bootstrap_manifest_path);

  NativeArtRuntimeSmokeReport report;
  report.package_name = resolution_report.package_name;
  report.install_id = resolution_report.install_id;
  report.bootstrap_manifest_path = resolution_report.bootstrap_manifest_path;
  report.artifact_root = resolution_report.artifact_root;
  report.dex_inventory_path = resolution_report.dex_inventory_path;
  report.classloader_plan_path = resolution_report.classloader_plan_path;
  report.classloader_trace_jsonl_path =
      resolution_report.classloader_trace_jsonl_path;
  report.class_resolution_map_path = resolution_report.resolution_map_path;
  report.class_resolution_trace_jsonl_path =
      resolution_report.trace_jsonl_path;
  report.class_resolution_result_json_path =
      resolution_report.result_json_path;
  report.invocation_plan_path =
      (fs::path(report.artifact_root) / "runtime-smoke-invocation-plan.json")
          .string();
  report.invocation_log_path =
      (fs::path(report.artifact_root) / "runtime-smoke-invocation.log").string();
  report.trace_jsonl_path =
      (fs::path(report.artifact_root) / "runtime-smoke-trace.jsonl").string();
  report.result_json_path =
      (fs::path(report.artifact_root) / "runtime-smoke-result.json").string();
  report.dex_entries_present = resolution_report.dex_entries_present;
  report.manifest_targets_ready = resolution_report.manifest_targets_ready;
  report.classpath_plan_ready = resolution_report.classpath_plan_ready;
  report.offline_resolution_ready = resolution_report.offline_resolution_ready;
  report.resolved_target_count = resolution_report.resolved_target_count;
  report.missing_target_count = resolution_report.missing_target_count;
  report.target_class_names = resolution_report.target_class_names;
  report.target_class_descriptors =
      resolution_report.target_class_descriptors;
  report.dex_entry_paths = resolution_report.dex_entry_paths;
  if (!resolution_report.target_results.empty()) {
    const auto resolved_it = std::find_if(
        resolution_report.target_results.begin(),
        resolution_report.target_results.end(),
        [](const ResolvedClassTarget& target) { return target.resolved_in_dex; });
    if (resolved_it != resolution_report.target_results.end()) {
      report.resolved_target_class_name = resolved_it->class_name;
      report.resolved_target_class_descriptor = resolved_it->class_descriptor;
    } else {
      report.resolved_target_class_name =
          resolution_report.target_results.front().class_name;
      report.resolved_target_class_descriptor =
          resolution_report.target_results.front().class_descriptor;
    }
  }

  const auto classloader_report =
      RunNativeArtClassloaderFixture(
          resolution_report.bootstrap_manifest_path);
  report.art_runtime_detected = classloader_report.art_runtime_detected;
  report.art_runtime_probe = classloader_report.art_runtime_probe;
  report.art_runtime_probe_source =
      ClassifyArtRuntimeProbeSource(classloader_report.art_runtime_probe);
  report.safe_runtime_probe_available =
      classloader_report.art_runtime_detected &&
      IsSafeRuntimeProbe(classloader_report.art_runtime_probe);
  report.pathclassloader_resolution_planned =
      report.offline_resolution_ready;
  report.pathclassloader_resolution_attempted = false;

  if (report.safe_runtime_probe_available &&
      !report.resolved_target_class_name.empty()) {
    report.runtime_probe_command =
        QuoteForShell(report.art_runtime_probe) + " -cp " +
        QuoteForShell(lifecycle.bootstrap.plan.bundle_apk_path) + " " +
        QuoteForShell(report.resolved_target_class_name);
  } else if (report.safe_runtime_probe_available) {
    report.runtime_probe_command =
        QuoteForShell(report.art_runtime_probe) + " -help";
  }

  WriteTextFile(report.invocation_plan_path, BuildInvocationPlanJson(report));

  CommandCaptureResult capture;
  if (!report.classpath_plan_ready) {
    report.exit_reason = "classpath_plan_not_ready";
  } else if (!report.offline_resolution_ready) {
    report.exit_reason = "offline_class_resolution_incomplete";
  } else if (report.resolved_target_class_name.empty()) {
    report.exit_reason = "no_runtime_resolution_target";
  } else if (!report.art_runtime_detected) {
    report.exit_reason = "art_runtime_not_detected";
  } else if (!report.safe_runtime_probe_available) {
    report.exit_reason = "art_runtime_detected_without_safe_probe";
  } else {
    report.runtime_probe_attempted = true;
    report.pathclassloader_resolution_attempted = true;
    capture = RunCommandCapture(report.runtime_probe_command);
    report.runtime_exit_code = capture.exit_code;
    report.runtime_probe_succeeded = capture.exit_code == 0;
    report.runtime_class_resolution_succeeded =
        report.runtime_probe_succeeded ||
        LooksLikeResolvedClassWithoutMain(capture.output);
    if (report.runtime_probe_succeeded) {
      report.exit_reason = "art_runtime_class_resolution_command_succeeded";
    } else if (report.runtime_class_resolution_succeeded) {
      report.exit_reason = "art_runtime_class_resolved_no_main";
    } else if (LooksLikeMissingClassFailure(capture.output)) {
      report.exit_reason = "art_runtime_class_not_found";
    } else {
      report.exit_reason = "art_runtime_probe_failed";
    }
  }

  WriteTextFile(report.invocation_log_path, BuildRuntimeLog(report, capture));
  WriteTextFile(report.trace_jsonl_path, BuildRuntimeTraceJsonl(report, capture));
  WriteTextFile(report.result_json_path,
                RenderNativeArtRuntimeSmokeFixtureJson(report));
  return report;
}

NativeArtRuntimeSmokeReport RunNativeArtRuntimeSmokeFixture(
    const std::string& bootstrap_manifest_path) {
  const auto resolution_report =
      RunNativeArtClassResolutionFixture(bootstrap_manifest_path);
  return BuildNativeArtRuntimeSmokeFixture(resolution_report);
}

std::string RenderNativeArtRuntimeSmokeFixtureJson(
    const NativeArtRuntimeSmokeReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id)
         << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"dex_inventory_path\": \""
         << EscapeJson(report.dex_inventory_path) << "\",\n"
         << "  \"classloader_plan_path\": \""
         << EscapeJson(report.classloader_plan_path) << "\",\n"
         << "  \"classloader_trace_jsonl_path\": \""
         << EscapeJson(report.classloader_trace_jsonl_path) << "\",\n"
         << "  \"class_resolution_map_path\": \""
         << EscapeJson(report.class_resolution_map_path) << "\",\n"
         << "  \"class_resolution_trace_jsonl_path\": \""
         << EscapeJson(report.class_resolution_trace_jsonl_path) << "\",\n"
         << "  \"class_resolution_result_json_path\": \""
         << EscapeJson(report.class_resolution_result_json_path) << "\",\n"
         << "  \"invocation_plan_path\": \""
         << EscapeJson(report.invocation_plan_path) << "\",\n"
         << "  \"invocation_log_path\": \""
         << EscapeJson(report.invocation_log_path) << "\",\n"
         << "  \"trace_jsonl_path\": \""
         << EscapeJson(report.trace_jsonl_path) << "\",\n"
         << "  \"result_json_path\": \""
         << EscapeJson(report.result_json_path) << "\",\n"
         << "  \"dex_entries_present\": "
         << (report.dex_entries_present ? "true" : "false") << ",\n"
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
         << "  \"runtime_probe_attempted\": "
         << (report.runtime_probe_attempted ? "true" : "false") << ",\n"
         << "  \"runtime_probe_succeeded\": "
         << (report.runtime_probe_succeeded ? "true" : "false") << ",\n"
         << "  \"pathclassloader_resolution_planned\": "
         << (report.pathclassloader_resolution_planned ? "true" : "false")
         << ",\n"
         << "  \"pathclassloader_resolution_attempted\": "
         << (report.pathclassloader_resolution_attempted ? "true" : "false")
         << ",\n"
         << "  \"runtime_class_resolution_succeeded\": "
         << (report.runtime_class_resolution_succeeded ? "true" : "false")
         << ",\n"
         << "  \"resolved_target_count\": " << report.resolved_target_count
         << ",\n"
         << "  \"missing_target_count\": " << report.missing_target_count
         << ",\n"
         << "  \"runtime_exit_code\": " << report.runtime_exit_code << ",\n"
         << "  \"art_runtime_probe\": \"" << EscapeJson(report.art_runtime_probe)
         << "\",\n"
         << "  \"art_runtime_probe_source\": \""
         << EscapeJson(report.art_runtime_probe_source)
         << "\",\n"
         << "  \"runtime_probe_command\": \""
         << EscapeJson(report.runtime_probe_command) << "\",\n"
         << "  \"resolved_target_class_name\": \""
         << EscapeJson(report.resolved_target_class_name) << "\",\n"
         << "  \"resolved_target_class_descriptor\": \""
         << EscapeJson(report.resolved_target_class_descriptor) << "\",\n"
         << "  \"target_class_names\": "
         << RenderJsonArray(report.target_class_names) << ",\n"
         << "  \"target_class_descriptors\": "
         << RenderJsonArray(report.target_class_descriptors) << ",\n"
         << "  \"dex_entry_paths\": "
         << RenderJsonArray(report.dex_entry_paths) << ",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\"\n"
         << "}\n";
  return output.str();
}

}  // namespace wfa
