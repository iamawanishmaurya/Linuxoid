#include "wfa/native_window_surface.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

struct ANativeWindowStub {
  NativeWindowSurfaceState state;
  std::vector<std::uint32_t> pixels;
};

struct CallbackJournalState {
  std::string journal_path;
  std::vector<NativeWindowCallbackEvent>* events = nullptr;
};

namespace {

constexpr const char* kHeadlessBackendName = "wayland-egl-headless-fixture";

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

bool MetadataIsValid(const NativeWindowMetadata& metadata) {
  return metadata.width > 0 && metadata.height > 0 &&
         metadata.format == kNativeWindowFormatRgba8888 &&
         metadata.stride >= metadata.width;
}

std::string HexPixel(std::uint32_t pixel) {
  std::ostringstream output;
  output << "0x" << std::hex << std::nouppercase << std::setfill('0')
         << std::setw(8) << pixel;
  return output.str();
}

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

std::string RenderCallbackEventJson(const NativeWindowCallbackEvent& event) {
  std::ostringstream output;
  output << "{"
         << "\"event_name\": \"" << EscapeJson(event.event_name) << "\", "
         << "\"window_present\": "
         << (event.window_present ? "true" : "false") << ", "
         << "\"width\": " << event.metadata.width << ", "
         << "\"height\": " << event.metadata.height << ", "
         << "\"format\": " << event.metadata.format << ", "
         << "\"stride\": " << event.metadata.stride
         << "}";
  return output.str();
}

void WriteCallbackJournal(
    const std::string& journal_path,
    const std::vector<NativeWindowCallbackEvent>& events) {
  std::ostringstream output;
  for (const auto& event : events) {
    output << RenderCallbackEventJson(event) << "\n";
  }
  WriteTextFile(journal_path, output.str());
}

ANativeWindowStub* AsStub(ANativeWindow* window) {
  return reinterpret_cast<ANativeWindowStub*>(window);
}

const ANativeWindowStub* AsStub(const ANativeWindow* window) {
  return reinterpret_cast<const ANativeWindowStub*>(window);
}

NativeWindowCallbackEvent BuildCallbackEvent(const std::string& event_name,
                                             ANativeWindow* window) {
  NativeWindowCallbackEvent event;
  event.event_name = event_name;
  event.window_present = window != nullptr;
  if (window != nullptr) {
    event.metadata = InspectNativeWindow(window);
  }
  return event;
}

void RecordCallbackEvent(ANativeActivity* activity, ANativeWindow* window,
                         const std::string& event_name) {
  if (activity == nullptr || activity->instance == nullptr) {
    return;
  }

  auto* journal = reinterpret_cast<CallbackJournalState*>(activity->instance);
  if (journal->events == nullptr) {
    return;
  }

  journal->events->push_back(BuildCallbackEvent(event_name, window));
  WriteCallbackJournal(journal->journal_path, *journal->events);
}

void OnNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window) {
  RecordCallbackEvent(activity, window, "window_created");
}

void OnNativeWindowChanged(ANativeActivity* activity, ANativeWindow* window) {
  RecordCallbackEvent(activity, window, "window_changed");
}

void OnNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window) {
  RecordCallbackEvent(activity, window, "window_destroyed");
}

}  // namespace

ANativeWindow* CreateHeadlessNativeWindowSurface(
    const NativeWindowMetadata& metadata, const std::string& session_root) {
  auto* window = new ANativeWindowStub;
  window->state.backend_name = kHeadlessBackendName;
  window->state.session_root = session_root;
  window->state.metadata = metadata;
  window->state.marker_path =
      (fs::path(session_root) / "first-pixel-marker.txt").string();

  fs::create_directories(session_root);

  if (!MetadataIsValid(metadata)) {
    window->state.failure_reason = "invalid_surface_metadata";
    return window;
  }

  window->pixels.assign(static_cast<std::size_t>(metadata.width) *
                            static_cast<std::size_t>(metadata.height),
                        0u);
  window->state.host_surface_created = true;
  window->state.egl_display_ready = true;
  window->state.egl_surface_ready = true;
  window->state.lifecycle_ready = true;
  return window;
}

