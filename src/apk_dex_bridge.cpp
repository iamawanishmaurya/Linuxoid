#include "wfa/apk_dex_bridge.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

bool IsDexArchiveEntry(const std::string& path) {
  if (path.size() < std::string("classes.dex").size()) {
    return false;
  }
  if (path.rfind("classes", 0) != 0) {
    return false;
  }
  return path.substr(path.size() - 4) == ".dex";
}

bool IsSafeArchivePath(const std::string& archive_path) {
  if (archive_path.empty() || archive_path.front() == '/') {
    return false;
  }

  std::stringstream stream(archive_path);
  std::string segment;
  while (std::getline(stream, segment, '/')) {
    if (segment.empty() || segment == "." || segment == "..") {
      return false;
    }
  }
  return true;
}

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
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

void AppendUnique(std::vector<std::string>* values, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(value);
  }
}

std::string ComputeFnv1a64Checksum(const std::string& contents) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const unsigned char byte : contents) {
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= 1099511628211ull;
  }

  std::ostringstream output;
  output << "fnv1a64:" << std::hex << std::nouppercase << std::setfill('0')
         << std::setw(16) << hash;
  return output.str();
}

std::uint32_t ReadLe32(const std::string& bytes, std::size_t offset) {
  if (offset + 4 > bytes.size()) {
    return 0;
  }
  return static_cast<std::uint32_t>(
             static_cast<unsigned char>(bytes[offset + 0])) |
         (static_cast<std::uint32_t>(
              static_cast<unsigned char>(bytes[offset + 1]))
          << 8u) |
         (static_cast<std::uint32_t>(
              static_cast<unsigned char>(bytes[offset + 2]))
          << 16u) |
         (static_cast<std::uint32_t>(
              static_cast<unsigned char>(bytes[offset + 3]))
          << 24u);
}

std::uint16_t ReadLe16(const std::string& bytes, std::size_t offset) {
  if (offset + 2 > bytes.size()) {
    return 0;
  }
  return static_cast<std::uint16_t>(
             static_cast<unsigned char>(bytes[offset + 0])) |
         static_cast<std::uint16_t>(
             static_cast<unsigned char>(bytes[offset + 1]))
             << 8u;
}

bool ReadUleb128(const std::string& bytes, std::size_t* offset,
                 std::uint32_t* value) {
  if (offset == nullptr || value == nullptr) {
    return false;
  }
  std::uint32_t result = 0;
  std::uint32_t shift = 0;
  for (int index = 0; index < 5; ++index) {
    if (*offset >= bytes.size()) {
      return false;
    }
    const unsigned char byte =
        static_cast<unsigned char>(bytes[*offset]);
    ++(*offset);
    result |= static_cast<std::uint32_t>(byte & 0x7Fu) << shift;
    if ((byte & 0x80u) == 0) {
      *value = result;
      return true;
    }
    shift += 7u;
  }
  return false;
}

std::string ReadDexString(const std::string& bytes, std::uint32_t offset,
                          bool* ok) {
  if (ok != nullptr) {
    *ok = false;
  }
  std::size_t cursor = offset;
  std::uint32_t utf16_length = 0;
  if (!ReadUleb128(bytes, &cursor, &utf16_length) || cursor > bytes.size()) {
    return "";
  }

  std::string value;
  while (cursor < bytes.size() && bytes[cursor] != '\0') {
    value.push_back(bytes[cursor]);
    ++cursor;
  }
  if (cursor >= bytes.size()) {
    return "";
  }
  if (ok != nullptr) {
    *ok = value.size() == utf16_length;
  }
  return value;
}

struct DexProtoId {
  std::uint32_t shorty_idx = 0;
  std::uint32_t return_type_idx = 0;
  std::uint32_t parameters_off = 0;
};

struct DexMethodId {
  std::uint16_t class_idx = 0;
  std::uint16_t proto_idx = 0;
  std::uint32_t name_idx = 0;
};

struct DexClassDef {
  std::uint32_t class_idx = 0;
  std::uint32_t class_data_off = 0;
};

struct DexEncodedMethod {
  std::uint32_t method_index = 0;
  std::uint32_t access_flags = 0;
  std::uint32_t code_off = 0;
};

struct DexExecutionCandidate {
  std::string class_descriptor;
  std::string method_name;
  std::string method_signature = "()V";
  std::uint32_t code_off = 0;
};

