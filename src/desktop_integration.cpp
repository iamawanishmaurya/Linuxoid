#include "wfa/desktop_integration.hpp"

#include "wfa/apk_loader.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace wfa {

namespace fs = std::filesystem;

namespace {

std::string QuoteForShell(const std::string& value) {
  std::string quoted = "'";
  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(character);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

std::string SanitizeFilenameSegment(const std::string& value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (const char character : value) {
    const bool safe = (character >= 'A' && character <= 'Z') ||
                      (character >= 'a' && character <= 'z') ||
                      (character >= '0' && character <= '9') ||
                      character == '.' || character == '_' || character == '-';
    sanitized.push_back(safe ? character : '_');
  }
  if (sanitized.empty()) {
    return "android-app";
  }
  return sanitized;
}

std::string PackageNameFromComponent(const std::string& component) {
  const auto slash = component.find('/');
  if (slash == std::string::npos) {
    return "";
  }
  return component.substr(0, slash);
}

std::string QuoteForDesktopExec(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 2);
  escaped.push_back('"');
  for (const char character : value) {
    if (character == '\\' || character == '"') {
      escaped.push_back('\\');
    }
    escaped.push_back(character);
  }
  escaped.push_back('"');
  return escaped;
}

std::string CanonicalizeAndroidComponent(const std::string& package_name,
                                         const std::string& component) {
  const std::string normalized =
      NormalizeAndroidComponent(package_name, component);
  const auto slash = normalized.find('/');
  if (slash == std::string::npos) {
    return normalized;
  }

  const std::string normalized_package = normalized.substr(0, slash);
  std::string class_name = normalized.substr(slash + 1);
  if (!class_name.empty() && class_name.front() == '.') {
    class_name = normalized_package + class_name;
  }
  return normalized_package + "/" + class_name;
}

bool DeclaresActivityComponent(const ManifestProfile& profile,
                               const std::string& component) {
  const std::string normalized_target =
      CanonicalizeAndroidComponent(profile.package_name, component);
  for (const auto& declared : profile.declared_activity_components) {
    if (CanonicalizeAndroidComponent(profile.package_name, declared) ==
        normalized_target) {
      return true;
    }
  }
  return false;
}

std::string ResolveLauncherRoot(const DesktopLaunchSpec& spec) {
  return spec.launcher_root.empty() ? spec.desktop_root : spec.launcher_root;
}

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

}  // namespace

std::string NormalizeAndroidComponent(const std::string& package_name,
                                      const std::string& component) {
  if (package_name.empty() || component.empty()) {
    throw std::invalid_argument(
        "package name and component are required for component normalization");
  }

  if (component.find('/') != std::string::npos) {
    return component;
  }
  if (component.front() == '.') {
    return package_name + "/" + component;
  }
  return package_name + "/" + component;
}

std::string SelectLauncherActivityComponent(const ManifestProfile& profile) {
  if (!profile.has_launcher_activity || profile.launcher_activity_name.empty()) {
    throw std::invalid_argument(
        "desktopified APK does not declare a launcher activity for automatic host launch");
  }
  if (!DeclaresActivityComponent(profile, profile.launcher_activity_name)) {
    throw std::invalid_argument(
        "recorded launcher activity is not declared as an activity by the desktopified APK");
  }
  return NormalizeAndroidComponent(profile.package_name,
                                   profile.launcher_activity_name);
}

DesktopLaunchArtifacts CreateDesktopLaunchArtifacts(
    const DesktopLaunchSpec& spec) {
  if (spec.serial.empty() || spec.apk_path.empty() || spec.package_name.empty() ||
      spec.launcher_component.empty() || spec.compatctl_path.empty() ||
      spec.desktop_root.empty()) {
    throw std::invalid_argument(
        "desktop launch spec is missing required fields");
  }

  DesktopLaunchArtifacts artifacts;
  artifacts.app_name = spec.app_name.empty() ? spec.package_name : spec.app_name;
  artifacts.serial = spec.serial;
  artifacts.apk_path = spec.apk_path;
  artifacts.package_name = spec.package_name;
  artifacts.launcher_component =
      NormalizeAndroidComponent(spec.package_name, spec.launcher_component);
  if (PackageNameFromComponent(artifacts.launcher_component) !=
      spec.package_name) {
    throw std::invalid_argument(
        "launcher component package does not match desktopified APK package");
  }
  artifacts.ime_component = spec.ime_component.empty()
                                ? ""
                                : NormalizeAndroidComponent(spec.package_name,
                                                            spec.ime_component);
  if (!artifacts.ime_component.empty() &&
      PackageNameFromComponent(artifacts.ime_component) != spec.package_name) {
    throw std::invalid_argument(
        "IME component package does not match desktopified APK package");
  }
  artifacts.compatctl_path = spec.compatctl_path;
  artifacts.desktop_root = spec.desktop_root;
  artifacts.launcher_root = ResolveLauncherRoot(spec);
  artifacts.uses_provision_mode = !artifacts.ime_component.empty();

  fs::create_directories(spec.desktop_root);
  fs::create_directories(artifacts.launcher_root);

  const std::string safe_name = SanitizeFilenameSegment(spec.package_name);
  const fs::path script_path =
      fs::path(artifacts.launcher_root) / (safe_name + ".sh");
  const fs::path desktop_file_path =
      fs::path(spec.desktop_root) / (safe_name + ".desktop");
  artifacts.script_path = script_path.string();
  artifacts.desktop_file_path = desktop_file_path.string();

  std::ostringstream command;
  command << QuoteForShell(spec.compatctl_path) << ' ';
  if (artifacts.uses_provision_mode) {
    command << "provision-ime " << QuoteForShell(spec.serial) << ' '
            << QuoteForShell(spec.apk_path) << ' '
            << QuoteForShell(spec.package_name) << ' '
            << QuoteForShell(artifacts.ime_component) << ' '
            << QuoteForShell(artifacts.launcher_component);
  } else {
    command << "launch-activity " << QuoteForShell(spec.serial) << ' '
            << QuoteForShell(artifacts.launcher_component);
  }
  artifacts.command_line = command.str();

  std::ostringstream script;
  script << "#!/bin/sh\n";
  script << "exec " << artifacts.command_line << "\n";
  WriteTextFile(script_path, script.str());

  fs::permissions(script_path,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec | fs::perms::others_read |
                      fs::perms::others_exec,
                  fs::perm_options::replace);

  std::ostringstream desktop_entry;
  desktop_entry << "[Desktop Entry]\n";
  desktop_entry << "Type=Application\n";
  desktop_entry << "Version=1.0\n";
  desktop_entry << "Name=" << artifacts.app_name << " (Android)\n";
  desktop_entry << "Exec=" << QuoteForDesktopExec(artifacts.script_path)
                << "\n";
  desktop_entry << "Terminal=false\n";
  desktop_entry << "StartupNotify=true\n";
  desktop_entry << "Categories=Utility;\n";
  WriteTextFile(desktop_file_path, desktop_entry.str());

  artifacts.host_launch_ready =
      fs::exists(script_path) && fs::exists(desktop_file_path) &&
      fs::exists(spec.compatctl_path) &&
      (!artifacts.uses_provision_mode || fs::exists(spec.apk_path));
  return artifacts;
}

