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

ANativeWindowStub* AsStub(ANativeWindow* window) {
  return reinterpret_cast<ANativeWindowStub*>(window);
}

const ANativeWindowStub* AsStub(const ANativeWindow* window) {
  return reinterpret_cast<const ANativeWindowStub*>(window);
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

}  // namespace wfa
