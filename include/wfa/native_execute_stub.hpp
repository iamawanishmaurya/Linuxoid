#ifndef WFA_NATIVE_EXECUTE_STUB_HPP
#define WFA_NATIVE_EXECUTE_STUB_HPP

#include <string>
#include <vector>

namespace wfa {

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
  std::string output;
};

std::vector<std::string> BuildNativeLibraryCandidates(
    const std::string& library_root);
NativeExecuteReport ExecuteNativeStub(const NativeExecuteRequest& request);

}  // namespace wfa

#endif  // WFA_NATIVE_EXECUTE_STUB_HPP