struct ParsedDexTables {
  std::vector<std::string> strings;
  std::vector<std::uint32_t> type_descriptor_string_indices;
  std::vector<DexProtoId> protos;
  std::vector<DexMethodId> methods;
  std::vector<DexClassDef> class_defs;
  std::vector<std::string> class_descriptors;
};

std::string BuildMethodSignature(const ParsedDexTables& tables,
                                 const DexMethodId& method) {
  if (method.proto_idx >= tables.protos.size()) {
    return "()?";
  }
  const auto& proto = tables.protos[method.proto_idx];
  if (proto.return_type_idx >= tables.type_descriptor_string_indices.size()) {
    return "()?";
  }
  const std::uint32_t return_string_index =
      tables.type_descriptor_string_indices[proto.return_type_idx];
  if (return_string_index >= tables.strings.size()) {
    return "()?";
  }
  return "()" + tables.strings[return_string_index];
}

bool ParseDexTables(const std::string& bytes, NativeApkDexFileReport* file,
                    ParsedDexTables* tables) {
  if (file == nullptr || tables == nullptr) {
    return false;
  }

  const std::uint32_t string_ids_off = ReadLe32(bytes, 60);
  const std::uint32_t type_ids_off = ReadLe32(bytes, 68);
  const std::uint32_t proto_ids_size = ReadLe32(bytes, 72);
  const std::uint32_t proto_ids_off = ReadLe32(bytes, 76);
  const std::uint32_t method_ids_size = ReadLe32(bytes, 88);
  const std::uint32_t method_ids_off = ReadLe32(bytes, 92);
  const std::uint32_t class_defs_off = ReadLe32(bytes, 100);

  file->proto_ids_size = proto_ids_size;
  file->method_ids_size = method_ids_size;

  if (string_ids_off + file->string_ids_size * 4u > bytes.size() ||
      type_ids_off + file->type_ids_size * 4u > bytes.size() ||
      proto_ids_off + proto_ids_size * 12u > bytes.size() ||
      method_ids_off + method_ids_size * 8u > bytes.size() ||
      class_defs_off + file->class_defs_count * 32u > bytes.size()) {
    file->errors.push_back("dex_table_bounds_invalid");
    return false;
  }

  tables->strings.clear();
  tables->type_descriptor_string_indices.clear();
  tables->protos.clear();
  tables->methods.clear();
  tables->class_defs.clear();
  tables->class_descriptors.clear();

  for (std::uint32_t index = 0; index < file->string_ids_size; ++index) {
    const std::uint32_t string_data_off =
        ReadLe32(bytes, string_ids_off + index * 4u);
    bool string_ok = false;
    const std::string value = ReadDexString(bytes, string_data_off, &string_ok);
    if (!string_ok) {
      file->errors.push_back("dex_string_data_invalid");
      return false;
    }
    tables->strings.push_back(value);
  }

  for (std::uint32_t index = 0; index < file->type_ids_size; ++index) {
    const std::uint32_t descriptor_idx =
        ReadLe32(bytes, type_ids_off + index * 4u);
    if (descriptor_idx >= tables->strings.size()) {
      file->errors.push_back("dex_type_descriptor_invalid");
      return false;
    }
    tables->type_descriptor_string_indices.push_back(descriptor_idx);
  }

  for (std::uint32_t index = 0; index < proto_ids_size; ++index) {
    const std::size_t offset = proto_ids_off + index * 12u;
    tables->protos.push_back(
        {.shorty_idx = ReadLe32(bytes, offset + 0u),
         .return_type_idx = ReadLe32(bytes, offset + 4u),
         .parameters_off = ReadLe32(bytes, offset + 8u)});
  }

  for (std::uint32_t index = 0; index < method_ids_size; ++index) {
    const std::size_t offset = method_ids_off + index * 8u;
    tables->methods.push_back(
        {.class_idx = ReadLe16(bytes, offset + 0u),
         .proto_idx = ReadLe16(bytes, offset + 2u),
         .name_idx = ReadLe32(bytes, offset + 4u)});
  }

  for (std::uint32_t index = 0; index < file->class_defs_count; ++index) {
    const std::size_t offset = class_defs_off + index * 32u;
    tables->class_defs.push_back(
        {.class_idx = ReadLe32(bytes, offset + 0u),
         .class_data_off = ReadLe32(bytes, offset + 24u)});
  }

  for (const auto& class_def : tables->class_defs) {
    if (class_def.class_idx >= tables->type_descriptor_string_indices.size()) {
      file->errors.push_back("dex_class_descriptor_invalid");
      return false;
    }
    const std::uint32_t string_index =
        tables->type_descriptor_string_indices[class_def.class_idx];
    if (string_index >= tables->strings.size()) {
      file->errors.push_back("dex_class_string_invalid");
      return false;
    }
    tables->class_descriptors.push_back(tables->strings[string_index]);
  }

  file->class_descriptors = tables->class_descriptors;
  return true;
}

