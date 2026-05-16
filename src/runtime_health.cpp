#include "wfa/runtime_health.hpp"

#include "wfa/art_class_resolution_fixture.hpp"
#include "wfa/art_classloader_fixture.hpp"
#include "wfa/art_runtime_smoke.hpp"
#include "wfa/apk_archive.hpp"
#include "wfa/apk_loader.hpp"
#include "wfa/native_input_queue_fixture.hpp"
#include "wfa/native_window_surface.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
  NativeArtClassResolutionFixtureReport art_resolution;
  NativeArtRuntimeSmokeReport art_runtime;
  NativeWindowBridgeFixtureReport bridge;
  NativeInputQueueFixtureReport input;
  bool has_classes_dex = false;
};

struct RecoveryActionTemplate {
  std::string action_name;
  std::string action_reason;
  int action_rank = 0;
  int retry_budget = 0;
  std::string recovery_scope;
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

std::string ComputeDeterministicFingerprint(
    const std::vector<std::string>& lines) {
  std::uint64_t hash = 1469598103934665603ull;
  const auto mix_byte = [&](unsigned char byte) {
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= 1099511628211ull;
  };

  for (const auto& line : lines) {
    for (const unsigned char byte : line) {
      mix_byte(byte);
    }
    mix_byte(static_cast<unsigned char>('\n'));
  }

  std::ostringstream output;
  output << "fnv1a64:";
  output << std::hex << std::setfill('0') << std::setw(16) << hash;
  return output.str();
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

std::string BuildRecoveryActionId(const std::string& subsystem_name,
                                  const std::string& action_name) {
  return subsystem_name + "::" + action_name;
}

RecoveryActionTemplate BuildRecoveryActionTemplate(
    const std::string& subsystem_name) {
  if (subsystem_name == "apk_staging") {
    return {.action_name = "restage_apk_bundle",
            .action_reason =
                "APK bundle or manifest artifact is missing, so Linuxoid must restage before any runtime bootstrap can continue.",
            .action_rank = 10,
            .retry_budget = 1,
            .recovery_scope = "bundle"};
  }
  if (subsystem_name == "native_loading") {
    return {.action_name = "retry_native_load_after_bundle_refresh",
            .action_reason =
                "Native libraries are unavailable or failed to load, so the runner should rebuild the ABI bundle before retrying.",
            .action_rank = 20,
            .retry_budget = 1,
            .recovery_scope = "native_loader"};
  }
  if (subsystem_name == "surface_readiness") {
    return {.action_name = "fallback_to_headless_surface_probe",
            .action_reason =
                "Display backing is unavailable, so Linuxoid should fall back to the headless probe path and keep diagnostics machine-readable.",
            .action_rank = 30,
            .retry_budget = 0,
            .recovery_scope = "graphics_probe"};
  }
  if (subsystem_name == "binder_service_readiness") {
    return {.action_name = "rebuild_service_registry_and_retry_lookup",
            .action_reason =
                "A service lookup failed, so the local Binder-shaped registry should be rebuilt before replaying the request.",
            .action_rank = 40,
            .retry_budget = 1,
            .recovery_scope = "service_registry"};
  }
  if (subsystem_name == "dex_classloader_readiness") {
    return {.action_name = "attempt_host_art_class_resolution",
            .action_reason =
                "APK classes are now resolved offline from real DEX contents, so the next step is to attempt host-side ART or PathClassLoader class resolution against the staged bundle.",
            .action_rank = 50,
            .retry_budget = 0,
            .recovery_scope = "art_bridge"};
  }
  if (subsystem_name == "input_queue_readiness") {
    return {.action_name = "recreate_input_queue_after_surface_ready",
            .action_reason =
                "Input delivery depends on a live surface contract, so Linuxoid should recreate the input queue after surface readiness improves.",
            .action_rank = 35,
            .retry_budget = 1,
            .recovery_scope = "input_queue"};
  }
  return {.action_name = "no_recovery_action",
          .action_reason =
              "No deterministic recovery action is defined for this subsystem.",
          .action_rank = 999,
          .retry_budget = 0,
          .recovery_scope = "none"};
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
    const auto recovery = BuildRecoveryActionTemplate(record.subsystem_name);
    record.selected_recovery_action = recovery.action_name;
    record.recovery_reason = recovery.action_reason;
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
  const bool dex_present = context.art_resolution.dex_entries_present;
  if (!dex_present) {
    return MakeHealthRecord("dex_classloader_readiness", "not_required", true,
                            context.art_resolution.result_json_path, "",
                            "APK archive does not contain classes.dex entries.");
  }

  return MakeHealthRecord(
      "dex_classloader_readiness", "pending", false,
      context.art_resolution.result_json_path,
      "dex_classloader_unimplemented",
      "classpath_plan_ready=" +
          std::string(context.art_resolution.classpath_plan_ready ? "true"
                                                                  : "false") +
      "; offline_resolution_ready=" +
          std::string(context.art_resolution.offline_resolution_ready ? "true"
                                                                      : "false") +
      "; resolved_targets=" +
          std::to_string(context.art_resolution.resolved_target_count) +
      "; missing_targets=" +
          std::to_string(context.art_resolution.missing_target_count) +
      "; art_runtime_detected=" +
          std::string(context.art_runtime.art_runtime_detected ? "true"
                                                               : "false") +
      "; runtime_class_resolution_attempted=" +
          std::string(context.art_runtime.pathclassloader_resolution_attempted
                          ? "true"
                          : "false") +
      "; runtime_class_resolution_succeeded=" +
          std::string(context.art_runtime.runtime_class_resolution_succeeded
                          ? "true"
                          : "false"));
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

std::string BuildRecoveryActionsJsonl(const RuntimeHealthReport& report) {
  std::ostringstream output;
  int sequence = 1;
  for (const auto& action : report.recovery_actions) {
    output << "{"
           << "\"sequence\": " << sequence++ << ", "
           << "\"action_id\": \"" << EscapeJson(action.action_id) << "\", "
           << "\"subsystem_name\": \"" << EscapeJson(action.subsystem_name)
           << "\", "
           << "\"action_name\": \"" << EscapeJson(action.action_name) << "\", "
           << "\"action_state\": \"" << EscapeJson(action.action_state)
           << "\", "
           << "\"action_reason\": \"" << EscapeJson(action.action_reason)
           << "\", "
           << "\"action_rank\": " << action.action_rank << ", "
           << "\"retry_budget\": " << action.retry_budget << ", "
           << "\"recovery_scope\": \"" << EscapeJson(action.recovery_scope)
           << "\", "
           << "\"artifact_path\": \"" << EscapeJson(action.artifact_path)
           << "\", "
           << "\"replay_trace_path\": \""
           << EscapeJson(action.replay_trace_path) << "\""
           << "}\n";
  }
  return output.str();
}

std::string ExtractJsonStringField(const std::string& json_line,
                                   const std::string& field_name) {
  const std::regex pattern("\"" + field_name + "\"\\s*:\\s*\"([^\"]*)\"");
  std::smatch match;
  if (std::regex_search(json_line, match, pattern) && match.size() == 2) {
    return match[1].str();
  }
  return "";
}

bool ExtractJsonBoolField(const std::string& json_line,
                          const std::string& field_name,
                          bool* present) {
  const std::regex pattern("\"" + field_name + "\"\\s*:\\s*(true|false)");
  std::smatch match;
  if (std::regex_search(json_line, match, pattern) && match.size() == 2) {
    *present = true;
    return match[1].str() == "true";
  }
  *present = false;
  return false;
}

std::vector<std::string> ReadJsonlLinesIfPresent(const std::string& path) {
  if (!FileExists(path)) {
    return {};
  }
  std::vector<std::string> lines;
  std::istringstream input(ReadFile(path));
  for (std::string line; std::getline(input, line);) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}

std::string BuildDiagnosticMergedEventJson(int sequence,
                                           const std::string& source_name,
                                           const std::string& source_path,
                                           int source_line_number,
                                           const std::string& event_type,
                                           const std::string& raw_json) {
  std::ostringstream output;
  output << "{"
         << "\"sequence\": " << sequence << ", "
         << "\"source_name\": \"" << EscapeJson(source_name) << "\", "
         << "\"source_path\": \"" << EscapeJson(source_path) << "\", "
         << "\"source_line_number\": " << source_line_number << ", "
         << "\"event_type\": \"" << EscapeJson(event_type) << "\", "
         << "\"raw_json\": \"" << EscapeJson(raw_json) << "\""
         << "}";
  return output.str();
}

std::string RenderRuntimeDiagnosticTraceIndexJson(
    const RuntimeDiagnosticReplayReport& report) {
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
         << "  \"trace_index_json_path\": \""
         << EscapeJson(report.trace_index_json_path) << "\",\n"
         << "  \"merged_trace_jsonl_path\": \""
         << EscapeJson(report.merged_trace_jsonl_path) << "\",\n"
         << "  \"scenario_name\": \"" << EscapeJson(report.scenario_name)
         << "\",\n"
         << "  \"trace_sources_found\": " << report.trace_sources_found
         << ",\n"
         << "  \"missing_trace_sources\": "
         << RenderJsonArray(report.missing_trace_sources) << ",\n"
         << "  \"trace_sources\": [\n";
  for (std::size_t index = 0; index < report.trace_sources.size(); ++index) {
    const auto& source = report.trace_sources[index];
    if (index != 0) {
      output << ",\n";
    }
    output << "    {"
           << "\"source_name\": \"" << EscapeJson(source.source_name)
           << "\", "
           << "\"trace_path\": \"" << EscapeJson(source.trace_path) << "\", "
           << "\"present\": " << (source.present ? "true" : "false") << ", "
           << "\"events_read\": " << source.events_read << ", "
           << "\"first_event_type\": \""
           << EscapeJson(source.first_event_type) << "\", "
           << "\"last_event_type\": \""
           << EscapeJson(source.last_event_type) << "\", "
           << "\"source_fingerprint\": \""
           << EscapeJson(source.source_fingerprint) << "\", "
           << "\"failure_reason\": \""
           << EscapeJson(source.failure_reason) << "\""
           << "}";
  }
  output << "\n  ]\n"
         << "}\n";
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
  context.art_resolution =
      RunNativeArtClassResolutionFixture(bootstrap_manifest_path);
  context.art_runtime = RunNativeArtRuntimeSmokeFixture(bootstrap_manifest_path);
  context.bridge = RunNativeWindowBridgeFixture(
      (fs::path(context.lifecycle.session_root) / "health" / "surface").string(),
      {.width = 48, .height = 32, .format = kNativeWindowFormatRgba8888, .stride = 48});
  context.input = RunNativeInputQueueFixture(
      (fs::path(context.lifecycle.session_root) / "health" / "input").string(),
      {.width = 48, .height = 32, .format = kNativeWindowFormatRgba8888, .stride = 48});
  context.has_classes_dex = context.art_resolution.dex_entries_present;

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
  report.recovery_plan_path =
      (fs::path(report.artifact_root) / "runtime-recovery-plan.json").string();
  report.recovery_actions_jsonl_path =
      (fs::path(report.artifact_root) / "runtime-recovery-actions.jsonl")
          .string();
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
        const auto recovery = BuildRecoveryActionTemplate(record.subsystem_name);
        report.recovery_actions.push_back(
            {.action_id = BuildRecoveryActionId(record.subsystem_name,
                                                record.selected_recovery_action),
             .subsystem_name = record.subsystem_name,
             .action_name = record.selected_recovery_action,
             .action_state = "planned",
             .action_reason = record.recovery_reason,
             .action_rank = recovery.action_rank,
             .retry_budget = recovery.retry_budget,
             .recovery_scope = recovery.recovery_scope,
             .artifact_path = record.artifact_path,
             .replay_trace_path = report.trace_jsonl_path});
      }
      if (record.subsystem_name == "dex_classloader_readiness" &&
          record.state == "pending") {
        continue;
      }
    }
  }

