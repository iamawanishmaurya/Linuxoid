#ifndef WFA_APK_DEX_BRIDGE_HPP
#define WFA_APK_DEX_BRIDGE_HPP

#include "wfa/apk_archive.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wfa {

struct NativeApkDexFileReport {
  std::string entry_name;
  std::string staged_path;
  std::size_t size_bytes = 0;
  std::string checksum;
  bool valid_dex_magic = false;
  std::string dex_version;
  std::uint32_t dex_file_size = 0;
  std::uint32_t header_size = 0;
  std::uint32_t string_ids_size = 0;
  std::uint32_t type_ids_size = 0;
  std::uint32_t proto_ids_size = 0;
  std::uint32_t method_ids_size = 0;
  std::uint32_t class_defs_count = 0;
  std::vector<std::string> class_descriptors;
  std::vector<std::string> errors;
};

struct NativeApkDexExecutionProbeReport {
  bool ready = false;
  bool target_method_found = false;
  bool code_item_found = false;
  bool execution_attempted = false;
  bool decoded_instruction = false;
  bool reached_return = false;
  std::string target_class_descriptor;
  std::string target_method_name = "linuxoidCheckpoint";
  std::string target_method_signature = "()V";
  std::string execution_backend = "linuxoid_minimal_dex_interpreter";
  std::string parse_state = "not_requested";
  std::string execution_state = "not_attempted";
  std::string invoked_method_class_descriptor;
  std::string invoked_method_name;
  std::string invoked_method_signature;
  std::string framework_boundary_state = "not_reached";
  std::string framework_boundary_reason = "none";
  std::uint32_t code_item_offset = 0;
  std::uint32_t instruction_offset = 0;
  std::uint16_t opcode_value = 0;
  std::string opcode_name;
  std::uint32_t last_instruction_offset = 0;
  std::uint16_t last_opcode_value = 0;
  std::string last_opcode_name;
  int decoded_instruction_count = 0;
  int executed_instruction_count = 0;
  std::string returned_value_type;
  std::string returned_value;
  std::string exact_blocker = "none";
  std::vector<std::string> diagnostics;
  std::vector<std::string> errors;
};

struct NativeApkDexProofReport {
  bool ready = false;
  std::string session_id;
  std::string package_name;
  std::string apk_path;
  std::string staged_dir;
  std::string dex_root;
  std::string artifact_root;
  std::string inventory_json_path;
  int files_count = 0;
  std::size_t total_bytes = 0;
  std::string decode_level = "not_requested";
  std::string parse_state = "not_requested";
  std::uint32_t class_defs_count = 0;
  std::vector<NativeApkDexFileReport> files;
  NativeApkDexExecutionProbeReport execution_probe;
  std::vector<std::string> errors;
};

struct NativeApkArtBootstrapReport {
  bool ready = false;
  std::string session_id;
  std::string package_name;
  std::string apk_path;
  std::string staged_dir;
  std::string artifact_root;
  std::string bootstrap_json_path;
  std::string asset_bridge_status = "not_requested";
  std::string lifecycle_status = "not_requested";
  std::string binder_service_registry_status = "not_present";
  bool art_runtime_required = true;
  bool art_runtime_available = false;
  bool dex_bootstrap_ready = false;
  bool class_loader_ready = false;
  bool java_execution_supported = false;
  std::string limitation =
      "minimal dex parse and interpreter probe only; full ART execution not implemented";
  std::vector<std::string> dex_files;
  std::vector<std::string> errors;
};

struct NativeApkDexBridgeSessionConfig {
  std::string session_id;
  std::string package_name;
  std::string apk_path;
  std::string staged_dir;
  std::string dex_root;
  std::string artifact_root;
  std::string entrypoint_class_descriptor;
  std::string entrypoint_method_name = "linuxoidCheckpoint";
  std::string asset_bridge_status = "not_requested";
  std::string lifecycle_status = "not_requested";
  std::string binder_service_registry_status = "not_present";
};

class NativeApkDexBridgeSession {
 public:
  explicit NativeApkDexBridgeSession(NativeApkDexBridgeSessionConfig config);

  NativeApkDexProofReport RunDexProof(const OpenedApkArchive& archive) const;
  NativeApkArtBootstrapReport BuildArtBootstrap(
      const NativeApkDexProofReport& dex_report) const;

 private:
  NativeApkDexBridgeSessionConfig config_;
};

}  // namespace wfa

#endif  // WFA_APK_DEX_BRIDGE_HPP