bool FindEntrypointMethod(const std::string& bytes, const ParsedDexTables& tables,
                          const std::string& target_class_descriptor,
                          const std::string& target_method_name,
                          DexExecutionCandidate* candidate,
                          std::vector<std::string>* errors) {
  if (candidate == nullptr || errors == nullptr) {
    return false;
  }

  auto append_error = [&](const std::string& value) {
    AppendUnique(errors, value);
  };

  for (const auto& class_def : tables.class_defs) {
    if (class_def.class_idx >= tables.type_descriptor_string_indices.size()) {
      append_error("dex_entrypoint_class_index_invalid");
      return false;
    }
    const std::uint32_t string_index =
        tables.type_descriptor_string_indices[class_def.class_idx];
    if (string_index >= tables.strings.size()) {
      append_error("dex_entrypoint_class_string_invalid");
      return false;
    }
    const std::string class_descriptor = tables.strings[string_index];
    if (class_descriptor != target_class_descriptor) {
      continue;
    }
    if (class_def.class_data_off == 0) {
      append_error("dex_entrypoint_class_data_missing");
      return false;
    }
    std::size_t cursor = class_def.class_data_off;
    std::uint32_t static_fields_size = 0;
    std::uint32_t instance_fields_size = 0;
    std::uint32_t direct_methods_size = 0;
    std::uint32_t virtual_methods_size = 0;
    if (!ReadUleb128(bytes, &cursor, &static_fields_size) ||
        !ReadUleb128(bytes, &cursor, &instance_fields_size) ||
        !ReadUleb128(bytes, &cursor, &direct_methods_size) ||
        !ReadUleb128(bytes, &cursor, &virtual_methods_size)) {
      append_error("dex_class_data_truncated");
      return false;
    }

    for (std::uint32_t index = 0; index < static_fields_size + instance_fields_size;
         ++index) {
      std::uint32_t ignored = 0;
      if (!ReadUleb128(bytes, &cursor, &ignored) ||
          !ReadUleb128(bytes, &cursor, &ignored)) {
        append_error("dex_field_data_truncated");
        return false;
      }
    }

    auto scan_methods = [&](std::uint32_t methods_size) -> bool {
      std::uint32_t previous_method_index = 0;
      for (std::uint32_t index = 0; index < methods_size; ++index) {
        std::uint32_t method_idx_diff = 0;
        std::uint32_t access_flags = 0;
        std::uint32_t code_off = 0;
        if (!ReadUleb128(bytes, &cursor, &method_idx_diff) ||
            !ReadUleb128(bytes, &cursor, &access_flags) ||
            !ReadUleb128(bytes, &cursor, &code_off)) {
          append_error("dex_method_data_truncated");
          return false;
        }
        previous_method_index += method_idx_diff;
        if (previous_method_index >= tables.methods.size()) {
          append_error("dex_method_index_invalid");
          return false;
        }
        const auto& method = tables.methods[previous_method_index];
        if (method.name_idx >= tables.strings.size()) {
          append_error("dex_method_name_invalid");
          return false;
        }
        if (tables.strings[method.name_idx] != target_method_name) {
          continue;
        }
        *candidate = {.class_descriptor = class_descriptor,
                      .method_name = tables.strings[method.name_idx],
                      .method_signature = BuildMethodSignature(tables, method),
                      .code_off = code_off};
        (void)access_flags;
        return true;
      }
      return false;
    };

    if (scan_methods(direct_methods_size) || scan_methods(virtual_methods_size)) {
      return true;
    }
    append_error("dex_entrypoint_method_missing");
    return false;
  }

  append_error("dex_entrypoint_class_missing");
  return false;
}

std::string DescribeOpcode(std::uint16_t opcode) {
  switch (opcode) {
    case 0x00:
      return "nop";
    case 0x0e:
      return "return-void";
    case 0x0f:
      return "return";
    case 0x10:
      return "return-wide";
    case 0x11:
      return "return-object";
    case 0x12:
      return "const/4";
    default:
      break;
  }
  std::ostringstream output;
  output << "opcode-0x" << std::hex << std::nouppercase << std::setfill('0')
         << std::setw(2) << opcode;
  return output.str();
}