  std::sort(report.recovery_actions.begin(), report.recovery_actions.end(),
            [](const RuntimeRecoveryAction& left,
               const RuntimeRecoveryAction& right) {
              if (left.action_rank != right.action_rank) {
                return left.action_rank < right.action_rank;
              }
              if (left.subsystem_name != right.subsystem_name) {
                return left.subsystem_name < right.subsystem_name;
              }
              return left.action_name < right.action_name;
            });

  if (!report.overall_ready) {
    report.exit_reason = "runtime_recovery_plan_required";
  }

  const std::string trace_jsonl = BuildHealthTraceJsonl(report);
  WriteTextFile(report.trace_jsonl_path, trace_jsonl);
  WriteTextFile(report.health_json_path, RenderRuntimeHealthReportJson(report));
  WriteTextFile(report.recovery_actions_jsonl_path,
                BuildRecoveryActionsJsonl(report));
  WriteTextFile(report.recovery_plan_path,
                RenderRuntimeRecoveryPlanJson(report));

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
         << "  \"recovery_plan_path\": \""
         << EscapeJson(report.recovery_plan_path) << "\",\n"
         << "  \"recovery_actions_jsonl_path\": \""
         << EscapeJson(report.recovery_actions_jsonl_path) << "\",\n"
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
           << "\"action_id\": \"" << EscapeJson(action.action_id) << "\", "
           << "\"subsystem_name\": \"" << EscapeJson(action.subsystem_name)
           << "\", "
           << "\"action_name\": \"" << EscapeJson(action.action_name) << "\", "
           << "\"action_state\": \"" << EscapeJson(action.action_state)
           << "\", "
           << "\"action_reason\": \"" << EscapeJson(action.action_reason)
           << "\", "
           << "\"action_rank\": " << action.action_rank << ", "
           << "\"retry_budget\": " << action.retry_budget << ", "
           << "\"recovery_scope\": \"" << EscapeJson(action.recovery_scope)
           << "\", "
           << "\"artifact_path\": \"" << EscapeJson(action.artifact_path)
           << "\", "
           << "\"replay_trace_path\": \""
           << EscapeJson(action.replay_trace_path) << "\""
           << "}";
  }
  output << "\n  ]\n"
         << "}\n";
  return output.str();
}

