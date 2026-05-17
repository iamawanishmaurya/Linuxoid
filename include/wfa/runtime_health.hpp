#ifndef WFA_RUNTIME_HEALTH_HPP
#define WFA_RUNTIME_HEALTH_HPP

#include "wfa/native_lifecycle.hpp"

#include <string>
#include <vector>

namespace wfa {

struct RuntimeHealthRecord {
  std::string subsystem_name;
  std::string state;
  bool ready = false;
  std::string artifact_path;
  std::string failure_reason;
  std::string evidence;
  std::string selected_recovery_action;
  std::string recovery_reason;
};

struct RuntimeRecoveryAction {
  std::string action_id;
  std::string subsystem_name;
  std::string action_name;
  std::string action_state;
  std::string action_reason;
  int action_rank = 0;
  int retry_budget = 0;
  std::string recovery_scope;
  std::string artifact_path;
  std::string replay_trace_path;
};

struct RuntimeRecoveryScenarioContract {
  std::string scenario_name;
  std::string subsystem_name;
  std::string action_name;
  int action_rank = 0;
  int retry_budget = 0;
  std::string recovery_scope;
  std::string action_reason;
};

struct RuntimeHealthReport {
  std::string package_name;
  std::string install_id;
  std::string bootstrap_manifest_path;
  std::string session_root;
  std::string artifact_root;
  std::string health_json_path;
  std::string trace_jsonl_path;
  std::string replay_json_path;
  std::string recovery_plan_path;
  std::string recovery_actions_jsonl_path;
  std::string scenario_name;
  std::string overall_state;
  std::string exit_reason;
  bool self_healing_ready = false;
  bool overall_ready = false;
  bool dependency_blocked = false;
  bool core_subsystems_ready = false;
  int core_subsystem_count = 0;
  int core_ready_subsystem_count = 0;
  int failing_subsystem_count = 0;
  int recovery_actions_selected = 0;
  int canonical_recovery_scenario_count = 0;
  std::vector<std::string> core_subsystems;
  std::vector<RuntimeHealthRecord> core_records;
  std::vector<std::string> failing_subsystems;
  std::vector<RuntimeHealthRecord> records;
  std::vector<RuntimeRecoveryAction> recovery_actions;
  std::vector<RuntimeRecoveryScenarioContract> canonical_recovery_scenarios;
};

struct RuntimeHealthReplayReport {
  std::string trace_jsonl_path;
  std::string overall_state;
  std::string exit_reason;
  int events_read = 0;
  int subsystems_observed = 0;
  int recovery_actions_selected = 0;
  std::vector<std::string> failing_subsystems;
  std::vector<std::string> selected_actions;
};

struct RuntimeDiagnosticTraceSource {
  std::string source_name;
  std::string trace_path;
  bool present = false;
  int events_read = 0;
  std::string first_event_type;
  std::string last_event_type;
  std::string source_fingerprint;
  std::string failure_reason;
};

struct RuntimeDiagnosticReplayReport {
  std::string package_name;
  std::string install_id;
  std::string bootstrap_manifest_path;
  std::string session_root;
  std::string artifact_root;
  std::string trace_index_json_path;
  std::string merged_trace_jsonl_path;
  std::string result_json_path;
  std::string scenario_name;
  bool replay_ready = false;
  std::string overall_state;
  std::string exit_reason;
  int total_events_read = 0;
  int trace_sources_found = 0;
  bool runtime_probe_attempted = false;
  bool runtime_probe_succeeded = false;
  std::vector<RuntimeDiagnosticTraceSource> trace_sources;
  std::vector<std::string> missing_trace_sources;
  std::vector<std::string> failing_subsystems;
  std::vector<std::string> selected_actions;
  std::vector<std::string> unresolved_classes;
};

RuntimeHealthReport RunRuntimeHealthFixture(
    const std::string& bootstrap_manifest_path,
    const std::string& scenario_name = "baseline");
std::string RenderRuntimeHealthReportJson(const RuntimeHealthReport& report);
std::string RenderRuntimeRecoveryPlanJson(const RuntimeHealthReport& report);

RuntimeHealthReplayReport ReplayRuntimeHealthTrace(
    const std::string& trace_jsonl_path);
std::string RenderRuntimeHealthReplayJson(
    const RuntimeHealthReplayReport& report);

RuntimeDiagnosticReplayReport ReplayRuntimeDiagnosticBundle(
    const std::string& bootstrap_manifest_path,
    const std::string& scenario_name = "replay_only");
std::string RenderRuntimeDiagnosticReplayJson(
    const RuntimeDiagnosticReplayReport& report);

}  // namespace wfa

#endif  // WFA_RUNTIME_HEALTH_HPP