NativeApkDexExecutionProbeReport RunExecutionProbe(
    const std::string& bytes, const NativeApkDexBridgeSessionConfig& config,
    const NativeApkDexFileReport& file, const ParsedDexTables& tables) {
  NativeApkDexExecutionProbeReport probe;
  probe.target_class_descriptor = config.entrypoint_class_descriptor;
  probe.target_method_name = config.entrypoint_method_name;
  probe.parse_state = "header_tables_methods_and_code_item";

  if (!file.valid_dex_magic) {
    probe.execution_state = "dex_magic_invalid";
    probe.exact_blocker = "dex_magic_invalid";
    probe.errors.push_back("dex_magic_invalid");
    return probe;
  }
  if (probe.target_class_descriptor.empty()) {
    probe.execution_state = "entrypoint_class_unknown";
    probe.exact_blocker = "dex_entrypoint_class_unknown";
    probe.errors.push_back("dex_entrypoint_class_unknown");
    return probe;
  }

  DexExecutionCandidate candidate;
  if (!FindEntrypointMethod(bytes, tables, probe.target_class_descriptor,
                            probe.target_method_name, &candidate,
                            &probe.errors)) {
    probe.execution_state = "entrypoint_missing";
    probe.exact_blocker = probe.errors.empty() ? "dex_entrypoint_missing"
                                               : probe.errors.front();
    return probe;
  }

  probe.target_method_found = true;
  probe.target_method_signature = candidate.method_signature;
  probe.code_item_found = candidate.code_off != 0;
  probe.code_item_offset = candidate.code_off;
  if (candidate.code_off == 0) {
    probe.execution_state = "code_item_missing";
    probe.exact_blocker = "dex_entrypoint_code_item_missing";
    probe.errors.push_back("dex_entrypoint_code_item_missing");
    return probe;
  }

  if (candidate.code_off + 16u > bytes.size()) {
    probe.execution_state = "code_item_truncated";
    probe.exact_blocker = "dex_code_item_truncated";
    probe.errors.push_back("dex_code_item_truncated");
    return probe;
  }

  const std::uint32_t insns_size = ReadLe32(bytes, candidate.code_off + 12u);
  const std::size_t insns_off = static_cast<std::size_t>(candidate.code_off) + 16u;
  if (insns_off + static_cast<std::size_t>(insns_size) * 2u > bytes.size()) {
    probe.execution_state = "instructions_truncated";
    probe.exact_blocker = "dex_instructions_truncated";
    probe.errors.push_back("dex_instructions_truncated");
    return probe;
  }

  probe.execution_attempted = true;
  probe.ready = true;
  probe.execution_state = "interpreting";

  std::uint32_t pc = 0;
  while (pc < insns_size) {
    const std::uint16_t code_unit = ReadLe16(bytes, insns_off + pc * 2u);
    const std::uint16_t opcode = static_cast<std::uint16_t>(code_unit & 0x00ffu);
    const std::string opcode_name = DescribeOpcode(opcode);
    if (!probe.decoded_instruction) {
      probe.instruction_offset = pc * 2u;
      probe.opcode_value = opcode;
      probe.opcode_name = opcode_name;
      probe.decoded_instruction = true;
    }
    ++probe.executed_instruction_count;

    switch (opcode) {
      case 0x00:  // nop
      case 0x12:  // const/4
        ++pc;
        continue;
      case 0x0e:  // return-void
      case 0x0f:  // return
      case 0x10:  // return-wide
      case 0x11:  // return-object
        probe.reached_return = true;
        probe.execution_state = "returned";
        probe.exact_blocker = "none";
        probe.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed a real bytecode instruction path");
        return probe;
      default:
        probe.execution_state = "unsupported_opcode";
        probe.exact_blocker = "unsupported-dex-opcode:" + opcode_name;
        probe.diagnostics.push_back(
            "Self-Healing Android Device DEX probe reached an exact unsupported opcode boundary");
        return probe;
    }
  }

  probe.execution_state = "fell_off_end";
  probe.exact_blocker = "dex_execution_fell_off_end";
  probe.errors.push_back("dex_execution_fell_off_end");
  return probe;
}

