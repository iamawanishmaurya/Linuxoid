#include "wfa/native_spike.hpp"

#include "wfa/manifest_assessment.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace wfa {

namespace fs = std::filesystem;

namespace {

void AppendUnique(std::vector<std::string>& values, const std::string& value) {
  if (value.empty()) {
    return;
  }

  for (const auto& existing : values) {
    if (existing == value) {
      return;
    }
  }
  values.push_back(value);
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

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

std::string NormalizeAndroidComponent(const std::string& package_name,
                                      const std::string& component) {
  if (package_name.empty() || component.empty()) {
    return "";
  }

  const auto slash = component.find('/');
  if (slash != std::string::npos) {
    const std::string class_name = component.substr(slash + 1);
    if (!class_name.empty() && class_name.front() == '.') {
      return component;
    }
    if (class_name.rfind(package_name + ".", 0) == 0) {
      return package_name + "/." + class_name.substr(package_name.size() + 1);
    }
    return component;
  }

  if (component.front() == '.') {
    return package_name + "/" + component;
  }
  if (component.rfind(package_name + ".", 0) == 0) {
    return package_name + "/." + component.substr(package_name.size() + 1);
  }
  return package_name + "/" + component;
}

std::string RenderJsonArray(const std::vector<std::string>& values) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    output << "\"" << EscapeJson(values[index]) << "\"";
  }
  output << "]";
  return output.str();
}

}  // namespace

NativeSpikeAssessment AssessNativeSpikeCandidate(
    const LoadedApkReport& report) {
  NativeSpikeAssessment assessment;
  assessment.package_name = report.manifest_profile.package_name;
  assessment.install_id = report.install_id;
  assessment.app_profile = report.assessment.app_profile;
  assessment.launcher_component = NormalizeAndroidComponent(
      report.manifest_profile.package_name,
      report.manifest_profile.launcher_activity_name);
  assessment.min_sdk = report.metadata.min_sdk;
  assessment.target_sdk = report.metadata.target_sdk;
  assessment.blockers = report.assessment.blockers;

  if (report.assessment.app_profile != "foreground_app") {
    AppendUnique(assessment.blockers,
                 "Native spike currently targets simple foreground apps only.");
  }
  if (!report.manifest_profile.has_launcher_activity ||
      assessment.launcher_component.empty()) {
    AppendUnique(
        assessment.blockers,
        "Native spike requires one resolvable launcher activity component.");
  }
  if (report.manifest_profile.has_background_service) {
    AppendUnique(
        assessment.blockers,
        "Native spike excludes background services in the first native slice.");
  }
  if (report.manifest_profile.has_input_method_service) {
    AppendUnique(assessment.blockers,
                 "Native spike excludes IME apps in the first native slice.");
  }
  if (report.manifest_profile.requests_boot_completed) {
    AppendUnique(
        assessment.blockers,
        "Native spike excludes boot-complete behaviors in the first native slice.");
  }
  if (report.manifest_profile.uses_secondary_processes) {
    AppendUnique(
        assessment.blockers,
        "Native spike excludes secondary processes in the first native slice.");
  }
  if (report.assessment.earliest_full_use_phase != "P6") {
    AppendUnique(
        assessment.blockers,
        "Native spike currently requires an app whose earliest full-use phase is P6.");
  }

  assessment.native_spike_candidate = assessment.blockers.empty();
  if (assessment.native_spike_candidate) {
    assessment.next_steps = {
        "Add Linuxoid-owned activity bootstrap for the launcher component.",
        "Attach the first DEX/class loading path for the simple foreground app.",
        "Connect the app to Linux window, input, and resource plumbing.",
    };
  } else {
    assessment.next_steps = {
        "Keep using Waydroid and attached-target verification as regression oracles.",
        "Reduce manifest/runtime blockers until the app fits the first native slice.",
        "Re-run the native spike planner after trimming app complexity.",
    };
  }

  return assessment;
}

