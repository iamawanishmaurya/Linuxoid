#include "wfa/runtime_health.hpp"

#include "wfa/apk_archive.hpp"
#include "wfa/apk_loader.hpp"
#include "wfa/native_input_queue_fixture.hpp"
#include "wfa/native_window_surface.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

struct RuntimeObservationContext {
  NativeLifecycleShim lifecycle;
  ApkResourceReadinessReport resources;
  NativeWindowBridgeFixtureReport bridge;
  NativeInputQueueFixtureReport input;
  bool has_classes_dex = false;
};

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

bool FileExists(const std::string& path) {
  return !path.empty() && fs::exists(path);
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

bool HasArchiveEntry(const std::string& apk_path,
                     const std::string& entry_prefix,
                     const std::string& entry_suffix = "") {
  for (const auto& entry : ListApkArchiveEntries(apk_path)) {
    if (entry.path.rfind(entry_prefix, 0) != 0) {
      continue;
    }
    if (!entry_suffix.empty()) {
      if (entry.path.size() < entry_suffix.size() ||
          entry.path.substr(entry.path.size() - entry_suffix.size()) !=
              entry_suffix) {
        continue;
      }
    }
    return true;
  }
  return false;
}

std::string BuildTraceEventJson(int sequence, const std::string& event_type,
                                const std::string& subsystem_name,
                                const std::string& state, bool ready,
                                const std::string& detail,
                                const std::string& action_name,
                                const std::string& artifact_path) {
  std::ostringstream output;
  output << "{"
         << "\"sequence\": " << sequence << ", "
         << "\"event_type\": \"" << EscapeJson(event_type) << "\", "
         << "\"subsystem_name\": \"" << EscapeJson(subsystem_name) << "\", "
         << "\"state\": \"" << EscapeJson(state) << "\", "
         << "\"ready\": " << (ready ? "true" : "false") << ", "
         << "\"detail\": \"" << EscapeJson(detail) << "\", "
         << "\"action_name\": \"" << EscapeJson(action_name) << "\", "
         << "\"artifact_path\": \"" << EscapeJson(artifact_path) << "\""
         << "}";
  return output.str();
}

std::string BuildRecoveryActionName(const RuntimeHealthRecord& record) {
  if (record.subsystem_name == "apk_staging") {
    return "restage_apk_bundle";
  }
  if (record.subsystem_name == "native_loading") {
    return "retry_native_load_after_bundle_refresh";
  }
  if (record.subsystem_name == "surface_readiness") {
    return "fallback_to_headless_surface_probe";
  }
  if (record.subsystem_name == "binder_service_readiness") {
    return "rebuild_service_registry_and_retry_lookup";
  }
  if (record.subsystem_name == "dex_classloader_readiness") {
    return "prepare_art_sidecar_classpath";
  }
  if (record.subsystem_name == "input_queue_readiness") {
    return "recreate_input_queue_after_surface_ready";
  }
  return "no_recovery_action";
}

std::string BuildRecoveryReason(const RuntimeHealthRecord& record) {
  if (record.subsystem_name == "apk_staging") {
    return "APK bundle or manifest artifact is missing, so Linuxoid must restage before any runtime bootstrap can continue.";
  }
  if (record.subsystem_name == "native_loading") {
    return "Native libraries are unavailable or failed to load, so the runner should rebuild the ABI bundle before retrying.";
  }
  if (record.subsystem_name == "surface_readiness") {
    return "Display backing is unavailable, so Linuxoid should fall back to the headless probe path and keep diagnostics machine-readable.";
  }
  if (record.subsystem_name == "binder_service_readiness") {
    return "A service lookup failed, so the local Binder-shaped registry should be rebuilt before replaying the request.";
  }
  if (record.subsystem_name == "dex_classloader_readiness") {
    return "APK classes are present but ART/DEX bootstrap is still missing, so the next step is to prepare the classpath and sidecar runtime.";
  }
  if (record.subsystem_name == "input_queue_readiness") {
    return "Input delivery depends on a live surface contract, so Linuxoid should recreate the input queue after surface readiness improves.";
  }
  return "No deterministic recovery action is defined for this subsystem.";
}

RuntimeHealthRecord MakeHealthRecord(const std::string& subsystem_name,
                                     const std::string& state, bool ready,
                                     const std::string& artifact_path,
                                     const std::string& failure_reason,
                                     const std::string& evidence) {
  RuntimeHealthRecord record;
  record.subsystem_name = subsystem_name;
  record.state = state;
  record.ready = ready;
  record.artifact_path = artifact_path;
  record.failure_reason = failure_reason;
  record.evidence = evidence;
  if (!ready) {
    record.selected_recovery_action = BuildRecoveryActionName(record);
    record.recovery_reason = BuildRecoveryReason(record);
  }
  return record;
}

RuntimeHealthRecord BuildApkStagingRecord(
    const RuntimeObservationContext& context, const std::string& scenario) {
  const bool bundle_ready =
      FileExists(context.lifecycle.bootstrap.plan.bundle_apk_path) &&
      FileExists(context.lifecycle.bootstrap.bootstrap_manifest_path) &&
      context.resources.manifest.manifest_ready;

  if (scenario == "missing_artifact") {
    return MakeHealthRecord(
        "apk_staging", "missing", false,
        context.lifecycle.bootstrap.plan.bundle_apk_path,
        "required bootstrap artifact is missing",
        "Scenario forced a missing bundle artifact classification.");
  }

  return MakeHealthRecord(
      "apk_staging", bundle_ready ? "ready" : "missing", bundle_ready,
      context.lifecycle.bootstrap.plan.bundle_apk_path,
      bundle_ready ? "" : "bundle_or_manifest_not_ready",
      "manifest_ready=" +
          std::string(context.resources.manifest.manifest_ready ? "true"
                                                                : "false") +
      "; bundle_present=" +
          std::string(FileExists(context.lifecycle.bootstrap.plan.bundle_apk_path)
                          ? "true"
                          : "false"));
}

RuntimeHealthRecord BuildNativeLoadingRecord(
    const RuntimeObservationContext& context, const std::string& scenario) {
  const bool native_loading_ready =
      context.lifecycle.bootstrap.plan.host_abi_supported &&
      !context.lifecycle.bootstrap.plan.staged_native_libraries.empty();

  if (scenario == "failed_native_load") {
    return MakeHealthRecord(
        "native_loading", "blocked", false,
        context.lifecycle.bootstrap.plan.library_root,
        "native load failed before entrypoint resolution",
        "Scenario forced a failed native load classification.");
  }

  return MakeHealthRecord(
      "native_loading", native_loading_ready ? "ready" : "blocked",
      native_loading_ready, context.lifecycle.bootstrap.plan.library_root,
      native_loading_ready ? "" : "no_staged_host_abi_native_libraries",
      "selected_abi=" + context.lifecycle.bootstrap.plan.selected_abi +
          "; staged_native_libraries=" +
          std::to_string(
              context.lifecycle.bootstrap.plan.staged_native_libraries.size()));
}

RuntimeHealthRecord BuildSurfaceRecord(
    const RuntimeObservationContext& context, const std::string& scenario) {
  const bool surface_ready = context.bridge.native_window_bridge_ready;
  std::string state = surface_ready ? "ready" : "degraded";
  std::string failure_reason =
      surface_ready ? "" : context.bridge.exit_reason;
  std::string evidence = "backing_mode=" + context.bridge.backing_mode +
                         "; geometry_updates=" +
                         std::to_string(context.bridge.geometry_updates);

  if (scenario == "unavailable_display") {
    state = "degraded";
    failure_reason = "display_unavailable";
    evidence = "Scenario forced unavailable display classification.";
  }

  return MakeHealthRecord("surface_readiness", state,
                          scenario == "unavailable_display" ? false : surface_ready,
                          context.bridge.metadata_path, failure_reason, evidence);
}

RuntimeHealthRecord BuildInputRecord(const RuntimeObservationContext& context,
                                    const std::string& scenario) {
  if (scenario == "unavailable_display") {
    return MakeHealthRecord(
        "input_queue_readiness", "degraded", false,
        context.input.metadata_path, "surface_dependency_unavailable",
        "Input delivery is intentionally marked unavailable when the display path is unavailable.");
  }
  return MakeHealthRecord(
      "input_queue_readiness",
      context.input.input_queue_ready ? "ready" : "degraded",
      context.input.input_queue_ready, context.input.metadata_path,
      context.input.input_queue_ready ? "" : context.input.exit_reason,
      "pointer_events=" +
          std::to_string(context.input.pointer_events_injected) +
          "; key_events=" + std::to_string(context.input.key_events_injected));
}

RuntimeHealthRecord BuildBinderRecord(
    const RuntimeObservationContext& context, const std::string& scenario) {
  if (scenario == "failed_service_lookup") {
    return MakeHealthRecord(
        "binder_service_readiness", "blocked", false,
        context.lifecycle.binder_transport_log_path,
        "service_lookup_failed",
        "Scenario forced a failed local service lookup classification.");
  }

  return MakeHealthRecord(
      "binder_service_readiness",
      context.lifecycle.binder_service_manager_ready ? "ready" : "blocked",
      context.lifecycle.binder_service_manager_ready,
      context.lifecycle.binder_transport_log_path,
      context.lifecycle.binder_service_manager_ready ? "" : "binder_registry_unavailable",
      "transport_kind=" + context.lifecycle.binder_service_manager.transport_kind +
          "; transport_round_trips=" +
          std::to_string(
              context.lifecycle.binder_service_manager.transport_round_trips));
}

RuntimeHealthRecord BuildDexRecord(const RuntimeObservationContext& context) {
  const bool dex_present = context.has_classes_dex;
  if (!dex_present) {
    return MakeHealthRecord("dex_classloader_readiness", "not_required", true,
                            context.lifecycle.bootstrap.plan.bundle_apk_path, "",
                            "APK archive does not contain classes.dex.");
  }

  return MakeHealthRecord(
      "dex_classloader_readiness", "pending", false,
      context.lifecycle.bootstrap.plan.bundle_apk_path,
      "dex_classloader_unimplemented",
      "classes.dex is present in the APK, but Linuxoid has not attached ART or PathClassLoader yet.");
}

RuntimeHealthReplayReport BuildReplayReportFromEvents(
    const std::string& trace_jsonl_path, const std::vector<std::string>& lines) {
  RuntimeHealthReplayReport report;
  report.trace_jsonl_path = trace_jsonl_path;
  report.overall_state = "ready";
  report.exit_reason = "replay_complete";
  report.events_read = static_cast<int>(lines.size());

  std::set<std::string> subsystems;
  std::set<std::string> actions;
  std::set<std::string> failing;
  const std::regex subsystem_pattern("\"subsystem_name\": \"([^\"]+)\"");
  const std::regex state_pattern("\"state\": \"([^\"]+)\"");
  const std::regex action_pattern("\"action_name\": \"([^\"]+)\"");
  const std::regex ready_pattern("\"ready\": (true|false)");

  for (const auto& line : lines) {
    std::smatch subsystem_match;
    if (std::regex_search(line, subsystem_match, subsystem_pattern) &&
        subsystem_match.size() == 2) {
      subsystems.insert(subsystem_match[1].str());
    }

    std::smatch action_match;
    if (std::regex_search(line, action_match, action_pattern) &&
        action_match.size() == 2) {
      const std::string action_name = action_match[1].str();
      if (!action_name.empty() && action_name != "no_recovery_action") {
        actions.insert(action_name);
      }
    }

    bool ready = true;
    std::smatch ready_match;
    if (std::regex_search(line, ready_match, ready_pattern) &&
        ready_match.size() == 2) {
      ready = ready_match[1].str() == "true";
    }
    if (!ready) {
      report.overall_state = "recovery_needed";
      std::smatch subsystem_failure_match;
      if (std::regex_search(line, subsystem_failure_match, subsystem_pattern) &&
          subsystem_failure_match.size() == 2) {
        failing.insert(subsystem_failure_match[1].str());
      }
    }
  }

  report.subsystems_observed = static_cast<int>(subsystems.size());
  report.recovery_actions_selected = static_cast<int>(actions.size());
  report.failing_subsystems.assign(failing.begin(), failing.end());
  report.selected_actions.assign(actions.begin(), actions.end());
  if (!report.failing_subsystems.empty()) {
    report.exit_reason = "replay_identified_recovery_needed";
  }
  return report;
}

std::string BuildHealthTraceJsonl(const RuntimeHealthReport& report) {
  std::ostringstream output;
  int sequence = 1;
  for (const auto& record : report.records) {
    output << BuildTraceEventJson(sequence++, "health_recorded",
                                 record.subsystem_name, record.state,
                                 record.ready, record.evidence,
                                 record.selected_recovery_action,
                                 record.artifact_path)
           << "\n";
    if (!record.selected_recovery_action.empty()) {
      output << BuildTraceEventJson(sequence++, "recovery_selected",
                                   record.subsystem_name, record.state,
                                   record.ready, record.recovery_reason,
                                   record.selected_recovery_action,
                                   record.artifact_path)
             << "\n";
    }
  }
  return output.str();
}

}  // namespace

