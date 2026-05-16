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
  std::string artifact_path;
  std::string replay_trace_path;
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
  std::vector<RuntimeHealthRecord> records;
  std::vector<RuntimeRecoveryAction> recovery_actions;
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

RuntimeHealthReport RunRuntimeHealthFixture(
    const std::string& bootstrap_manifest_path,
    const std::string& scenario_name = "baseline");
std::string RenderRuntimeHealthReportJson(const RuntimeHealthReport& report);
std::string RenderRuntimeRecoveryPlanJson(const RuntimeHealthReport& report);

RuntimeHealthReplayReport ReplayRuntimeHealthTrace(
    const std::string& trace_jsonl_path);
std::string RenderRuntimeHealthReplayJson(
    const RuntimeHealthReplayReport& report);

}  // namespace wfa

#endif  // WFA_RUNTIME_HEALTH_HPP