NativeLaunchPlan BuildNativeLaunchPlan(const LoadedApkReport& report,
                                       const std::string& native_root) {
  if (native_root.empty()) {
    throw std::invalid_argument("native_root must not be empty");
  }
  if (report.install_root.empty()) {
    throw std::invalid_argument(
        "loaded apk report must include an install_root for native planning");
  }

  NativeLaunchPlan plan;
  plan.assessment = AssessNativeSpikeCandidate(report);
  plan.apk_path = report.apk_path;
  plan.staged_apk_path =
      (fs::path(report.install_root) / "base.apk").string();
  plan.native_root = native_root;

  const fs::path package_root = fs::path(native_root) / "packages" /
                                report.manifest_profile.package_name /
                                report.install_id;
  const fs::path bundle_root = package_root / "bundle";
  const fs::path sandbox_root = package_root / "sandbox";
  const fs::path dex_cache_root = package_root / "dex-cache";
  const fs::path resource_root = package_root / "resources";
  const fs::path library_root = package_root / "lib";
  const fs::path bootstrap_root = package_root / "bootstrap";

  fs::create_directories(bundle_root);
  fs::create_directories(sandbox_root);
  fs::create_directories(dex_cache_root);
  fs::create_directories(resource_root);
  fs::create_directories(library_root);
  fs::create_directories(bootstrap_root);

  plan.package_root = package_root.string();
  plan.bundle_root = bundle_root.string();
  plan.sandbox_root = sandbox_root.string();
  plan.dex_cache_root = dex_cache_root.string();
  plan.resource_root = resource_root.string();
  plan.library_root = library_root.string();
  plan.bootstrap_root = bootstrap_root.string();
  plan.bundle_apk_path = (bundle_root / "base.apk").string();
  plan.manifest_copy_path = (bundle_root / "AndroidManifest.xml").string();
  plan.assessment_copy_path = (bundle_root / "assessment.txt").string();
  plan.bootstrap_spec_path = (bootstrap_root / "native-plan.json").string();

  const fs::path staged_apk(plan.staged_apk_path);
  if (!fs::exists(staged_apk)) {
    throw std::runtime_error("staged APK does not exist: " +
                             staged_apk.string());
  }
  fs::copy_file(staged_apk, plan.bundle_apk_path,
                fs::copy_options::overwrite_existing);

  const fs::path staged_manifest = fs::path(report.install_root) /
                                   "AndroidManifest.xml";
  if (fs::exists(staged_manifest)) {
    fs::copy_file(staged_manifest, plan.manifest_copy_path,
                  fs::copy_options::overwrite_existing);
  } else {
    WriteTextFile(plan.manifest_copy_path,
                  "<manifest package=\"" +
                      report.manifest_profile.package_name + "\"/>\n");
  }

  const fs::path staged_assessment = fs::path(report.install_root) /
                                     "assessment.txt";
  if (fs::exists(staged_assessment)) {
    fs::copy_file(staged_assessment, plan.assessment_copy_path,
                  fs::copy_options::overwrite_existing);
  } else {
    WriteTextFile(plan.assessment_copy_path,
                  RenderManifestAssessmentReport(report.assessment));
  }

  std::ostringstream spec;
  spec << "{\n"
       << "  \"package_name\": \""
       << EscapeJson(plan.assessment.package_name) << "\",\n"
       << "  \"install_id\": \"" << EscapeJson(plan.assessment.install_id)
       << "\",\n"
       << "  \"apk_path\": \"" << EscapeJson(plan.apk_path) << "\",\n"
       << "  \"staged_apk_path\": \"" << EscapeJson(plan.staged_apk_path)
       << "\",\n"
       << "  \"bundle_apk_path\": \"" << EscapeJson(plan.bundle_apk_path)
       << "\",\n"
       << "  \"launcher_component\": \""
       << EscapeJson(plan.assessment.launcher_component) << "\",\n"
       << "  \"native_spike_candidate\": "
       << (plan.assessment.native_spike_candidate ? "true" : "false") << ",\n"
       << "  \"app_profile\": \""
       << EscapeJson(plan.assessment.app_profile) << "\",\n"
       << "  \"min_sdk\": " << plan.assessment.min_sdk << ",\n"
       << "  \"target_sdk\": " << plan.assessment.target_sdk << ",\n"
       << "  \"package_root\": \"" << EscapeJson(plan.package_root)
       << "\",\n"
       << "  \"sandbox_root\": \"" << EscapeJson(plan.sandbox_root)
       << "\",\n"
       << "  \"dex_cache_root\": \"" << EscapeJson(plan.dex_cache_root)
       << "\",\n"
       << "  \"resource_root\": \"" << EscapeJson(plan.resource_root)
       << "\",\n"
       << "  \"library_root\": \"" << EscapeJson(plan.library_root)
       << "\",\n"
       << "  \"blockers\": "
       << RenderJsonArray(plan.assessment.blockers) << ",\n"
       << "  \"next_steps\": "
       << RenderJsonArray(plan.assessment.next_steps) << "\n"
       << "}\n";
  WriteTextFile(plan.bootstrap_spec_path, spec.str());

  plan.plan_written = fs::exists(plan.bundle_apk_path) &&
                      fs::exists(plan.manifest_copy_path) &&
                      fs::exists(plan.assessment_copy_path) &&
                      fs::exists(plan.bootstrap_spec_path);
  return plan;
}

NativeLaunchPlan PlanNativeLaunchSpike(const std::string& apk_path,
                                       const std::string& compat_root,
                                       const std::string& native_root) {
  const auto report = LoadApkToCompatRoot(apk_path, compat_root);
  return BuildNativeLaunchPlan(report, native_root);
}

std::string RenderNativeLaunchPlanReport(const NativeLaunchPlan& plan) {
  std::ostringstream output;
  output << "Package: " << plan.assessment.package_name << '\n';
  output << "Install ID: " << plan.assessment.install_id << '\n';
  output << "Launcher Component: " << plan.assessment.launcher_component
         << '\n';
  output << "Min SDK: " << plan.assessment.min_sdk << '\n';
  output << "Target SDK: " << plan.assessment.target_sdk << '\n';
  output << "Native Spike Candidate: "
         << (plan.assessment.native_spike_candidate ? "yes" : "no") << '\n';
  output << "Staged APK: " << plan.staged_apk_path << '\n';
  output << "Package Root: " << plan.package_root << '\n';
  output << "Bundle Root: " << plan.bundle_root << '\n';
  output << "Sandbox Root: " << plan.sandbox_root << '\n';
  output << "DEX Cache Root: " << plan.dex_cache_root << '\n';
  output << "Resource Root: " << plan.resource_root << '\n';
  output << "Library Root: " << plan.library_root << '\n';
  output << "Bootstrap Spec: " << plan.bootstrap_spec_path << '\n';
  output << "Plan Written: " << (plan.plan_written ? "yes" : "no") << '\n';

  output << "Blockers:\n";
  if (plan.assessment.blockers.empty()) {
    output << "  - none\n";
  } else {
    for (const auto& blocker : plan.assessment.blockers) {
      output << "  - " << blocker << '\n';
    }
  }

  output << "Next Steps:\n";
  for (const auto& step : plan.assessment.next_steps) {
    output << "  - " << step << '\n';
  }

  return output.str();
}

}  // namespace wfa