std::string RenderDexExecutionProbeJson(
    const NativeApkDexExecutionProbeReport& probe) {
  std::ostringstream output;
  output << "{\n"
         << "    \"ready\": " << (probe.ready ? "true" : "false") << ",\n"
         << "    \"target_method_found\": "
         << (probe.target_method_found ? "true" : "false") << ",\n"
         << "    \"code_item_found\": "
         << (probe.code_item_found ? "true" : "false") << ",\n"
         << "    \"execution_attempted\": "
         << (probe.execution_attempted ? "true" : "false") << ",\n"
         << "    \"decoded_instruction\": "
         << (probe.decoded_instruction ? "true" : "false") << ",\n"
         << "    \"reached_return\": "
         << (probe.reached_return ? "true" : "false") << ",\n"
         << "    \"target_class_descriptor\": \""
         << EscapeJson(probe.target_class_descriptor) << "\",\n"
         << "    \"target_method_name\": \""
         << EscapeJson(probe.target_method_name) << "\",\n"
         << "    \"target_method_signature\": \""
         << EscapeJson(probe.target_method_signature) << "\",\n"
         << "    \"execution_backend\": \""
         << EscapeJson(probe.execution_backend) << "\",\n"
         << "    \"parse_state\": \"" << EscapeJson(probe.parse_state)
         << "\",\n"
         << "    \"execution_state\": \"" << EscapeJson(probe.execution_state)
         << "\",\n"
         << "    \"code_item_offset\": " << probe.code_item_offset << ",\n"
         << "    \"instruction_offset\": " << probe.instruction_offset
         << ",\n"
         << "    \"opcode_value\": " << probe.opcode_value << ",\n"
         << "    \"opcode_name\": \"" << EscapeJson(probe.opcode_name)
         << "\",\n"
         << "    \"executed_instruction_count\": "
         << probe.executed_instruction_count << ",\n"
         << "    \"exact_blocker\": \""
         << EscapeJson(probe.exact_blocker) << "\",\n"
         << "    \"diagnostics\": " << RenderJsonArray(probe.diagnostics)
         << ",\n"
         << "    \"errors\": " << RenderJsonArray(probe.errors) << "\n"
         << "  }";
  return output.str();
}

std::string RenderDexProofArtifactJson(const NativeApkDexProofReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id)
         << "\",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir)
         << "\",\n"
         << "  \"dex_root\": \"" << EscapeJson(report.dex_root) << "\",\n"
         << "  \"files_count\": " << report.files_count << ",\n"
         << "  \"total_bytes\": " << report.total_bytes << ",\n"
         << "  \"decode_level\": \"" << EscapeJson(report.decode_level)
         << "\",\n"
         << "  \"parse_state\": \"" << EscapeJson(report.parse_state)
         << "\",\n"
         << "  \"class_defs_count\": " << report.class_defs_count << ",\n"
         << "  \"files\": [\n";
  for (std::size_t index = 0; index < report.files.size(); ++index) {
    const auto& file = report.files[index];
    output << "    {\n"
           << "      \"entry_name\": \"" << EscapeJson(file.entry_name)
           << "\",\n"
           << "      \"staged_path\": \"" << EscapeJson(file.staged_path)
           << "\",\n"
           << "      \"size_bytes\": " << file.size_bytes << ",\n"
           << "      \"checksum\": \"" << EscapeJson(file.checksum)
           << "\",\n"
           << "      \"valid_dex_magic\": "
           << (file.valid_dex_magic ? "true" : "false") << ",\n"
           << "      \"dex_version\": \"" << EscapeJson(file.dex_version)
           << "\",\n"
           << "      \"dex_file_size\": " << file.dex_file_size << ",\n"
           << "      \"header_size\": " << file.header_size << ",\n"
           << "      \"string_ids_size\": " << file.string_ids_size << ",\n"
           << "      \"type_ids_size\": " << file.type_ids_size << ",\n"
           << "      \"proto_ids_size\": " << file.proto_ids_size << ",\n"
           << "      \"method_ids_size\": " << file.method_ids_size << ",\n"
           << "      \"class_defs_count\": " << file.class_defs_count
           << ",\n"
           << "      \"class_descriptors\": "
           << RenderJsonArray(file.class_descriptors) << ",\n"
           << "      \"errors\": " << RenderJsonArray(file.errors) << "\n"
           << "    }";
    if (index + 1 != report.files.size()) {
      output << ",";
    }
    output << "\n";
  }
  output << "  ],\n"
         << "  \"execution_probe\": "
         << RenderDexExecutionProbeJson(report.execution_probe) << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

