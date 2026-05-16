#ifndef WFA_NATIVE_INPUT_QUEUE_FIXTURE_HPP
#define WFA_NATIVE_INPUT_QUEUE_FIXTURE_HPP

#include "wfa/native_window_surface.hpp"

#include <cstddef>
#include <string>

namespace wfa {

struct NativeInputQueueFixtureReport {
  bool input_queue_ready = false;
  bool focus_owned = false;
  std::string focus_owner;
  int width = 0;
  int height = 0;
  int format = kNativeWindowFormatRgba8888;
  int stride = 0;
  std::size_t pointer_events_injected = 0;
  std::size_t key_events_injected = 0;
  std::string artifact_root;
  std::string metadata_path;
  std::string event_log_path;
  std::string backing_mode;
  std::string exit_reason;
};

NativeInputQueueFixtureReport RunNativeInputQueueFixture(
    const std::string& session_root, const NativeWindowMetadata& metadata);
std::string RenderNativeInputQueueFixtureJson(
    const NativeInputQueueFixtureReport& report);

}  // namespace wfa

#endif  // WFA_NATIVE_INPUT_QUEUE_FIXTURE_HPP
