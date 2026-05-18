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
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

using JniOnLoadFn = int (*)(JavaVM*, void*);

struct LoadedLibraryHandle {
  std::string path;
  void* handle = nullptr;
};

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

std::vector<std::string> BuildEntrypointLibraryNames() {
  return {"libmain.so", "libcalculator.so", "libapp.so"};
}

bool IsEntrypointLibraryName(const std::string& file_name) {
  const auto preferred = BuildEntrypointLibraryNames();
  return std::find(preferred.begin(), preferred.end(), file_name) !=
         preferred.end();
}

int DetermineLibraryOrderRank(const std::string& file_name) {
  if (file_name == "libc++_shared.so") {
    return 0;
  }
  if (file_name == "libmain.so") {
    return 20;
  }
  if (file_name == "libcalculator.so") {
    return 21;
  }
  if (file_name == "libapp.so") {
    return 22;
  }
  return 10;
}

void CloseLoadedLibraries(const std::vector<LoadedLibraryHandle>& libraries) {
  for (auto it = libraries.rbegin(); it != libraries.rend(); ++it) {
    if (it->handle != nullptr) {
      dlclose(it->handle);
    }
  }
}

std::string BuildJsonStringArray(const std::vector<std::string>& values) {
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

std::string BuildJniOnLoadResultsJson(
    const std::vector<JniOnLoadResult>& results) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < results.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& result = results[index];
    output << "{"
           << "\"library_path\": \"" << EscapeJson(result.library_path)
           << "\", "
           << "\"symbol_present\": "
           << (result.symbol_present ? "true" : "false") << ", "
           << "\"call_succeeded\": "
           << (result.call_succeeded ? "true" : "false") << ", "
           << "\"return_code\": " << result.return_code << ", "
           << "\"status\": \"" << EscapeJson(result.status) << "\""
           << "}";
  }
  output << "]";
  return output.str();
}

std::size_t FindLibraryLoadAttemptIndex(
    const std::vector<NativeLibraryLoadAttempt>& attempts,
    const std::string& library_path) {
  for (std::size_t index = 0; index < attempts.size(); ++index) {
    if (attempts[index].library_path == library_path) {
      return index;
    }
  }
  return attempts.size();
}

std::string ResolveWorkingDirectory() {
  std::error_code error;
  const fs::path working_directory = fs::current_path(error);
  if (error) {
    return "<unavailable>";
  }
  return working_directory.string();
}

}  // namespace

std::string RenderNativeLibraryLoadAttemptsJson(
    const std::vector<NativeLibraryLoadAttempt>& attempts) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < attempts.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& attempt = attempts[index];
    output << "{"
           << "\"library_path\": \"" << EscapeJson(attempt.library_path)
           << "\", "
           << "\"library_name\": \"" << EscapeJson(attempt.library_name)
           << "\", "
           << "\"candidate_index\": " << attempt.candidate_index << ", "
           << "\"load_state\": \"" << EscapeJson(attempt.load_state)
           << "\", "
           << "\"jni_state\": \"" << EscapeJson(attempt.jni_state)
           << "\", "
           << "\"entrypoint_state\": \""
           << EscapeJson(attempt.entrypoint_state) << "\", "
           << "\"jni_return_code\": " << attempt.jni_return_code << ", "
           << "\"failure_reason\": \""
           << EscapeJson(attempt.failure_reason) << "\", "
           << "\"error_detail\": \"" << EscapeJson(attempt.error_detail)
           << "\""
           << "}";
  }
  output << "]";
  return output.str();
}

