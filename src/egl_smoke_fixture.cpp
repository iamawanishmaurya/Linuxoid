#include "wfa/egl_smoke_fixture.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef WFA_HAVE_EGL
#include <EGL/egl.h>
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

}  // namespace

bool EglSupportCompiled() {
#ifdef WFA_HAVE_EGL
  return true;
#else
  return false;
#endif
}

std::string RenderEglSmokeFixtureJson(const EglSmokeFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"build_support_present\": "
         << (report.build_support_present ? "true" : "false") << ",\n"
         << "  \"egl_available\": "
         << (report.egl_available ? "true" : "false") << ",\n"
         << "  \"display_initialized\": "
         << (report.display_initialized ? "true" : "false") << ",\n"
         << "  \"config_chosen\": "
         << (report.config_chosen ? "true" : "false") << ",\n"
         << "  \"context_created\": "
         << (report.context_created ? "true" : "false") << ",\n"
         << "  \"pbuffer_created\": "
         << (report.pbuffer_created ? "true" : "false") << ",\n"
         << "  \"width\": " << report.width << ",\n"
         << "  \"height\": " << report.height << ",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"egl_metadata_path\": \""
         << EscapeJson(report.egl_metadata_path) << "\",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\"\n"
         << "}\n";
  return output.str();
}

EglSmokeFixtureReport RunEglSmokeFixture(
    const std::string& artifact_root, const NativeWindowMetadata& metadata) {
  EglSmokeFixtureReport report;
  report.build_support_present = EglSupportCompiled();
  report.width = metadata.width;
  report.height = metadata.height;
  report.artifact_root = artifact_root;
  report.egl_metadata_path =
      (fs::path(artifact_root) / "egl-metadata.json").string();

  fs::create_directories(artifact_root);

  if (!MetadataIsRenderable(metadata)) {
    report.exit_reason = "invalid_surface_metadata";
    WriteTextFile(report.egl_metadata_path, RenderEglSmokeFixtureJson(report));
    return report;
  }

#ifndef WFA_HAVE_EGL
  report.exit_reason = "egl_unavailable";
  WriteTextFile(report.egl_metadata_path, RenderEglSmokeFixtureJson(report));
  return report;
#else
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display == EGL_NO_DISPLAY) {
    report.exit_reason = "egl_display_unavailable";
    WriteTextFile(report.egl_metadata_path, RenderEglSmokeFixtureJson(report));
    return report;
  }

  EGLint major = 0;
  EGLint minor = 0;
  if (eglInitialize(display, &major, &minor) != EGL_TRUE) {
    report.exit_reason = "egl_initialize_failed";
    WriteTextFile(report.egl_metadata_path, RenderEglSmokeFixtureJson(report));
    return report;
  }
  report.display_initialized = true;

  if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
    eglTerminate(display);
    report.exit_reason = "egl_initialize_failed";
    WriteTextFile(report.egl_metadata_path, RenderEglSmokeFixtureJson(report));
    return report;
  }

  const EGLint config_attributes[] = {
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_NONE,
  };
  EGLConfig config = nullptr;
  EGLint num_configs = 0;
  if (eglChooseConfig(display, config_attributes, &config, 1, &num_configs) !=
          EGL_TRUE ||
      num_configs < 1) {
    eglTerminate(display);
    report.exit_reason = "egl_no_config_found";
    WriteTextFile(report.egl_metadata_path, RenderEglSmokeFixtureJson(report));
    return report;
  }
  report.config_chosen = true;

  const EGLint context_attributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE,
  };
  EGLContext context =
      eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
  if (context == EGL_NO_CONTEXT) {
    eglTerminate(display);
    report.exit_reason = "egl_context_creation_failed";
    WriteTextFile(report.egl_metadata_path, RenderEglSmokeFixtureJson(report));
    return report;
  }
  report.context_created = true;

  const EGLint pbuffer_attributes[] = {
      EGL_WIDTH, metadata.width,
      EGL_HEIGHT, metadata.height,
      EGL_NONE,
  };
  EGLSurface surface =
      eglCreatePbufferSurface(display, config, pbuffer_attributes);
  if (surface == EGL_NO_SURFACE) {
    eglDestroyContext(display, context);
    eglTerminate(display);
    report.exit_reason = "egl_pbuffer_creation_failed";
    WriteTextFile(report.egl_metadata_path, RenderEglSmokeFixtureJson(report));
    return report;
  }
  report.pbuffer_created = true;

  if (eglMakeCurrent(display, surface, surface, context) != EGL_TRUE) {
    eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    eglTerminate(display);
    report.exit_reason = "egl_context_creation_failed";
    WriteTextFile(report.egl_metadata_path, RenderEglSmokeFixtureJson(report));
    return report;
  }

  report.egl_available = true;
  report.exit_reason = "egl_pbuffer_ready";

  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroySurface(display, surface);
  eglDestroyContext(display, context);
  eglTerminate(display);

  WriteTextFile(report.egl_metadata_path, RenderEglSmokeFixtureJson(report));
  return report;
#endif
}

}  // namespace wfa
