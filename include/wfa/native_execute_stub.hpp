#ifndef WFA_NATIVE_EXECUTE_STUB_HPP
#define WFA_NATIVE_EXECUTE_STUB_HPP

#include <string>
#include <vector>

namespace wfa {

struct NativeLibraryLoadAttempt {
  std::string library_path;
  std::string library_name;
  int candidate_index = -1;
  std::string load_state = "not_attempted";
  std::string jni_state = "not_attempted";
  std::string entrypoint_state = "not_checked";
  int jni_return_code = 0;
  std::string failure_reason;
  std::string error_detail;
};

struct JniOnLoadResult {
  std::string library_path;
  bool symbol_present = false;
  bool call_succeeded = false;
  int return_code = 0;
  std::string status;
  std::string error_detail;
};

struct NativeExecuteRequest {
  std::string package_name;
  std::string launcher_component;
  std::string bundle_apk_path;
  std::string sandbox_root;
  std::string dex_cache_root;
  std::string resource_root;
  std::string library_root;
  std::string bootstrap_manifest_path;
  int watchdog_seconds = 5;
};

struct NativeExecuteReport {
  std::string package_name;
  std::string launcher_component;
  std::string bundle_apk_path;
  std::string sandbox_root;
  std::string dex_cache_root;
  std::string resource_root;
  std::string library_root;
  std::string bootstrap_manifest_path;
  bool bundle_present = false;
  bool bootstrap_manifest_present = false;
  bool native_library_found = false;
  bool dlopen_ok = false;
  bool entrypoint_found = false;
  bool activity_called = false;
  bool execution_engine_ready = false;
  int exit_code = 1;
  std::string selected_library_path;
  std::vector<std::string> candidate_library_paths;
  std::vector<std::string> libraries_loaded;
  std::vector<NativeLibraryLoadAttempt> library_load_attempts;
  std::vector<JniOnLoadResult> jni_onload_results;
  std::string app_start_bridge_state = "not_applicable";
  std::string app_start_bridge_reason = "none";
  std::string registration_dispatch_state = "not_applicable";
  std::string registration_dispatch_symbol_kind = "none";
  std::string registration_dispatch_symbol;
  std::string registration_outcome_state = "not_applicable";
  std::string registration_outcome_reason = "none";
  std::string registration_class_name;
  int registration_method_count = 0;
  std::string post_jni_startup_state = "not_applicable";
  std::string post_jni_dispatch_symbol_kind = "none";
  std::string post_jni_dispatch_symbol;
  std::string post_jni_dispatch_reason = "none";
  std::string managed_activity_dispatch_state = "not_applicable";
  std::string managed_activity_dispatch_reason = "none";
  std::string managed_activity_dispatch_component;
  std::string managed_activity_dispatch_class_name;
  std::string managed_activity_dispatch_class_descriptor;
  std::string managed_activity_dispatch_method_name;
  std::string managed_activity_dispatch_method_signature;
  std::string managed_activity_runtime_binding_state = "not_applicable";
  std::string managed_activity_runtime_binding_reason = "none";
  std::string managed_activity_runtime_context_id;
  std::string managed_activity_runtime_context_kind = "not_applicable";
  std::string android_compat_state;
  int elf_undefined_versions_normalized = 0;
  std::vector<std::string> android_compat_preloaded_paths;
  std::vector<std::string> android_compat_diagnostics;
  std::string working_directory;
  std::string exit_reason;
  std::string output;
};

std::vector<std::string> BuildNativeLibraryCandidates(
    const std::string& library_root);
std::string RenderNativeLibraryLoadAttemptsJson(
    const std::vector<NativeLibraryLoadAttempt>& attempts);
NativeExecuteReport ExecuteNativeStub(const NativeExecuteRequest& request);
std::string RenderNativeExecuteReportJson(const NativeExecuteReport& report);

}  // namespace wfa

#endif  // WFA_NATIVE_EXECUTE_STUB_HPP
