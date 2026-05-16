#include "wfa/native_lifecycle.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace wfa {

namespace fs = std::filesystem;

namespace {

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

std::string ReadFile(const fs::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to read file: " + path.string());
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

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

std::string ExtractJsonString(const std::string& json,
                              const std::string& key) {
  const std::regex pattern("\"" + key + R"(\"\s*:\s*\"([^\"]*)\")");
  std::smatch match;
  if (!std::regex_search(json, match, pattern) || match.size() != 2) {
    throw std::runtime_error("unable to extract string field from bootstrap manifest: " +
                             key);
  }
  return match[1].str();
}

bool ExtractJsonBool(const std::string& json, const std::string& key) {
  const std::regex pattern("\"" + key + R"(\"\s*:\s*(true|false))");
  std::smatch match;
  if (!std::regex_search(json, match, pattern) || match.size() != 2) {
    throw std::runtime_error("unable to extract boolean field from bootstrap manifest: " +
                             key);
  }
  return match[1].str() == "true";
}

NativeActivityBootstrap ReadNativeActivityBootstrapManifest(
    const std::string& bootstrap_manifest_path) {
  if (bootstrap_manifest_path.empty()) {
    throw std::invalid_argument("bootstrap_manifest_path must not be empty");
  }

  const fs::path manifest_path(bootstrap_manifest_path);
  const std::string json = ReadFile(manifest_path);
  const fs::path bootstrap_root = manifest_path.parent_path();
  const fs::path package_root = bootstrap_root.parent_path();

  NativeActivityBootstrap bootstrap;
  bootstrap.plan.assessment.package_name =
      ExtractJsonString(json, "package_name");
  bootstrap.plan.assessment.install_id = ExtractJsonString(json, "install_id");
  bootstrap.plan.assessment.launcher_component =
      ExtractJsonString(json, "launcher_component");
  bootstrap.plan.bundle_apk_path = ExtractJsonString(json, "bundle_apk_path");
  bootstrap.plan.sandbox_root = ExtractJsonString(json, "sandbox_root");
  bootstrap.plan.dex_cache_root = ExtractJsonString(json, "dex_cache_root");
  bootstrap.plan.resource_root = ExtractJsonString(json, "resource_root");
  bootstrap.plan.library_root = ExtractJsonString(json, "library_root");
  bootstrap.plan.bootstrap_spec_path =
      ExtractJsonString(json, "bootstrap_spec_path");
  bootstrap.plan.package_root = package_root.string();
  bootstrap.plan.bootstrap_root = bootstrap_root.string();
  bootstrap.plan.plan_written = true;
  bootstrap.bootstrap_manifest_path = manifest_path.string();
  bootstrap.env_script_path = (bootstrap_root / "native-env.sh").string();
  bootstrap.entrypoint_script_path =
      (bootstrap_root / "launch-native-activity.sh").string();
  bootstrap.report_path = (bootstrap_root / "bootstrap-report.txt").string();
  bootstrap.command_line = ExtractJsonString(json, "command_line");
  bootstrap.execution_engine_ready =
      ExtractJsonBool(json, "execution_engine_ready");
  bootstrap.bootstrap_ready = fs::exists(bootstrap.bootstrap_manifest_path) &&
                              fs::exists(bootstrap.env_script_path) &&
                              fs::exists(bootstrap.entrypoint_script_path) &&
                              fs::exists(bootstrap.plan.bundle_apk_path);
  return bootstrap;
}

std::vector<NativeServiceBinding> BuildDefaultServiceBindings() {
  return {
      {"activity_manager", "lifecycle", "ready",
       "Tracks activity creation, start, resume, and stop transitions for the native bootstrap session."},
      {"package_manager", "metadata", "ready",
       "Exposes package identity, launcher resolution, and staged APK metadata to the native bootstrap session."},
      {"resource_loader", "resources", "stub",
       "Owns resource-path handoff until the real asset and graphics pipeline is attached."},
      {"binder_registry", "ipc", "stub",
       "Reserves the service-discovery seam for a future Binder-compatible implementation."},
  };
}

}  // namespace

NativeLifecycleShim BuildNativeLifecycleShim(
    const NativeActivityBootstrap& bootstrap) {
  if (!bootstrap.bootstrap_ready) {
    throw std::invalid_argument(
        "native lifecycle shim requires a ready native activity bootstrap");
  }

  NativeLifecycleShim lifecycle;
  lifecycle.bootstrap = bootstrap;
  lifecycle.session_id = bootstrap.plan.assessment.install_id + "-default";
  lifecycle.session_root =
      (fs::path(bootstrap.plan.package_root) / "lifecycle" / lifecycle.session_id)
          .string();
  lifecycle.session_manifest_path =
      (fs::path(lifecycle.session_root) / "session.json").string();
  lifecycle.activity_state_path =
      (fs::path(lifecycle.session_root) / "activity-state.txt").string();
  lifecycle.service_registry_path =
      (fs::path(lifecycle.session_root) / "services.txt").string();
  lifecycle.report_path =
      (fs::path(lifecycle.session_root) / "lifecycle-report.txt").string();
  lifecycle.current_activity_state = "RESUMED";
  lifecycle.services = BuildDefaultServiceBindings();
  lifecycle.execution_engine_ready = false;

  fs::create_directories(lifecycle.session_root);

  std::ostringstream session_manifest;
  session_manifest << "{\n"
                   << "  \"package_name\": \""
                   << EscapeJson(bootstrap.plan.assessment.package_name)
                   << "\",\n"
                   << "  \"install_id\": \""
                   << EscapeJson(bootstrap.plan.assessment.install_id)
                   << "\",\n"
                   << "  \"launcher_component\": \""
                   << EscapeJson(bootstrap.plan.assessment.launcher_component)
                   << "\",\n"
                   << "  \"bootstrap_manifest_path\": \""
                   << EscapeJson(bootstrap.bootstrap_manifest_path) << "\",\n"
                   << "  \"activity_state\": \""
                   << EscapeJson(lifecycle.current_activity_state) << "\",\n"
                   << "  \"execution_engine_ready\": false\n"
                   << "}\n";
  WriteTextFile(lifecycle.session_manifest_path, session_manifest.str());

  std::ostringstream activity_state;
  activity_state << "package_name="
                 << bootstrap.plan.assessment.package_name << "\n";
  activity_state << "launcher_component="
                 << bootstrap.plan.assessment.launcher_component << "\n";
  activity_state << "created=true\n";
  activity_state << "started=true\n";
  activity_state << "resumed=true\n";
  activity_state << "current_state=" << lifecycle.current_activity_state
                 << "\n";
  WriteTextFile(lifecycle.activity_state_path, activity_state.str());

  std::ostringstream services;
  for (const auto& service : lifecycle.services) {
    services << service.service_name << "|" << service.service_kind << "|"
             << service.status << "|" << service.notes << "\n";
  }
  WriteTextFile(lifecycle.service_registry_path, services.str());

  lifecycle.lifecycle_handoff_ready =
      fs::exists(lifecycle.session_manifest_path) &&
      fs::exists(lifecycle.activity_state_path) &&
      fs::exists(lifecycle.service_registry_path) &&
      !lifecycle.services.empty();

  WriteTextFile(lifecycle.report_path,
                RenderNativeLifecycleShimReport(lifecycle));
  lifecycle.lifecycle_handoff_ready =
      lifecycle.lifecycle_handoff_ready && fs::exists(lifecycle.report_path);
  return lifecycle;
}

NativeLifecycleShim BuildNativeLifecycleShimFromManifest(
    const std::string& bootstrap_manifest_path) {
  return BuildNativeLifecycleShim(
      ReadNativeActivityBootstrapManifest(bootstrap_manifest_path));
}

std::string RenderNativeLifecycleShimReport(
    const NativeLifecycleShim& lifecycle) {
  std::ostringstream output;
  output << "Package: " << lifecycle.bootstrap.plan.assessment.package_name
         << "\n";
  output << "Install ID: " << lifecycle.bootstrap.plan.assessment.install_id
         << "\n";
  output << "Launcher Component: "
         << lifecycle.bootstrap.plan.assessment.launcher_component << "\n";
  output << "Bootstrap Manifest: " << lifecycle.bootstrap.bootstrap_manifest_path
         << "\n";
  output << "Session Root: " << lifecycle.session_root << "\n";
  output << "Session Manifest: " << lifecycle.session_manifest_path << "\n";
  output << "Activity State File: " << lifecycle.activity_state_path << "\n";
  output << "Service Registry: " << lifecycle.service_registry_path << "\n";
  output << "Lifecycle Handoff Ready: "
         << (lifecycle.lifecycle_handoff_ready ? "yes" : "no") << "\n";
  output << "Execution Engine Ready: "
         << (lifecycle.execution_engine_ready ? "yes" : "no") << "\n";
  output << "Current Activity State: " << lifecycle.current_activity_state
         << "\n";
  output << "Services:\n";
  for (const auto& service : lifecycle.services) {
    output << "  - " << service.service_name << " [" << service.service_kind
           << "] " << service.status << ": " << service.notes << "\n";
  }
  output << "Next Steps:\n";
  output << "  - Replace the lifecycle shim with a Linuxoid-owned process bootstrap.\n";
  output << "  - Attach DEX/class loading, resource lookup, and JNI plumbing.\n";
  output << "  - Keep outputs stable for MCP clients and validation harnesses.\n";
  return output.str();
}

}  // namespace wfa
