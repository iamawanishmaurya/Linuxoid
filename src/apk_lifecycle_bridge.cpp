#include "wfa/apk_lifecycle_bridge.hpp"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

struct QueuedLooperEvent {
  std::string kind;
  std::string state;
  NativeApkInputEvent input;
  std::uint64_t sequence_id = 0;
};

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

void AppendUnique(std::vector<std::string>* values, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(value);
  }
}

void WriteTextFile(const fs::path& path, const std::string& contents) {
  fs::create_directories(path.parent_path());
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

bool IsTouchAction(const std::string& action) {
  return action == "touch_down" || action == "touch_move" ||
         action == "touch_up";
}

bool IsKeyAction(const std::string& action) {
  return action == "key_down" || action == "key_up";
}

std::string RenderLifecycleMetadataJson(const NativeApkLifecycleBridgeContext& context,
                                        const NativeApkLifecycleProof& proof) {
  std::ostringstream output;
  output << "{\n"
         << "  \"session_id\": \"" << EscapeJson(proof.session_id) << "\",\n"
         << "  \"package_name\": \"" << EscapeJson(context.package_name)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(context.apk_path) << "\",\n"
         << "  \"staged_dir\": \"" << EscapeJson(context.staged_dir) << "\",\n"
         << "  \"selected_abi\": \"" << EscapeJson(context.selected_abi)
         << "\",\n"
         << "  \"selected_library_path\": \""
         << EscapeJson(context.selected_library_path) << "\",\n"
         << "  \"launch_status\": \"" << EscapeJson(context.launch_status)
         << "\",\n"
         << "  \"asset_health\": \"" << EscapeJson(context.asset_health)
         << "\",\n"
         << "  \"resource_health\": \"" << EscapeJson(context.resource_health)
         << "\",\n"
         << "  \"surface_health\": \"" << EscapeJson(context.surface_health)
         << "\",\n"
         << "  \"surface_state\": \"" << EscapeJson(context.surface_state)
         << "\",\n"
         << "  \"ready\": " << (proof.ready ? "true" : "false") << ",\n"
         << "  \"states_visited\": " << RenderJsonArray(proof.states_visited)
         << ",\n"
         << "  \"current_state\": \"" << EscapeJson(proof.current_state)
         << "\",\n"
         << "  \"events_dispatched\": " << proof.events_dispatched << ",\n"
         << "  \"errors\": " << RenderJsonArray(proof.errors) << "\n"
         << "}\n";
  return output.str();
}

std::string RenderLooperMetadataJson(const NativeApkLooperProof& proof) {
  std::ostringstream output;
  output << "{\n"
         << "  \"ready\": " << (proof.ready ? "true" : "false") << ",\n"
         << "  \"posted_events\": " << proof.posted_events << ",\n"
         << "  \"dispatched_events\": " << proof.dispatched_events << ",\n"
         << "  \"shutdown_clean\": "
         << (proof.shutdown_clean ? "true" : "false") << ",\n"
         << "  \"errors\": " << RenderJsonArray(proof.errors) << "\n"
         << "}\n";
  return output.str();
}

std::string RenderInputQueueMetadataJson(const NativeApkInputQueueProof& proof) {
  std::ostringstream output;
  output << "{\n"
         << "  \"ready\": " << (proof.ready ? "true" : "false") << ",\n"
         << "  \"queued_events\": " << proof.queued_events << ",\n"
         << "  \"dispatched_events\": " << proof.dispatched_events << ",\n"
         << "  \"handled_events\": " << proof.handled_events << ",\n"
         << "  \"rejected_events\": " << proof.rejected_events << ",\n"
         << "  \"handled_actions\": " << RenderJsonArray(proof.handled_actions)
         << ",\n"
         << "  \"errors\": " << RenderJsonArray(proof.errors) << "\n"
         << "}\n";
  return output.str();
}

}  // namespace

NativeApkLifecycleBridgeSession::NativeApkLifecycleBridgeSession(
    NativeApkLifecycleBridgeContext context)
    : context_(std::move(context)) {}