RuntimeHealthReport RunRuntimeHealthFixture(
    const std::string& bootstrap_manifest_path,
    const std::string& scenario_name) {
  RuntimeObservationContext context;
  context.lifecycle = BuildNativeLifecycleShimFromManifest(bootstrap_manifest_path);
  context.resources = InspectApkResourceReadiness(
      context.lifecycle.bootstrap.plan.bundle_apk_path,
      context.lifecycle.bootstrap.plan.resource_root);
  context.bridge = RunNativeWindowBridgeFixture(
      (fs::path(context.lifecycle.session_root) / "health" / "surface").string(),
      {.width = 48, .height = 32, .format = kNativeWindowFormatRgba8888, .stride = 48});
  context.input = RunNativeInputQueueFixture(
      (fs::path(context.lifecycle.session_root) / "health" / "input").string(),
      {.width = 48, .height = 32, .format = kNativeWindowFormatRgba8888, .stride = 48});
  context.has_classes_dex =
      HasArchiveEntry(context.lifecycle.bootstrap.plan.bundle_apk_path, "classes",
                      ".dex");

  RuntimeHealthReport report;
  report.package_name = context.lifecycle.bootstrap.plan.assessment.package_name;
  report.install_id = context.lifecycle.bootstrap.plan.assessment.install_id;
  report.bootstrap_manifest_path = bootstrap_manifest_path;
  report.session_root = context.lifecycle.session_root;
  report.artifact_root =
      (fs::path(context.lifecycle.session_root) / "health").string();
  report.health_json_path =
      (fs::path(report.artifact_root) / "runtime-health.json").string();
  report.trace_jsonl_path =
      (fs::path(report.artifact_root) / "runtime-health-trace.jsonl").string();
  report.replay_json_path =
      (fs::path(report.artifact_root) / "runtime-health-replay.json").string();
  report.scenario_name = scenario_name;
  fs::create_directories(report.artifact_root);

  report.records.push_back(BuildApkStagingRecord(context, scenario_name));
  report.records.push_back(BuildNativeLoadingRecord(context, scenario_name));
  report.records.push_back(BuildSurfaceRecord(context, scenario_name));
  report.records.push_back(BuildInputRecord(context, scenario_name));
  report.records.push_back(BuildBinderRecord(context, scenario_name));
  report.records.push_back(BuildDexRecord(context));

  report.overall_ready = true;
  report.self_healing_ready = true;
  report.overall_state = "ready";
  report.exit_reason = "runtime_health_ready";

  for (const auto& record : report.records) {
    if (!record.ready) {
      report.overall_ready = false;
      report.overall_state = "recovery_needed";
      if (!record.selected_recovery_action.empty()) {
        report.recovery_actions.push_back(
            {.subsystem_name = record.subsystem_name,
             .action_name = record.selected_recovery_action,
             .action_state = "planned",
             .action_reason = record.recovery_reason});
      }
      if (record.subsystem_name == "dex_classloader_readiness" &&
          record.state == "pending") {
        continue;
      }
    }
  }

  if (!report.overall_ready) {
    report.exit_reason = "runtime_recovery_plan_required";
  }

  const std::string trace_jsonl = BuildHealthTraceJsonl(report);
  WriteTextFile(report.trace_jsonl_path, trace_jsonl);
  WriteTextFile(report.health_json_path, RenderRuntimeHealthReportJson(report));

  std::vector<std::string> trace_lines;
  std::istringstream trace_input(trace_jsonl);
  for (std::string line; std::getline(trace_input, line);) {
    if (!line.empty()) {
      trace_lines.push_back(line);
    }
  }
  const auto replay = BuildReplayReportFromEvents(report.trace_jsonl_path, trace_lines);
  WriteTextFile(report.replay_json_path, RenderRuntimeHealthReplayJson(replay));

  return report;
}

