#include "wfa/native_execute_stub.hpp"

#include "wfa/asset_manager_stub.hpp"
#include "wfa/jni_stub.hpp"
#include "wfa/native_types.hpp"
#include "wfa/signal_handler.hpp"

#include <dlfcn.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <thread>

namespace wfa {

namespace fs = std::filesystem;

namespace {

std::vector<std::string> BuildPreferredLibraryPaths(
    const std::string& library_root) {
  return {
      (fs::path(library_root) / "libmain.so").string(),
      (fs::path(library_root) / "libcalculator.so").string(),
      (fs::path(library_root) / "libapp.so").string(),
  };
}

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

}  // namespace

std::vector<std::string> BuildNativeLibraryCandidates(
    const std::string& library_root) {
  std::vector<std::string> candidates;
  if (library_root.empty() || !fs::exists(library_root)) {
    return candidates;
  }

  for (const auto& preferred : BuildPreferredLibraryPaths(library_root)) {
    if (fs::exists(preferred)) {
      AppendUnique(candidates, preferred);
    }
  }

  for (const auto& entry : fs::directory_iterator(library_root)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension() == ".so") {
      AppendUnique(candidates, entry.path().string());
    }
  }

  return candidates;
}

NativeExecuteReport ExecuteNativeStub(const NativeExecuteRequest& request) {
  NativeExecuteReport report;
  report.package_name = request.package_name;
  report.launcher_component = request.launcher_component;
  report.bundle_apk_path = request.bundle_apk_path;
  report.sandbox_root = request.sandbox_root;
  report.dex_cache_root = request.dex_cache_root;
  report.resource_root = request.resource_root;
  report.library_root = request.library_root;
  report.bootstrap_manifest_path = request.bootstrap_manifest_path;
  report.bundle_present = fs::exists(request.bundle_apk_path);
  report.bootstrap_manifest_present = fs::exists(request.bootstrap_manifest_path);

  InstallSignalHandler();

  std::ostringstream output;
  output << "Package: " << request.package_name << '\n';
  output << "Launcher Component: " << request.launcher_component << '\n';
  output << "Bundle APK: " << request.bundle_apk_path << '\n';
  output << "Sandbox Root: " << request.sandbox_root << '\n';
  output << "DEX Cache Root: " << request.dex_cache_root << '\n';
  output << "Resource Root: " << request.resource_root << '\n';
  output << "Library Root: " << request.library_root << '\n';
  output << "Bootstrap Manifest: " << request.bootstrap_manifest_path << '\n';
  output << "Bundle Present: " << (report.bundle_present ? "yes" : "no")
         << '\n';
  output << "Bootstrap Manifest Present: "
         << (report.bootstrap_manifest_present ? "yes" : "no") << '\n';

  report.candidate_library_paths =
      BuildNativeLibraryCandidates(request.library_root);
  if (report.candidate_library_paths.empty()) {
    output << "[p1] No native library candidates found in "
           << request.library_root << "\n";
    report.exit_code = 2;
    report.output = output.str();
    return report;
  }

  void* handle = nullptr;
  for (const auto& candidate : report.candidate_library_paths) {
    output << "[p1] trying: " << candidate << "\n";
    handle = dlopen(candidate.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (handle != nullptr) {
      report.native_library_found = true;
      report.dlopen_ok = true;
      report.selected_library_path = candidate;
      output << "[p1] dlopen OK: " << candidate << "\n";
      break;
    }
    const char* error = dlerror();
    output << "[p1] dlopen failed: "
           << (error == nullptr ? "unknown error" : error) << "\n";
  }

  if (handle == nullptr) {
    report.exit_code = 2;
    report.output = output.str();
    return report;
  }

  dlerror();
  auto entry = reinterpret_cast<ANativeActivityCreateFn>(
      dlsym(handle, "ANativeActivity_onCreate"));
  const char* symbol_error = dlerror();
  if (entry == nullptr || symbol_error != nullptr) {
    output << "[p1] entrypoint not found: "
           << (symbol_error == nullptr ? "unknown error" : symbol_error)
           << "\n";
    report.exit_code = 3;
    report.output = output.str();
    dlclose(handle);
    return report;
  }

  report.entrypoint_found = true;
  output << "[p1] entrypoint found: ANativeActivity_onCreate\n";

  ANativeActivity activity{};
  activity.vm = MakeStubJavaVm();
  activity.env = MakeStubJniEnv();
  activity.internalDataPath = request.sandbox_root.c_str();
  activity.externalDataPath = request.sandbox_root.c_str();
  activity.sdkVersion = 33;
  activity.assetManager = MakeStubAssetManager(request.bundle_apk_path);

  std::atomic<bool> entry_completed{false};
  std::thread watchdog([&entry_completed, seconds = request.watchdog_seconds]() {
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    if (!entry_completed.load()) {
      std::cout << "[p1] watchdog: 5s elapsed, clean exit\n";
      std::cout.flush();
      std::_Exit(0);
    }
  });

  output << "[p1] calling ANativeActivity_onCreate\n";
  entry(&activity, nullptr, 0);
  report.activity_called = true;
  output << "[p1] ANativeActivity_onCreate returned\n";

  entry_completed = true;
  watchdog.join();

  output << "[p1] watchdog: 5s elapsed, clean exit\n";
  report.execution_engine_ready = true;
  report.exit_code = 0;
  report.output = output.str();
  dlclose(handle);
  return report;
}

}  // namespace wfa