const NativeApkLifecycleBridgeContext&
NativeApkLifecycleBridgeSession::context() const {
  return context_;
}

NativeApkInputEnqueueResult NativeApkLifecycleBridgeSession::EnqueueInputEvent(
    const NativeApkInputEvent& event) {
  NativeApkInputEnqueueResult result;
  result.event = event;
  if (event.action.empty()) {
    result.rejected = true;
    AppendUnique(&result.errors, "input_action_missing");
  } else if (IsTouchAction(event.action)) {
    if (event.x < 0 || event.y < 0) {
      result.rejected = true;
      AppendUnique(&result.errors, "input_coordinates_negative");
    }
    if (event.x >= context_.width || event.y >= context_.height) {
      result.rejected = true;
      AppendUnique(&result.errors, "input_coordinates_out_of_bounds");
    }
    if (event.key_code != 0) {
      result.rejected = true;
      AppendUnique(&result.errors, "touch_event_key_code_forbidden");
    }
  } else if (IsKeyAction(event.action)) {
    if (event.key_code <= 0) {
      result.rejected = true;
      AppendUnique(&result.errors, "input_key_code_invalid");
    }
  } else {
    result.rejected = true;
    AppendUnique(&result.errors, "input_action_unsupported");
  }

  if (result.rejected) {
    ++rejected_input_events_;
    for (const auto& error : result.errors) {
      AppendUnique(&session_errors_, error);
    }
    return result;
  }

  result.accepted = true;
  result.event.sequence_id =
      event.sequence_id == 0 ? next_sequence_id_++ : event.sequence_id;
  queued_input_events_.push_back(result.event);
  return result;
}