NativeWindowMetadata InspectNativeWindow(const ANativeWindow* window) {
  if (window == nullptr) {
    return {};
  }
  return AsStub(window)->state.metadata;
}

bool NativeWindowLifecycleReady(const ANativeWindow* window) {
  return window != nullptr && AsStub(window)->state.lifecycle_ready &&
         AsStub(window)->state.host_surface_created &&
         AsStub(window)->state.egl_display_ready &&
         AsStub(window)->state.egl_surface_ready;
}

void DestroyHeadlessNativeWindowSurface(ANativeWindow* window) {
  delete AsStub(window);
}

void DispatchNativeWindowCreated(ANativeActivity* activity,
                                 ANativeWindow* window) {
  if (activity == nullptr || activity->callbacks == nullptr ||
      activity->callbacks->onNativeWindowCreated == nullptr) {
    return;
  }
  activity->callbacks->onNativeWindowCreated(activity, window);
}

void DispatchNativeWindowChanged(ANativeActivity* activity,
                                 ANativeWindow* window) {
  if (activity == nullptr || activity->callbacks == nullptr ||
      activity->callbacks->onNativeWindowResized == nullptr) {
    return;
  }
  activity->callbacks->onNativeWindowResized(activity, window);
}

void DispatchNativeWindowDestroyed(ANativeActivity* activity,
                                   ANativeWindow* window) {
  if (activity != nullptr && activity->callbacks != nullptr &&
      activity->callbacks->onNativeWindowDestroyed != nullptr) {
    activity->callbacks->onNativeWindowDestroyed(activity, window);
  }
  if (window != nullptr) {
    AsStub(window)->state.activity_window_attached = false;
  }
  if (activity != nullptr && activity->window == window) {
    activity->window = nullptr;
  }
}

FirstPixelFixtureReport RunHeadlessFirstPixelFixture(
    const std::string& session_root, const NativeWindowMetadata& metadata,
    std::uint32_t first_pixel_value) {
  FirstPixelFixtureReport report;
  ANativeWindow* window =
      CreateHeadlessNativeWindowSurface(metadata, session_root);
  report.surface = AsStub(window)->state;
  report.surface_ready = NativeWindowLifecycleReady(window);

  if (!report.surface_ready) {
    report.exit_reason = report.surface.failure_reason.empty()
                             ? "surface_not_ready"
                             : report.surface.failure_reason;
    DestroyHeadlessNativeWindowSurface(window);
    return report;
  }

  ANativeActivity activity{};
  activity.window = window;
  auto* stub = AsStub(window);
  stub->state.activity_window_attached = activity.window != nullptr;
  stub->pixels.front() = first_pixel_value;
  stub->state.first_pixel_value = first_pixel_value;
  stub->state.first_pixel_observed = true;

  std::ostringstream marker;
  marker << "backend=" << stub->state.backend_name << "\n";
  marker << "width=" << stub->state.metadata.width << "\n";
  marker << "height=" << stub->state.metadata.height << "\n";
  marker << "format=" << stub->state.metadata.format << "\n";
  marker << "stride=" << stub->state.metadata.stride << "\n";
  marker << "first_pixel=" << HexPixel(first_pixel_value) << "\n";
  WriteTextFile(stub->state.marker_path, marker.str());

  report.surface = stub->state;
  report.surface_ready = true;
  report.render_ready = true;
  report.exit_reason = "first_pixel_marker_written";
  DestroyHeadlessNativeWindowSurface(window);
  return report;
}