std::string RenderArtBootstrapArtifactJson(
    const NativeApkArtBootstrapReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id)
         << "\",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir)
         << "\",\n"
         << "  \"asset_bridge_status\": \""
         << EscapeJson(report.asset_bridge_status) << "\",\n"
         << "  \"lifecycle_status\": \""
         << EscapeJson(report.lifecycle_status) << "\",\n"
         << "  \"binder_service_registry_status\": \""
         << EscapeJson(report.binder_service_registry_status) << "\",\n"
         << "  \"art_runtime_required\": "
         << (report.art_runtime_required ? "true" : "false") << ",\n"
         << "  \"art_runtime_available\": "
         << (report.art_runtime_available ? "true" : "false") << ",\n"
         << "  \"dex_bootstrap_ready\": "
         << (report.dex_bootstrap_ready ? "true" : "false") << ",\n"
         << "  \"class_loader_ready\": "
         << (report.class_loader_ready ? "true" : "false") << ",\n"
         << "  \"java_execution_supported\": "
         << (report.java_execution_supported ? "true" : "false") << ",\n"
         << "  \"limitation\": \"" << EscapeJson(report.limitation)
         << "\",\n"
         << "  \"dex_files\": " << RenderJsonArray(report.dex_files) << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

}  // namespace

NativeApkDexBridgeSession::NativeApkDexBridgeSession(
    NativeApkDexBridgeSessionConfig config)
    : config_(std::move(config)) {}

