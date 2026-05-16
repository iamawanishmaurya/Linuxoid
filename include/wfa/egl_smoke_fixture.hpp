#ifndef WFA_EGL_SMOKE_FIXTURE_HPP
#define WFA_EGL_SMOKE_FIXTURE_HPP

#include "wfa/native_window_surface.hpp"

#include <string>

namespace wfa {

struct EglSmokeFixtureReport {
  bool build_support_present = false;
  bool egl_available = false;
  bool display_initialized = false;
  bool config_chosen = false;
  bool context_created = false;
  bool pbuffer_created = false;
  int width = 0;
  int height = 0;
  std::string artifact_root;
  std::string egl_metadata_path;
  std::string exit_reason;
};

bool EglSupportCompiled();
EglSmokeFixtureReport RunEglSmokeFixture(
    const std::string& artifact_root, const NativeWindowMetadata& metadata);
std::string RenderEglSmokeFixtureJson(const EglSmokeFixtureReport& report);

}  // namespace wfa

#endif  // WFA_EGL_SMOKE_FIXTURE_HPP
