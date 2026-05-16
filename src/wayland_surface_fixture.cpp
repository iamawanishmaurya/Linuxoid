#include "wfa/wayland_surface_fixture.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef WFA_HAVE_WAYLAND_CLIENT
#include <wayland-client.h>

#include <algorithm>
#include <cstring>
#endif

namespace wfa {

namespace fs = std::filesystem;

namespace {

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

bool MetadataIsRenderable(const NativeWindowMetadata& metadata) {
  return metadata.width > 0 && metadata.height > 0 &&
         metadata.format == kNativeWindowFormatRgba8888 &&
         metadata.stride >= metadata.width;
}

#ifdef WFA_HAVE_WAYLAND_CLIENT
struct RegistryState {
  wl_compositor* compositor = nullptr;
};

void HandleRegistryGlobal(void* data, wl_registry* registry, uint32_t name,
                          const char* interface, uint32_t version) {
  auto* state = static_cast<RegistryState*>(data);
  if (std::strcmp(interface, wl_compositor_interface.name) != 0) {
    return;
  }

  const uint32_t bind_version = std::min(version, 4u);
  state->compositor = static_cast<wl_compositor*>(
      wl_registry_bind(registry, name, &wl_compositor_interface, bind_version));
}

void HandleRegistryGlobalRemove(void*, wl_registry*, uint32_t) {}

constexpr wl_registry_listener kRegistryListener = {
    .global = HandleRegistryGlobal,
    .global_remove = HandleRegistryGlobalRemove,
};
#endif

}  // namespace

bool WaylandClientSupportCompiled() {
#ifdef WFA_HAVE_WAYLAND_CLIENT
  return true;
#else
  return false;
#endif
}

std::string RenderWaylandSurfaceFixtureJson(
    const WaylandSurfaceFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"build_support_present\": "
         << (report.build_support_present ? "true" : "false") << ",\n"
         << "  \"wayland_available\": "
         << (report.wayland_available ? "true" : "false") << ",\n"
         << "  \"surface_created\": "
         << (report.surface_created ? "true" : "false") << ",\n"
         << "  \"width\": " << report.width << ",\n"
         << "  \"height\": " << report.height << ",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"surface_metadata_path\": \""
         << EscapeJson(report.surface_metadata_path) << "\",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\"\n"
         << "}\n";
  return output.str();
}

WaylandSurfaceFixtureReport RunWaylandSurfaceFixture(
    const std::string& artifact_root, const NativeWindowMetadata& metadata) {
  WaylandSurfaceFixtureReport report;
  report.build_support_present = WaylandClientSupportCompiled();
  report.width = metadata.width;
  report.height = metadata.height;
  report.artifact_root = artifact_root;
  report.surface_metadata_path =
      (fs::path(artifact_root) / "surface-metadata.json").string();

  fs::create_directories(artifact_root);

  if (!MetadataIsRenderable(metadata)) {
    report.exit_reason = "invalid_surface_metadata";
    WriteTextFile(report.surface_metadata_path,
                  RenderWaylandSurfaceFixtureJson(report));
    return report;
  }

#ifndef WFA_HAVE_WAYLAND_CLIENT
  report.exit_reason = "wayland_client_unavailable";
  WriteTextFile(report.surface_metadata_path,
                RenderWaylandSurfaceFixtureJson(report));
  return report;
#else
  wl_display* display = wl_display_connect(nullptr);
  if (display == nullptr) {
    report.exit_reason = "wayland_display_unavailable";
    WriteTextFile(report.surface_metadata_path,
                  RenderWaylandSurfaceFixtureJson(report));
    return report;
  }

  wl_registry* registry = wl_display_get_registry(display);
  if (registry == nullptr) {
    report.exit_reason = "wayland_registry_unavailable";
    wl_display_disconnect(display);
    WriteTextFile(report.surface_metadata_path,
                  RenderWaylandSurfaceFixtureJson(report));
    return report;
  }

  RegistryState state;
  wl_registry_add_listener(registry, &kRegistryListener, &state);
  wl_display_roundtrip(display);

  if (state.compositor == nullptr) {
    report.exit_reason = "wayland_compositor_unavailable";
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    WriteTextFile(report.surface_metadata_path,
                  RenderWaylandSurfaceFixtureJson(report));
    return report;
  }

  wl_surface* surface = wl_compositor_create_surface(state.compositor);
  if (surface == nullptr) {
    report.exit_reason = "wayland_surface_creation_failed";
    wl_compositor_destroy(state.compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    WriteTextFile(report.surface_metadata_path,
                  RenderWaylandSurfaceFixtureJson(report));
    return report;
  }

  wl_surface_commit(surface);
  wl_display_roundtrip(display);
  wl_display_flush(display);

  report.wayland_available = true;
  report.surface_created = true;
  report.exit_reason = "wayland_surface_created";

  wl_surface_destroy(surface);
  wl_compositor_destroy(state.compositor);
  wl_registry_destroy(registry);
  wl_display_disconnect(display);

  WriteTextFile(report.surface_metadata_path,
                RenderWaylandSurfaceFixtureJson(report));
  return report;
#endif
}

}  // namespace wfa
