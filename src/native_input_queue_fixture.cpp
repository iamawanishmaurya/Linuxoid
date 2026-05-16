#include "wfa/native_input_queue_fixture.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

struct NativeInputEvent {
  std::string event_name;
  std::string device_type;
  std::string action;
  int x = 0;
  int y = 0;
  int key_code = 0;
  std::string focus_owner;
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

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

std::string RenderInputEventJson(const NativeInputEvent& event) {
  std::ostringstream output;
  output << "{"
         << "\"event_name\": \"" << EscapeJson(event.event_name) << "\", "
         << "\"device_type\": \"" << EscapeJson(event.device_type) << "\", "
         << "\"action\": \"" << EscapeJson(event.action) << "\", "
         << "\"x\": " << event.x << ", "
         << "\"y\": " << event.y << ", "
         << "\"key_code\": " << event.key_code << ", "
         << "\"focus_owner\": \"" << EscapeJson(event.focus_owner) << "\""
         << "}";
  return output.str();
}

void WriteInputEventLog(const std::string& path,
                        const std::vector<NativeInputEvent>& events) {
  std::ostringstream output;
  for (const auto& event : events) {
    output << RenderInputEventJson(event) << "\n";
  }
  WriteTextFile(path, output.str());
}

int ClampCoordinate(int value, int upper_bound) {
  return std::clamp(value, 0, std::max(0, upper_bound));
}

}  // namespace

NativeInputQueueFixtureReport RunNativeInputQueueFixture(
    const std::string& session_root, const NativeWindowMetadata& metadata) {
  NativeInputQueueFixtureReport report;
  report.artifact_root = session_root;
  report.metadata_path =
      (fs::path(session_root) / "native-input-queue-metadata.json").string();
  report.event_log_path =
      (fs::path(session_root) / "native-input-events.jsonl").string();

  fs::create_directories(session_root);

  const auto bridge = RunNativeWindowBridgeFixture(
      (fs::path(session_root) / "bridge-probe").string(), metadata);
  report.width = bridge.width;
  report.height = bridge.height;
  report.format = bridge.format;
  report.stride = bridge.stride;
  report.backing_mode = bridge.backing_mode;

  if (!bridge.native_window_bridge_ready) {
    report.exit_reason = "native_input_queue_bridge_not_ready";
    WriteTextFile(report.metadata_path,
                  RenderNativeInputQueueFixtureJson(report));
    WriteTextFile(report.event_log_path, "");
    return report;
  }

  report.input_queue_ready = true;
  report.focus_owned = true;
  report.focus_owner = "linuxoid-native-window";
  report.pointer_events_injected = 3;
  report.key_events_injected = 2;

  const int pointer_down_x = ClampCoordinate(12, report.width - 1);
  const int pointer_down_y = ClampCoordinate(8, report.height - 1);
  const int pointer_move_x = ClampCoordinate(20, report.width - 1);
  const int pointer_move_y = ClampCoordinate(12, report.height - 1);

  const std::vector<NativeInputEvent> events = {
      {.event_name = "focus_acquired",
       .device_type = "focus",
       .action = "own",
       .focus_owner = report.focus_owner},
      {.event_name = "pointer_down",
       .device_type = "pointer",
       .action = "down",
       .x = pointer_down_x,
       .y = pointer_down_y,
       .focus_owner = report.focus_owner},
      {.event_name = "pointer_move",
       .device_type = "pointer",
       .action = "move",
       .x = pointer_move_x,
       .y = pointer_move_y,
       .focus_owner = report.focus_owner},
      {.event_name = "pointer_up",
       .device_type = "pointer",
       .action = "up",
       .x = pointer_move_x,
       .y = pointer_move_y,
       .focus_owner = report.focus_owner},
      {.event_name = "key_down",
       .device_type = "keyboard",
       .action = "down",
       .key_code = 29,
       .focus_owner = report.focus_owner},
      {.event_name = "key_up",
       .device_type = "keyboard",
       .action = "up",
       .key_code = 29,
       .focus_owner = report.focus_owner},
  };

  WriteInputEventLog(report.event_log_path, events);

  report.exit_reason =
      report.backing_mode == "headless_fallback"
          ? "native_input_queue_ready_headless_fallback"
          : "native_input_queue_ready_probe_only";
  WriteTextFile(report.metadata_path, RenderNativeInputQueueFixtureJson(report));
  return report;
}

std::string RenderNativeInputQueueFixtureJson(
    const NativeInputQueueFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"input_queue_ready\": "
         << (report.input_queue_ready ? "true" : "false") << ",\n"
         << "  \"focus_owned\": " << (report.focus_owned ? "true" : "false")
         << ",\n"
         << "  \"focus_owner\": \"" << EscapeJson(report.focus_owner)
         << "\",\n"
         << "  \"width\": " << report.width << ",\n"
         << "  \"height\": " << report.height << ",\n"
         << "  \"format\": " << report.format << ",\n"
         << "  \"stride\": " << report.stride << ",\n"
         << "  \"pointer_events_injected\": " << report.pointer_events_injected
         << ",\n"
         << "  \"key_events_injected\": " << report.key_events_injected
         << ",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"metadata_path\": \"" << EscapeJson(report.metadata_path)
         << "\",\n"
         << "  \"event_log_path\": \"" << EscapeJson(report.event_log_path)
         << "\",\n"
         << "  \"backing_mode\": \"" << EscapeJson(report.backing_mode)
         << "\",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\"\n"
         << "}\n";
  return output.str();
}

}  // namespace wfa