std::string RenderRuntimeHealthReportJson(const RuntimeHealthReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name) << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id) << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"session_root\": \"" << EscapeJson(report.session_root) << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root) << "\",\n"
         << "  \"health_json_path\": \"" << EscapeJson(report.health_json_path)
         << "\",\n"
         << "  \"trace_jsonl_path\": \"" << EscapeJson(report.trace_jsonl_path)
         << "\",\n"
         << "  \"replay_json_path\": \"" << EscapeJson(report.replay_json_path)
         << "\",\n"
         << "  \"scenario_name\": \"" << EscapeJson(report.scenario_name)
         << "\",\n"
         << "  \"self_healing_ready\": "
         << (report.self_healing_ready ? "true" : "false") << ",\n"
         << "  \"overall_ready\": " << (report.overall_ready ? "true" : "false")
         << ",\n"
         << "  \"overall_state\": \"" << EscapeJson(report.overall_state)
         << "\",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason) << "\",\n"
         << "  \"records\": [\n";
  for (std::size_t index = 0; index < report.records.size(); ++index) {
    const auto& record = report.records[index];
    if (index != 0) {
      output << ",\n";
    }
    output << "    {"
           << "\"subsystem_name\": \"" << EscapeJson(record.subsystem_name)
           << "\", "
           << "\"state\": \"" << EscapeJson(record.state) << "\", "
           << "\"ready\": " << (record.ready ? "true" : "false") << ", "
           << "\"artifact_path\": \"" << EscapeJson(record.artifact_path)
           << "\", "
           << "\"failure_reason\": \"" << EscapeJson(record.failure_reason)
           << "\", "
           << "\"evidence\": \"" << EscapeJson(record.evidence) << "\", "
           << "\"selected_recovery_action\": \""
           << EscapeJson(record.selected_recovery_action) << "\", "
           << "\"recovery_reason\": \"" << EscapeJson(record.recovery_reason)
           << "\""
           << "}";
  }
  output << "\n  ],\n"
         << "  \"recovery_actions\": [\n";
  for (std::size_t index = 0; index < report.recovery_actions.size(); ++index) {
    const auto& action = report.recovery_actions[index];
    if (index != 0) {
      output << ",\n";
    }
    output << "    {"
           << "\"subsystem_name\": \"" << EscapeJson(action.subsystem_name)
           << "\", "
           << "\"action_name\": \"" << EscapeJson(action.action_name) << "\", "
           << "\"action_state\": \"" << EscapeJson(action.action_state)
           << "\", "
           << "\"action_reason\": \"" << EscapeJson(action.action_reason)
           << "\""
           << "}";
  }
  output << "\n  ]\n"
         << "}\n";
  return output.str();
}

RuntimeHealthReplayReport ReplayRuntimeHealthTrace(
    const std::string& trace_jsonl_path) {
  const std::string contents = ReadFile(trace_jsonl_path);
  std::vector<std::string> lines;
  std::istringstream input(contents);
  for (std::string line; std::getline(input, line);) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return BuildReplayReportFromEvents(trace_jsonl_path, lines);
}

std::string RenderRuntimeHealthReplayJson(
    const RuntimeHealthReplayReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"trace_jsonl_path\": \"" << EscapeJson(report.trace_jsonl_path)
         << "\",\n"
         << "  \"overall_state\": \"" << EscapeJson(report.overall_state)
         << "\",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\",\n"
         << "  \"events_read\": " << report.events_read << ",\n"
         << "  \"subsystems_observed\": " << report.subsystems_observed
         << ",\n"
         << "  \"recovery_actions_selected\": "
         << report.recovery_actions_selected << ",\n"
         << "  \"failing_subsystems\": "
         << RenderJsonArray(report.failing_subsystems) << ",\n"
         << "  \"selected_actions\": "
         << RenderJsonArray(report.selected_actions) << "\n"
         << "}\n";
  return output.str();
}

}  // namespace wfa