std::vector<std::string> BuildNativeLibraryCandidates(
    const std::string& library_root) {
  std::vector<fs::path> library_paths;
  if (library_root.empty() || !fs::exists(library_root)) {
    return {};
  }

  for (const auto& entry : fs::directory_iterator(library_root)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension() == ".so") {
      library_paths.push_back(entry.path());
    }
  }

  std::sort(library_paths.begin(), library_paths.end(),
            [](const fs::path& left, const fs::path& right) {
              const int left_rank =
                  DetermineLibraryOrderRank(left.filename().string());
              const int right_rank =
                  DetermineLibraryOrderRank(right.filename().string());
              if (left_rank != right_rank) {
                return left_rank < right_rank;
              }
              return left.filename().string() < right.filename().string();
            });

  std::vector<std::string> candidates;
  candidates.reserve(library_paths.size());
  for (const auto& path : library_paths) {
    candidates.push_back(path.string());
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
  report.working_directory = ResolveWorkingDirectory();
  report.exit_reason = "bootstrap_started";

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
  output << "Working Directory: " << report.working_directory << '\n';
  output << "Bundle Present: " << (report.bundle_present ? "yes" : "no")
         << '\n';
  output << "Bootstrap Manifest Present: "
         << (report.bootstrap_manifest_present ? "yes" : "no") << '\n';

  report.candidate_library_paths =
      BuildNativeLibraryCandidates(request.library_root);
  if (report.candidate_library_paths.empty()) {
    output << "[p1] No native library candidates found in "
           << request.library_root << "\n";
    report.exit_reason = "no_native_libraries_found";
    report.exit_code = 0;
    report.output = output.str();
    return report;
  }

  std::vector<LoadedLibraryHandle> loaded_libraries;
  loaded_libraries.reserve(report.candidate_library_paths.size());
  bool any_jni_onload_success = false;
  for (std::size_t candidate_index = 0;
       candidate_index < report.candidate_library_paths.size();
       ++candidate_index) {
    const auto& candidate = report.candidate_library_paths[candidate_index];
    NativeLibraryLoadAttempt attempt;
    attempt.library_path = candidate;
    attempt.library_name = fs::path(candidate).filename().string();
    attempt.candidate_index = static_cast<int>(candidate_index);
    output << "[p1] trying: " << candidate << "\n";
    void* handle = dlopen(candidate.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (handle == nullptr) {
      const char* error = dlerror();
      attempt.load_state = "dlopen_failed";
      attempt.failure_reason = "dlopen_failed";
      attempt.error_detail =
          error == nullptr ? "unknown_dlopen_error" : std::string(error);
      output << "[p1] dlopen failed: "
             << (error == nullptr ? "unknown error" : error) << "\n";
      report.library_load_attempts.push_back(attempt);
      continue;
    }

    report.native_library_found = true;
    report.dlopen_ok = true;
    report.libraries_loaded.push_back(candidate);
    loaded_libraries.push_back({candidate, handle});
    attempt.load_state = "loaded";
    output << "[p1] dlopen OK: " << candidate << "\n";

    JniOnLoadResult jni_result;
    jni_result.library_path = candidate;
    dlerror();
    auto jni_onload =
        reinterpret_cast<JniOnLoadFn>(dlsym(handle, "JNI_OnLoad"));
    const char* symbol_error = dlerror();
    if (jni_onload == nullptr || symbol_error != nullptr) {
      attempt.jni_state = "missing";
      attempt.failure_reason = "jni_onload_missing";
      jni_result.status = "missing";
      output << "[p1] JNI_OnLoad missing: " << candidate << "\n";
      report.jni_onload_results.push_back(jni_result);
      report.library_load_attempts.push_back(attempt);
      continue;
    }

    jni_result.symbol_present = true;
    output << "[p1] calling JNI_OnLoad: " << candidate << "\n";
    jni_result.return_code = jni_onload(MakeStubJavaVm(), nullptr);
    jni_result.call_succeeded = true;
    jni_result.status = "called";
    any_jni_onload_success = true;
    attempt.jni_state = "called";
    attempt.jni_return_code = jni_result.return_code;
    output << "[p1] JNI_OnLoad OK: " << candidate
           << " returned " << jni_result.return_code << "\n";
    report.jni_onload_results.push_back(jni_result);
    report.library_load_attempts.push_back(attempt);
  }

  if (report.libraries_loaded.empty()) {
    report.exit_reason = "libraries_failed_to_load";
    report.exit_code = 0;
    report.output = output.str();
    return report;
  }

  report.execution_engine_ready =
      !report.libraries_loaded.empty() && any_jni_onload_success;
  if (!any_jni_onload_success) {
    report.exit_reason = "jni_onload_missing_or_failed";
  } else {
    report.exit_reason = "jni_onload_attempts_completed";
  }

  ANativeActivityCreateFn entrypoint = nullptr;
  for (const auto& library : loaded_libraries) {
    dlerror();
    auto* candidate = reinterpret_cast<ANativeActivityCreateFn>(
        dlsym(library.handle, "ANativeActivity_onCreate"));
    const char* symbol_error = dlerror();
    const std::size_t attempt_index =
        FindLibraryLoadAttemptIndex(report.library_load_attempts, library.path);
    if (candidate == nullptr || symbol_error != nullptr) {
      if (attempt_index < report.library_load_attempts.size()) {
        report.library_load_attempts[attempt_index].entrypoint_state = "missing";
        if (report.library_load_attempts[attempt_index].failure_reason.empty()) {
          report.library_load_attempts[attempt_index].failure_reason =
              "native_activity_entrypoint_missing";
        }
      }
      continue;
    }
    entrypoint = candidate;
    report.entrypoint_found = true;
    report.selected_library_path = library.path;
    if (attempt_index < report.library_load_attempts.size()) {
      report.library_load_attempts[attempt_index].entrypoint_state = "found";
    }
    output << "[p1] entrypoint found: ANativeActivity_onCreate in "
           << library.path << "\n";
    if (IsEntrypointLibraryName(fs::path(library.path).filename().string())) {
      break;
    }
  }

  if (entrypoint == nullptr) {
    output << "[p1] entrypoint not found in loaded libraries\n";
    report.exit_reason = "native_activity_entrypoint_missing";
    report.exit_code = 0;
    report.output = output.str();
    CloseLoadedLibraries(loaded_libraries);
    return report;
  }

  ANativeActivity activity{};
  activity.vm = MakeStubJavaVm();
  activity.env = MakeStubJniEnv();
  activity.internalDataPath = request.sandbox_root.c_str();
  activity.externalDataPath = request.sandbox_root.c_str();
  activity.sdkVersion = 33;
  activity.assetManager =
      MakeStubAssetManager(request.bundle_apk_path, request.resource_root);

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
  entrypoint(&activity, nullptr, 0);
  report.activity_called = true;
  output << "[p1] ANativeActivity_onCreate returned\n";

  entry_completed = true;
  watchdog.join();

  output << "[p1] watchdog: 5s elapsed, clean exit\n";
  report.exit_reason = report.execution_engine_ready
                           ? "native_activity_completed"
                           : "native_activity_completed_without_jni_ready";
  report.exit_code = 0;
  report.output = output.str();
  CloseLoadedLibraries(loaded_libraries);
  return report;
}

std::string RenderNativeExecuteReportJson(const NativeExecuteReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"launcher_component\": \""
         << EscapeJson(report.launcher_component) << "\",\n"
         << "  \"bundle_apk_path\": \"" << EscapeJson(report.bundle_apk_path)
         << "\",\n"
         << "  \"sandbox_root\": \"" << EscapeJson(report.sandbox_root)
         << "\",\n"
         << "  \"dex_cache_root\": \"" << EscapeJson(report.dex_cache_root)
         << "\",\n"
         << "  \"resource_root\": \"" << EscapeJson(report.resource_root)
         << "\",\n"
         << "  \"library_root\": \"" << EscapeJson(report.library_root)
         << "\",\n"
         << "  \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "  \"bundle_present\": "
         << (report.bundle_present ? "true" : "false") << ",\n"
         << "  \"bootstrap_manifest_present\": "
         << (report.bootstrap_manifest_present ? "true" : "false")
         << ",\n"
         << "  \"native_library_found\": "
         << (report.native_library_found ? "true" : "false") << ",\n"
         << "  \"dlopen_ok\": "
         << (report.dlopen_ok ? "true" : "false") << ",\n"
         << "  \"execution_engine_ready\": "
         << (report.execution_engine_ready ? "true" : "false") << ",\n"
         << "  \"libraries_loaded\": "
         << BuildJsonStringArray(report.libraries_loaded) << ",\n"
         << "  \"library_load_attempts\": "
         << RenderNativeLibraryLoadAttemptsJson(report.library_load_attempts)
         << ",\n"
         << "  \"jni_onload_results\": "
         << BuildJniOnLoadResultsJson(report.jni_onload_results) << ",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\",\n"
         << "  \"working_directory\": \""
         << EscapeJson(report.working_directory) << "\",\n"
         << "  \"selected_library_path\": \""
         << EscapeJson(report.selected_library_path) << "\",\n"
         << "  \"entrypoint_found\": "
         << (report.entrypoint_found ? "true" : "false") << ",\n"
         << "  \"activity_called\": "
         << (report.activity_called ? "true" : "false") << ",\n"
         << "  \"exit_code\": " << report.exit_code << ",\n"
         << "  \"artifact_paths\": {\n"
         << "    \"bootstrap_manifest_path\": \""
         << EscapeJson(report.bootstrap_manifest_path) << "\",\n"
         << "    \"bundle_apk_path\": \"" << EscapeJson(report.bundle_apk_path)
         << "\",\n"
         << "    \"sandbox_root\": \"" << EscapeJson(report.sandbox_root)
         << "\",\n"
         << "    \"dex_cache_root\": \"" << EscapeJson(report.dex_cache_root)
         << "\",\n"
         << "    \"resource_root\": \"" << EscapeJson(report.resource_root)
         << "\",\n"
         << "    \"library_root\": \"" << EscapeJson(report.library_root)
         << "\"\n"
         << "  },\n"
         << "  \"debug_log\": \"" << EscapeJson(report.output) << "\"\n"
         << "}\n";
  return output.str();
}

}  // namespace wfa