NativeApkLifecycleBridgeReport
NativeApkLifecycleBridgeSession::RunDeterministicProof() {
  NativeApkLifecycleBridgeReport report;
  report.lifecycle.session_id = context_.session_id.empty()
                                    ? context_.package_name + ":lifecycle"
                                    : context_.session_id;

  const fs::path artifact_root =
      context_.artifact_root.empty()
          ? (fs::path(context_.staged_dir) / "lifecycle-proof")
          : fs::path(context_.artifact_root);
  report.lifecycle.metadata_path =
      (artifact_root / "lifecycle-session.json").string();
  report.lifecycle.event_log_path =
      (artifact_root / "lifecycle-events.jsonl").string();
  report.looper.metadata_path = (artifact_root / "looper-metadata.json").string();
  report.looper.event_log_path = (artifact_root / "looper-events.jsonl").string();
  report.input_queue.metadata_path =
      (artifact_root / "input-queue-metadata.json").string();
  report.input_queue.event_log_path =
      (artifact_root / "input-events.jsonl").string();

  if (context_.package_name.empty() || context_.apk_path.empty() ||
      context_.staged_dir.empty()) {
    report.exit_reason = "lifecycle_session_context_missing";
    AppendUnique(&report.errors, report.exit_reason);
    report.lifecycle.errors = report.errors;
    report.looper.errors = report.errors;
    report.input_queue.errors = report.errors;
    WriteTextFile(report.lifecycle.metadata_path,
                  RenderLifecycleMetadataJson(context_, report.lifecycle));
    WriteTextFile(report.looper.metadata_path,
                  RenderLooperMetadataJson(report.looper));
    WriteTextFile(report.input_queue.metadata_path,
                  RenderInputQueueMetadataJson(report.input_queue));
    WriteTextFile(report.lifecycle.event_log_path, "");
    WriteTextFile(report.looper.event_log_path, "");
    WriteTextFile(report.input_queue.event_log_path, "");
    return report;
  }

  const std::vector<std::string> lifecycle_states = {
      "created", "started", "resumed", "paused", "stopped", "destroyed"};

  if (queued_input_events_.empty()) {
    (void)EnqueueInputEvent(
        {.action = "touch_down", .x = context_.width / 4, .y = context_.height / 4});
    (void)EnqueueInputEvent(
        {.action = "touch_move", .x = context_.width / 2, .y = context_.height / 2});
    (void)EnqueueInputEvent(
        {.action = "touch_up", .x = context_.width / 2, .y = context_.height / 2});
    (void)EnqueueInputEvent({.action = "key_down", .key_code = 29});
    (void)EnqueueInputEvent({.action = "key_up", .key_code = 29});
  }

  std::deque<QueuedLooperEvent> queue;
  for (const auto& state : lifecycle_states) {
    queue.push_back(
        {.kind = "lifecycle", .state = state, .sequence_id = next_sequence_id_++});
  }
  for (const auto& input_event : queued_input_events_) {
    queue.push_back(
        {.kind = "input", .input = input_event, .sequence_id = input_event.sequence_id});
  }

  report.looper.posted_events = queue.size();
  report.input_queue.queued_events = queued_input_events_.size();
  report.input_queue.rejected_events = rejected_input_events_;

  std::ostringstream lifecycle_log;
  std::ostringstream looper_log;
  std::ostringstream input_log;

  while (!queue.empty()) {
    const QueuedLooperEvent event = queue.front();
    queue.pop_front();
    ++report.looper.dispatched_events;
    looper_log << "{\"sequence_id\": " << event.sequence_id << ", "
               << "\"kind\": \"" << EscapeJson(event.kind) << "\"}\n";

    if (event.kind == "lifecycle") {
      report.lifecycle.states_visited.push_back(event.state);
      report.lifecycle.state_sequence_ids.push_back(event.sequence_id);
      report.lifecycle.current_state = event.state;
      ++report.lifecycle.events_dispatched;
      lifecycle_log << "{\"sequence_id\": " << event.sequence_id << ", "
                    << "\"state\": \"" << EscapeJson(event.state) << "\"}\n";
      continue;
    }

    NativeApkInputEvent dispatched = event.input;
    dispatched.handled = true;
    ++report.input_queue.dispatched_events;
    ++report.input_queue.handled_events;
    report.input_queue.handled_actions.push_back(dispatched.action);
    input_log << "{\"sequence_id\": " << dispatched.sequence_id << ", "
              << "\"action\": \"" << EscapeJson(dispatched.action) << "\", "
              << "\"x\": " << dispatched.x << ", "
              << "\"y\": " << dispatched.y << ", "
              << "\"key_code\": " << dispatched.key_code << ", "
              << "\"handled\": true}\n";
  }

  report.looper.shutdown_clean = true;
  report.lifecycle.ready =
      report.lifecycle.states_visited == lifecycle_states &&
      report.lifecycle.current_state == "destroyed";
  report.looper.ready =
      report.looper.posted_events == report.looper.dispatched_events &&
      report.looper.shutdown_clean;
  report.input_queue.ready =
      report.input_queue.queued_events == report.input_queue.dispatched_events &&
      report.input_queue.queued_events == report.input_queue.handled_events &&
      report.input_queue.rejected_events == 0;

  report.lifecycle.errors = session_errors_;
  report.looper.errors = session_errors_;
  report.input_queue.errors = session_errors_;

  report.ready =
      report.lifecycle.ready && report.looper.ready && report.input_queue.ready;
  report.exit_reason =
      report.ready ? "lifecycle_input_proof_succeeded"
                   : "lifecycle_input_proof_failed";
  if (!report.ready) {
    AppendUnique(&report.errors, report.exit_reason);
  }

  WriteTextFile(report.lifecycle.metadata_path,
                RenderLifecycleMetadataJson(context_, report.lifecycle));
  WriteTextFile(report.looper.metadata_path,
                RenderLooperMetadataJson(report.looper));
  WriteTextFile(report.input_queue.metadata_path,
                RenderInputQueueMetadataJson(report.input_queue));
  WriteTextFile(report.lifecycle.event_log_path, lifecycle_log.str());
  WriteTextFile(report.looper.event_log_path, looper_log.str());
  WriteTextFile(report.input_queue.event_log_path, input_log.str());
  return report;
}

}  // namespace wfa
