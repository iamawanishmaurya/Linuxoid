#ifndef WFA_WAYLAND_SURFACE_FIXTURE_HPP
#define WFA_WAYLAND_SURFACE_FIXTURE_HPP

#include "wfa/native_window_surface.hpp"

#include <string>

namespace wfa {

struct WaylandSurfaceFixtureReport {
  bool build_support_present = false;
  bool wayland_available = false;
  bool surface_created = false;
  int width = 0;
  int height = 0;
  std::string artifact_root;
  std::string surface_metadata_path;
  std::string exit_reason;
};

bool WaylandClientSupportCompiled();
WaylandSurfaceFixtureReport RunWaylandSurfaceFixture(
    const std::string& artifact_root, const NativeWindowMetadata& metadata);
std::string RenderWaylandSurfaceFixtureJson(
    const WaylandSurfaceFixtureReport& report);

}  // namespace wfa

#endif  // WFA_WAYLAND_SURFACE_FIXTURE_HPP