NativeApkDexProofReport NativeApkDexBridgeSession::RunDexProof(
    const OpenedApkArchive& archive) const {
  NativeApkDexProofReport report;
  report.session_id = config_.session_id;
  report.package_name = config_.package_name;
  report.apk_path = config_.apk_path;
  report.staged_dir = config_.staged_dir;
  report.dex_root = config_.dex_root;
  report.artifact_root = config_.artifact_root;
  report.inventory_json_path =
      (fs::path(config_.artifact_root) / "dex-proof.json").string();
  report.decode_level = "header_tables_methods_and_code_item";
  report.parse_state = "header_validated";
  report.execution_probe.target_class_descriptor =
      config_.entrypoint_class_descriptor;
  report.execution_probe.target_method_name = config_.entrypoint_method_name;

  fs::create_directories(config_.dex_root);
  fs::create_directories(config_.artifact_root);

  std::vector<ApkArchiveEntry> dex_entries;
  for (const auto& entry : ListApkArchiveEntries(archive)) {
    if (!entry.is_directory && IsDexArchiveEntry(entry.path)) {
      dex_entries.push_back(entry);
    }
  }

  std::sort(dex_entries.begin(), dex_entries.end(),
            [](const ApkArchiveEntry& left, const ApkArchiveEntry& right) {
              return left.path < right.path;
            });

  if (dex_entries.empty()) {
    report.errors.push_back("no_dex_entries_found");
    report.parse_state = "no_dex_entries_found";
    WriteTextFile(report.inventory_json_path, RenderDexProofArtifactJson(report));
    return report;
  }

  bool execution_probe_resolved = false;
  for (const auto& entry : dex_entries) {
    NativeApkDexFileReport file;
    file.entry_name = entry.path;
    if (!IsSafeArchivePath(entry.path)) {
      file.errors.push_back("unsafe_dex_entry");
      AppendUnique(&report.errors, "unsafe_dex_entry:" + entry.path);
      report.files.push_back(file);
      continue;
    }

    const auto read_result = ReadApkArchiveEntry(archive, entry.path);
    if (!read_result.found) {
      file.errors.push_back("dex_entry_missing");
      AppendUnique(&report.errors, "dex_entry_missing:" + entry.path);
      report.files.push_back(file);
      continue;
    }
    if (!read_result.readable) {
      file.errors.push_back("dex_entry_unreadable:" + read_result.failure_reason);
      AppendUnique(&report.errors,
                   "dex_entry_unreadable:" + entry.path + ":" +
                       read_result.failure_reason);
      report.files.push_back(file);
      continue;
    }

    const fs::path staged_path =
        fs::path(config_.dex_root) / fs::path(entry.path).filename();
    fs::create_directories(staged_path.parent_path());
    {
      std::ofstream output(staged_path, std::ios::binary);
      if (!output) {
        file.errors.push_back("dex_stage_write_failed");
        AppendUnique(&report.errors, "dex_stage_write_failed:" + entry.path);
        report.files.push_back(file);
        continue;
      }
      output.write(read_result.contents.data(),
                   static_cast<std::streamsize>(read_result.contents.size()));
      if (!output.good()) {
        file.errors.push_back("dex_stage_write_failed");
        AppendUnique(&report.errors, "dex_stage_write_failed:" + entry.path);
        report.files.push_back(file);
        continue;
      }
    }

    file.staged_path = staged_path.string();
    file.size_bytes = read_result.contents.size();
    file.checksum = ComputeFnv1a64Checksum(read_result.contents);
    report.total_bytes += file.size_bytes;

    if (read_result.contents.size() < 0x70) {
      file.errors.push_back("dex_header_truncated");
      AppendUnique(&report.errors, "dex_header_truncated:" + entry.path);
      report.files.push_back(file);
      continue;
    }

    if (read_result.contents[0] != 'd' || read_result.contents[1] != 'e' ||
        read_result.contents[2] != 'x' || read_result.contents[3] != '\n' ||
        read_result.contents[7] != '\0') {
      file.errors.push_back("dex_magic_invalid");
      AppendUnique(&report.errors, "dex_magic_invalid:" + entry.path);
      report.files.push_back(file);
      continue;
    }

    file.valid_dex_magic = true;
    file.dex_version.assign(read_result.contents.data() + 4, 3);
    file.dex_file_size = ReadLe32(read_result.contents, 32);
    file.header_size = ReadLe32(read_result.contents, 36);
    file.string_ids_size = ReadLe32(read_result.contents, 56);
    file.type_ids_size = ReadLe32(read_result.contents, 64);
    file.class_defs_count = ReadLe32(read_result.contents, 96);

    ParsedDexTables tables;
    if (!ParseDexTables(read_result.contents, &file, &tables)) {
      for (const auto& error : file.errors) {
        AppendUnique(&report.errors, error + ":" + entry.path);
      }
      report.files.push_back(file);
      continue;
    }

    if (!execution_probe_resolved) {
      const auto probe =
          RunExecutionProbe(read_result.contents, config_, file, tables);
      if (probe.target_method_found || probe.exact_blocker != "dex_entrypoint_class_missing") {
        report.execution_probe = probe;
        execution_probe_resolved = true;
        report.parse_state = probe.parse_state;
      }
    }

    report.class_defs_count += file.class_defs_count;
    report.files.push_back(file);
  }

  if (!execution_probe_resolved) {
    report.execution_probe.parse_state = "entrypoint_lookup_unresolved";
    report.execution_probe.execution_state = "entrypoint_missing";
    report.execution_probe.exact_blocker = "dex_entrypoint_class_missing";
    report.execution_probe.errors.push_back("dex_entrypoint_class_missing");
    report.parse_state = "entrypoint_lookup_unresolved";
  }

  report.files_count = static_cast<int>(report.files.size());
  report.ready = report.files_count > 0 && report.errors.empty();
  WriteTextFile(report.inventory_json_path, RenderDexProofArtifactJson(report));
  return report;
}

NativeApkArtBootstrapReport NativeApkDexBridgeSession::BuildArtBootstrap(
    const NativeApkDexProofReport& dex_report) const {
  NativeApkArtBootstrapReport report;
  report.session_id = config_.session_id;
  report.package_name = config_.package_name;
  report.apk_path = config_.apk_path;
  report.staged_dir = config_.staged_dir;
  report.artifact_root = config_.artifact_root;
  report.bootstrap_json_path =
      (fs::path(config_.artifact_root) / "art-bootstrap.json").string();
  report.asset_bridge_status = config_.asset_bridge_status;
  report.lifecycle_status = config_.lifecycle_status;
  report.binder_service_registry_status =
      config_.binder_service_registry_status;
  report.dex_bootstrap_ready = dex_report.ready;
  report.class_loader_ready = dex_report.ready;
  report.ready = dex_report.ready;
  for (const auto& file : dex_report.files) {
    if (!file.staged_path.empty()) {
      report.dex_files.push_back(file.staged_path);
    }
  }
  report.errors = dex_report.errors;
  fs::create_directories(config_.artifact_root);
  WriteTextFile(report.bootstrap_json_path,
                RenderArtBootstrapArtifactJson(report));
  return report;
}

}  // namespace wfa