DesktopLaunchArtifacts CreateDesktopLaunchArtifactsForLoadedApk(
    const LoadedApkReport& report, const std::string& serial,
    const std::string& component, const std::string& compatctl_path,
    const std::string& desktop_root, const std::string& launcher_root) {
  if (!DeclaresActivityComponent(report.manifest_profile, component)) {
    throw std::invalid_argument(
        "requested launcher component is not declared as an activity by the desktopified APK");
  }
  const fs::path staged_apk_path =
      fs::path(report.layout.host_package_root) / "base.apk";
  return CreateDesktopLaunchArtifacts(DesktopLaunchSpec{
      .app_name = report.manifest_profile.package_name,
      .serial = serial,
      .apk_path = staged_apk_path.string(),
      .package_name = report.manifest_profile.package_name,
      .launcher_component = component,
      .ime_component = report.manifest_profile.input_method_service_name,
      .compatctl_path = compatctl_path,
      .desktop_root = desktop_root,
      .launcher_root = launcher_root,
  });
}

DesktopLaunchArtifacts CreateDesktopLaunchArtifactsForLoadedApkAuto(
    const LoadedApkReport& report, const std::string& serial,
    const std::string& compatctl_path, const std::string& desktop_root,
    const std::string& launcher_root) {
  auto artifacts = CreateDesktopLaunchArtifactsForLoadedApk(
      report, serial, SelectLauncherActivityComponent(report.manifest_profile),
      compatctl_path, desktop_root, launcher_root);
  artifacts.launcher_component_inferred = true;
  return artifacts;
}

DesktopLaunchArtifacts DesktopifyApk(const std::string& serial,
                                     const std::string& apk_path,
                                     const std::string& component,
                                     const std::string& compat_root,
                                     const std::string& desktop_root,
                                     const std::string& launcher_root,
                                     const std::string& compatctl_path) {
  const auto report = LoadApkToCompatRoot(apk_path, compat_root);
  return CreateDesktopLaunchArtifactsForLoadedApk(
      report, serial, component, compatctl_path, desktop_root, launcher_root);
}

DesktopLaunchArtifacts DesktopifyApkAuto(const std::string& serial,
                                         const std::string& apk_path,
                                         const std::string& compat_root,
                                         const std::string& desktop_root,
                                         const std::string& launcher_root,
                                         const std::string& compatctl_path) {
  const auto report = LoadApkToCompatRoot(apk_path, compat_root);
  return CreateDesktopLaunchArtifactsForLoadedApkAuto(
      report, serial, compatctl_path, desktop_root, launcher_root);
}

std::string RenderDesktopLaunchArtifactsReport(
    const DesktopLaunchArtifacts& artifacts) {
  std::ostringstream output;
  output << "Desktop Loading: " << (artifacts.host_launch_ready ? "35/100" : "0/100")
         << '\n';
  output << "App Name: " << artifacts.app_name << '\n';
  output << "Package: " << artifacts.package_name << '\n';
  output << "ADB Serial: " << artifacts.serial << '\n';
  output << "Launcher Component: " << artifacts.launcher_component << '\n';
  output << "Launcher Selection: "
         << (artifacts.launcher_component_inferred ? "inferred" : "explicit")
         << '\n';
  output << "IME Component: " << artifacts.ime_component << '\n';
  output << "Launch Mode: "
         << (artifacts.uses_provision_mode ? "provision-ime" : "launch-activity")
         << '\n';
  if (artifacts.uses_provision_mode) {
    output << "Launch Side Effects: may reinstall the APK, enable the IME, and "
              "set it as default before opening the target component.\n";
  }
  output << "Launcher Script: " << artifacts.script_path << '\n';
  output << "Launcher Root: " << artifacts.launcher_root << '\n';
  output << "Desktop Entry: " << artifacts.desktop_file_path << '\n';
  output << "Desktop Entry Root: " << artifacts.desktop_root << '\n';
  output << "Host launch ready: "
         << (artifacts.host_launch_ready ? "yes" : "no") << '\n';
  return output.str();
}

}  // namespace wfa
