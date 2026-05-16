#ifndef WFA_NATIVE_WINDOW_SURFACE_HPP
#define WFA_NATIVE_WINDOW_SURFACE_HPP

#include "wfa/native_types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace wfa {

constexpr int kNativeWindowFormatRgba8888 = 1;

struct NativeWindowMetadata {
  int width = 0;
  int height = 0;
  int format = kNativeWindowFormatRgba8888;
  int stride = 0;
};

struct NativeWindowSurfaceState {
  std::string backend_name;
  std::string session_root;
  std::string marker_path;
  NativeWindowMetadata metadata;
  std::size_t geometry_updates = 0;
  bool host_surface_created = false;
  bool egl_display_ready = false;
  bool egl_surface_ready = false;
  bool lifecycle_ready = false;
  bool activity_window_attached = false;
  bool first_pixel_observed = false;
  std::uint32_t first_pixel_value = 0;
  std::string failure_reason;
};

struct FirstPixelFixtureReport {
  NativeWindowSurfaceState surface;
  bool surface_ready = false;
  bool render_ready = false;
  std::string exit_reason;
};

struct NativeWindowCallbackEvent {
  std::string event_name;
  bool window_present = false;
  NativeWindowMetadata metadata;
};

struct NativeWindowCallbackFixtureReport {
  NativeWindowSurfaceState surface;
  std::string callback_journal_path;
  std::vector<NativeWindowCallbackEvent> events;
  bool surface_ready = false;
  bool callbacks_ready = false;
  std::string exit_reason;
};

struct NativeWindowBridgeFixtureReport {
  bool native_window_bridge_ready = false;
  int width = 0;
  int height = 0;
  int format = kNativeWindowFormatRgba8888;
  int stride = 0;
  std::size_t geometry_updates = 0;
  std::string artifact_root;
  std::string metadata_path;
  std::string event_log_path;
  std::string backing_mode;
  bool wayland_surface_created = false;
  bool egl_pbuffer_created = false;
  std::string exit_reason;
};

ANativeWindow* CreateHeadlessNativeWindowSurface(
    const NativeWindowMetadata& metadata, const std::string& session_root);
NativeWindowMetadata InspectNativeWindow(const ANativeWindow* window);
bool NativeWindowLifecycleReady(const ANativeWindow* window);
bool UpdateNativeWindowBufferGeometry(ANativeWindow* window,
                                      const NativeWindowMetadata& metadata);
std::size_t GetNativeWindowGeometryUpdateCount(const ANativeWindow* window);
void DestroyHeadlessNativeWindowSurface(ANativeWindow* window);
void DispatchNativeWindowCreated(ANativeActivity* activity,
                                 ANativeWindow* window);
void DispatchNativeWindowChanged(ANativeActivity* activity,
                                 ANativeWindow* window);
void DispatchNativeWindowDestroyed(ANativeActivity* activity,
                                   ANativeWindow* window);
FirstPixelFixtureReport RunHeadlessFirstPixelFixture(
    const std::string& session_root, const NativeWindowMetadata& metadata,
    std::uint32_t first_pixel_value);
std::string RenderFirstPixelFixtureJson(
    const FirstPixelFixtureReport& report);
NativeWindowCallbackFixtureReport RunHeadlessNativeWindowCallbackFixture(
    const std::string& session_root, const NativeWindowMetadata& metadata);
std::string RenderNativeWindowCallbackFixtureJson(
    const NativeWindowCallbackFixtureReport& report);
NativeWindowBridgeFixtureReport RunNativeWindowBridgeFixture(
    const std::string& session_root, const NativeWindowMetadata& metadata);
std::string RenderNativeWindowBridgeFixtureJson(
    const NativeWindowBridgeFixtureReport& report);

}  // namespace wfa

#endif  // WFA_NATIVE_WINDOW_SURFACE_HPP