std::string RenderFirstPixelFixtureJson(
    const FirstPixelFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"surface_ready\": "
         << (report.surface_ready ? "true" : "false") << ",\n"
         << "  \"render_ready\": "
         << (report.render_ready ? "true" : "false") << ",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\",\n"
         << "  \"backend_name\": \""
         << EscapeJson(report.surface.backend_name) << "\",\n"
         << "  \"session_root\": \""
         << EscapeJson(report.surface.session_root) << "\",\n"
         << "  \"marker_path\": \""
         << EscapeJson(report.surface.marker_path) << "\",\n"
         << "  \"width\": " << report.surface.metadata.width << ",\n"
         << "  \"height\": " << report.surface.metadata.height << ",\n"
         << "  \"format\": " << report.surface.metadata.format << ",\n"
         << "  \"stride\": " << report.surface.metadata.stride << ",\n"
         << "  \"host_surface_created\": "
         << (report.surface.host_surface_created ? "true" : "false") << ",\n"
         << "  \"egl_display_ready\": "
         << (report.surface.egl_display_ready ? "true" : "false") << ",\n"
         << "  \"egl_surface_ready\": "
         << (report.surface.egl_surface_ready ? "true" : "false") << ",\n"
         << "  \"lifecycle_ready\": "
         << (report.surface.lifecycle_ready ? "true" : "false") << ",\n"
         << "  \"activity_window_attached\": "
         << (report.surface.activity_window_attached ? "true" : "false")
         << ",\n"
         << "  \"first_pixel_observed\": "
         << (report.surface.first_pixel_observed ? "true" : "false") << ",\n"
         << "  \"first_pixel_value\": \""
         << HexPixel(report.surface.first_pixel_value) << "\"\n"
         << "}\n";
  return output.str();
}

NativeWindowCallbackFixtureReport RunHeadlessNativeWindowCallbackFixture(
    const std::string& session_root, const NativeWindowMetadata& metadata) {
  NativeWindowCallbackFixtureReport report;
  report.callback_journal_path =
      (fs::path(session_root) / "native-window-callbacks.jsonl").string();

  ANativeWindow* window =
      CreateHeadlessNativeWindowSurface(metadata, session_root);
  report.surface = AsStub(window)->state;
  report.surface_ready = NativeWindowLifecycleReady(window);
  if (!report.surface_ready) {
    report.exit_reason = report.surface.failure_reason.empty()
                             ? "surface_not_ready"
                             : report.surface.failure_reason;
    DestroyHeadlessNativeWindowSurface(window);
    return report;
  }

  std::vector<NativeWindowCallbackEvent> events;
  CallbackJournalState journal{
      .journal_path = report.callback_journal_path,
      .events = &events,
  };
  ANativeActivityCallbacks callbacks{
      .onNativeWindowCreated = OnNativeWindowCreated,
      .onNativeWindowResized = OnNativeWindowChanged,
      .onNativeWindowDestroyed = OnNativeWindowDestroyed,
  };
  ANativeActivity activity{};
  activity.instance = &journal;
  activity.callbacks = &callbacks;
  activity.window = window;

  auto* stub = AsStub(window);
  stub->state.activity_window_attached = true;

  DispatchNativeWindowCreated(&activity, window);
  DispatchNativeWindowChanged(&activity, window);
  DispatchNativeWindowDestroyed(&activity, window);

  report.surface = stub->state;
  report.events = events;
  report.callbacks_ready = report.events.size() == 3 &&
                           report.events[0].event_name == "window_created" &&
                           report.events[1].event_name == "window_changed" &&
                           report.events[2].event_name == "window_destroyed";
  report.exit_reason = report.callbacks_ready
                           ? "native_window_callbacks_recorded"
                           : "callback_dispatch_incomplete";

  DestroyHeadlessNativeWindowSurface(window);
  return report;
}

std::string RenderNativeWindowCallbackFixtureJson(
    const NativeWindowCallbackFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"surface_ready\": "
         << (report.surface_ready ? "true" : "false") << ",\n"
         << "  \"callbacks_ready\": "
         << (report.callbacks_ready ? "true" : "false") << ",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\",\n"
         << "  \"callback_journal_path\": \""
         << EscapeJson(report.callback_journal_path) << "\",\n"
         << "  \"backend_name\": \""
         << EscapeJson(report.surface.backend_name) << "\",\n"
         << "  \"activity_window_attached\": "
         << (report.surface.activity_window_attached ? "true" : "false")
         << ",\n"
         << "  \"events\": [";
  for (std::size_t index = 0; index < report.events.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    output << RenderCallbackEventJson(report.events[index]);
  }
  output << "]\n"
         << "}\n";
  return output.str();
}

}  // namespace wfa