std::string RenderRuntimeRecoveryPlanJson(const RuntimeHealthReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id) << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"recovery_plan_path\": \""
         << EscapeJson(report.recovery_plan_path) << "\",\n"
         << "  \"recovery_actions_jsonl_path\": \""
         << EscapeJson(report.recovery_actions_jsonl_path) << "\",\n"
         << "  \"scenario_name\": \"" << EscapeJson(report.scenario_name)
         << "\",\n"
         << "  \"overall_state\": \"" << EscapeJson(report.overall_state)
         << "\",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason) << "\",\n"
         << "  \"recovery_actions\": [\n";
  for (std::size_t index = 0; index < report.recovery_actions.size(); ++index) {
    const auto& action = report.recovery_actions[index];
    if (index != 0) {
      output << ",\n";
    }
    output << "    {"
           << "\"action_id\": \"" << EscapeJson(action.action_id) << "\", "
           << "\"subsystem_name\": \"" << EscapeJson(action.subsystem_name)
           << "\", "
           << "\"action_name\": \"" << EscapeJson(action.action_name) << "\", "
           << "\"action_state\": \"" << EscapeJson(action.action_state)
           << "\", "
           << "\"action_reason\": \"" << EscapeJson(action.action_reason)
           << "\", "
           << "\"action_rank\": " << action.action_rank << ", "
           << "\"retry_budget\": " << action.retry_budget << ", "
           << "\"recovery_scope\": \"" << EscapeJson(action.recovery_scope)
           << "\", "
           << "\"artifact_path\": \"" << EscapeJson(action.artifact_path)
           << "\", "
           << "\"replay_trace_path\": \""
           << EscapeJson(action.replay_trace_path) << "\""
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

RuntimeDiagnosticReplayReport ReplayRuntimeDiagnosticBundle(
    const std::string& bootstrap_manifest_path,
    const std::string& scenario_name) {
  const NativeLifecycleShim lifecycle =
      BuildNativeLifecycleShimFromManifest(bootstrap_manifest_path);

  RuntimeDiagnosticReplayReport report;
  report.package_name = lifecycle.bootstrap.plan.assessment.package_name;
  report.install_id = lifecycle.bootstrap.plan.assessment.install_id;
  report.bootstrap_manifest_path = bootstrap_manifest_path;
  report.session_root = lifecycle.session_root;
  report.artifact_root = (fs::path(lifecycle.session_root) / "health").string();
  report.trace_index_json_path =
      (fs::path(report.artifact_root) / "runtime-diagnostic-trace-index.json")
          .string();
  report.merged_trace_jsonl_path =
      (fs::path(report.artifact_root) / "runtime-diagnostic-events.jsonl")
          .string();
  report.result_json_path =
      (fs::path(report.artifact_root) / "runtime-diagnostic-replay.json")
          .string();
  report.scenario_name = scenario_name;

  const std::vector<std::pair<std::string, std::string>> source_specs = {
      {"runtime_health_trace",
       (fs::path(report.artifact_root) / "runtime-health-trace.jsonl").string()},
      {"runtime_recovery_actions",
       (fs::path(report.artifact_root) / "runtime-recovery-actions.jsonl")
           .string()},
      {"art_classloader_trace",
       (fs::path(lifecycle.session_root) / "art" / "art-classloader-trace.jsonl")
           .string()},
      {"art_class_resolution_trace",
       (fs::path(lifecycle.session_root) / "art" /
        "art-class-resolution-trace.jsonl")
           .string()},
      {"art_runtime_smoke_trace",
       (fs::path(lifecycle.session_root) / "art" / "runtime-smoke-trace.jsonl")
           .string()},
  };

  std::set<std::string> failing_subsystems;
  std::set<std::string> selected_actions;
  std::set<std::string> unresolved_classes;
  std::ostringstream merged_trace;
  int merged_sequence = 1;

  for (const auto& [source_name, source_path] : source_specs) {
    RuntimeDiagnosticTraceSource source;
    source.source_name = source_name;
    source.trace_path = source_path;
    const auto lines = ReadJsonlLinesIfPresent(source_path);
    source.present = !lines.empty();
    source.events_read = static_cast<int>(lines.size());
    if (!source.present) {
      source.failure_reason = "trace_missing_or_empty";
      report.missing_trace_sources.push_back(source_name);
      report.trace_sources.push_back(source);
      continue;
    }

    ++report.trace_sources_found;
    report.total_events_read += source.events_read;
    source.source_fingerprint = ComputeDeterministicFingerprint(lines);
    int source_line_number = 1;
    for (const auto& line : lines) {
      std::string event_type = ExtractJsonStringField(line, "event_type");
      if (event_type.empty()) {
        event_type = source_name == "runtime_recovery_actions"
                         ? "recovery_action"
                         : "trace_event";
      }
      if (source.first_event_type.empty()) {
        source.first_event_type = event_type;
      }
      source.last_event_type = event_type;

      if (source_name == "runtime_health_trace") {
        bool ready_present = false;
        const bool ready = ExtractJsonBoolField(line, "ready", &ready_present);
        if (ready_present && !ready) {
          const std::string subsystem =
              ExtractJsonStringField(line, "subsystem_name");
          if (!subsystem.empty()) {
            failing_subsystems.insert(subsystem);
          }
        }
        const std::string action_name =
            ExtractJsonStringField(line, "action_name");
        if (!action_name.empty() && action_name != "no_recovery_action") {
          selected_actions.insert(action_name);
        }
      } else if (source_name == "runtime_recovery_actions") {
        const std::string action_name =
            ExtractJsonStringField(line, "action_name");
        if (!action_name.empty()) {
          selected_actions.insert(action_name);
        }
      } else if (source_name == "art_class_resolution_trace") {
        bool resolved_present = false;
        const bool resolved =
            ExtractJsonBoolField(line, "resolved_in_dex", &resolved_present);
        if (resolved_present && !resolved) {
          const std::string class_name =
              ExtractJsonStringField(line, "class_name");
          if (!class_name.empty()) {
            unresolved_classes.insert(class_name);
          }
        }
      } else if (source_name == "art_runtime_smoke_trace") {
        bool attempted_present = false;
        const bool attempted =
            ExtractJsonBoolField(line, "runtime_probe_attempted",
                                 &attempted_present);
        if (attempted_present) {
          report.runtime_probe_attempted =
              report.runtime_probe_attempted || attempted;
        }
        bool succeeded_present = false;
        const bool succeeded =
            ExtractJsonBoolField(line, "runtime_probe_succeeded",
                                 &succeeded_present);
        if (succeeded_present) {
          report.runtime_probe_succeeded =
              report.runtime_probe_succeeded || succeeded;
        }
      }

      merged_trace << BuildDiagnosticMergedEventJson(
                          merged_sequence++, source_name, source_path,
                          source_line_number++, event_type, line)
                   << "\n";
    }

    report.trace_sources.push_back(source);
  }

  report.replay_ready = report.missing_trace_sources.empty();
  report.failing_subsystems.assign(failing_subsystems.begin(),
                                   failing_subsystems.end());
  report.selected_actions.assign(selected_actions.begin(),
                                 selected_actions.end());
  report.unresolved_classes.assign(unresolved_classes.begin(),
                                   unresolved_classes.end());
  if (!report.replay_ready) {
    report.overall_state = "incomplete";
    report.exit_reason = "missing_trace_artifact";
  } else if (!report.failing_subsystems.empty() ||
             !report.unresolved_classes.empty()) {
    report.overall_state = "recovery_needed";
    report.exit_reason = "diagnostic_replay_identified_recovery_needed";
  } else {
    report.overall_state = "ready";
    report.exit_reason = "diagnostic_replay_ready";
  }

  fs::create_directories(report.artifact_root);
  WriteTextFile(report.merged_trace_jsonl_path, merged_trace.str());
  WriteTextFile(report.trace_index_json_path,
                RenderRuntimeDiagnosticTraceIndexJson(report));
  WriteTextFile(report.result_json_path,
                RenderRuntimeDiagnosticReplayJson(report));
  return report;
}

std::string RenderRuntimeDiagnosticReplayJson(
    const RuntimeDiagnosticReplayReport& report) {
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
         << "  \"trace_index_json_path\": \""
         << EscapeJson(report.trace_index_json_path) << "\",\n"
         << "  \"merged_trace_jsonl_path\": \""
         << EscapeJson(report.merged_trace_jsonl_path) << "\",\n"
         << "  \"result_json_path\": \"" << EscapeJson(report.result_json_path)
         << "\",\n"
         << "  \"scenario_name\": \"" << EscapeJson(report.scenario_name)
         << "\",\n"
         << "  \"replay_ready\": "
         << (report.replay_ready ? "true" : "false") << ",\n"
         << "  \"overall_state\": \"" << EscapeJson(report.overall_state)
         << "\",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\",\n"
         << "  \"total_events_read\": " << report.total_events_read << ",\n"
         << "  \"trace_sources_found\": " << report.trace_sources_found
         << ",\n"
         << "  \"runtime_probe_attempted\": "
         << (report.runtime_probe_attempted ? "true" : "false") << ",\n"
         << "  \"runtime_probe_succeeded\": "
         << (report.runtime_probe_succeeded ? "true" : "false") << ",\n"
         << "  \"missing_trace_sources\": "
         << RenderJsonArray(report.missing_trace_sources) << ",\n"
         << "  \"failing_subsystems\": "
         << RenderJsonArray(report.failing_subsystems) << ",\n"
         << "  \"selected_actions\": "
         << RenderJsonArray(report.selected_actions) << ",\n"
         << "  \"unresolved_classes\": "
         << RenderJsonArray(report.unresolved_classes) << ",\n"
         << "  \"trace_sources\": [\n";
  for (std::size_t index = 0; index < report.trace_sources.size(); ++index) {
    const auto& source = report.trace_sources[index];
    if (index != 0) {
      output << ",\n";
    }
    output << "    {"
           << "\"source_name\": \"" << EscapeJson(source.source_name)
           << "\", "
           << "\"trace_path\": \"" << EscapeJson(source.trace_path) << "\", "
           << "\"present\": " << (source.present ? "true" : "false") << ", "
           << "\"events_read\": " << source.events_read << ", "
           << "\"first_event_type\": \""
           << EscapeJson(source.first_event_type) << "\", "
           << "\"last_event_type\": \""
           << EscapeJson(source.last_event_type) << "\", "
           << "\"source_fingerprint\": \""
           << EscapeJson(source.source_fingerprint) << "\", "
           << "\"failure_reason\": \""
           << EscapeJson(source.failure_reason) << "\""
           << "}";
  }
  output << "\n  ]\n"
         << "}\n";
  return output.str();
}

}  // namespace wfa
