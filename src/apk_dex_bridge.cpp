#include "wfa/apk_dex_bridge.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
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

std::int32_t ReadSignedLe32(const std::string& bytes, std::size_t offset) {
  return static_cast<std::int32_t>(ReadLe32(bytes, offset));
}

bool ResolvePackedSwitchTarget(const std::string& bytes, std::uint32_t insns_off,
                               std::uint32_t insns_size,
                               std::uint32_t switch_pc,
                               std::int32_t switch_value,
                               std::uint32_t* next_pc,
                               std::string* failure_reason) {
  if (next_pc == nullptr || failure_reason == nullptr) {
    return false;
  }
  *failure_reason = "packed_switch_invalid_arguments";
  if (switch_pc + 2u >= insns_size) {
    *failure_reason = "packed_switch_truncated";
    return false;
  }

  const std::int32_t payload_offset = ReadSignedLe32(
      bytes, insns_off + static_cast<std::size_t>(switch_pc + 1u) * 2u);
  const std::int64_t payload_pc =
      static_cast<std::int64_t>(switch_pc) + payload_offset;
  if (payload_pc < 0 ||
      payload_pc >= static_cast<std::int64_t>(insns_size)) {
    *failure_reason = "packed_switch_payload_out_of_range";
    return false;
  }

  const std::size_t payload_off =
      insns_off + static_cast<std::size_t>(payload_pc) * 2u;
  if (payload_off + 8u > bytes.size()) {
    *failure_reason = "packed_switch_payload_truncated";
    return false;
  }

  const std::uint16_t payload_ident = ReadLe16(bytes, payload_off + 0u);
  if (payload_ident != 0x0100u) {
    *failure_reason = "packed_switch_payload_invalid";
    return false;
  }
  const std::uint16_t payload_size = ReadLe16(bytes, payload_off + 2u);
  const std::int32_t first_key = ReadSignedLe32(bytes, payload_off + 4u);
  const std::int64_t case_index =
      static_cast<std::int64_t>(switch_value) -
      static_cast<std::int64_t>(first_key);
  if (case_index < 0 ||
      case_index >= static_cast<std::int64_t>(payload_size)) {
    const std::uint32_t fallthrough_pc = switch_pc + 3u;
    if (fallthrough_pc > insns_size) {
      *failure_reason = "packed_switch_fallthrough_out_of_range";
      return false;
    }
    *next_pc = fallthrough_pc;
    *failure_reason = "none";
    return true;
  }

  const std::size_t target_off =
      payload_off + 8u + static_cast<std::size_t>(case_index) * 4u;
  if (target_off + 4u > bytes.size()) {
    *failure_reason = "packed_switch_targets_truncated";
    return false;
  }
  const std::int32_t branch_offset = ReadSignedLe32(bytes, target_off);
  const std::int64_t branch_target =
      static_cast<std::int64_t>(switch_pc) + branch_offset;
  if (branch_target < 0 ||
      branch_target >= static_cast<std::int64_t>(insns_size)) {
    *failure_reason = "packed_switch_branch_out_of_range";
    return false;
  }

  *next_pc = static_cast<std::uint32_t>(branch_target);
  *failure_reason = "none";
  return true;
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
  (void)utf16_length;
  if (ok != nullptr) {
    // DEX stores a UTF-16 code unit count ahead of a MUTF-8 payload, so the
    // byte length does not necessarily match the declared character length.
    // A null-terminated payload inside bounds is enough for this staged lookup
    // contract; exact Unicode semantics remain outside the current checkpoint.
    *ok = true;
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

struct DexFieldId {
  std::uint16_t class_idx = 0;
  std::uint16_t type_idx = 0;
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
  std::vector<std::string> proto_signatures;
  std::vector<DexFieldId> fields;
  std::vector<DexMethodId> methods;
  std::vector<std::uint32_t> method_code_offsets;
  std::vector<DexClassDef> class_defs;
  std::vector<std::string> class_descriptors;
};

struct DexRegisterValue {
  enum class Kind {
    kUnknown,
    kInt,
    kObject,
  };

  Kind kind = Kind::kUnknown;
  std::int32_t int_value = 0;
  std::uint32_t object_id = 0;
  std::string class_descriptor;
};

struct PlaceholderObject {
  std::uint32_t object_id = 0;
  std::string class_descriptor;
  std::int32_t array_length = -1;
  std::map<std::string, DexRegisterValue> fields;
  std::map<std::int32_t, DexRegisterValue> array_elements;
};

std::string BuildMethodSignature(const ParsedDexTables& tables,
                                 const DexMethodId& method) {
  if (method.proto_idx >= tables.protos.size()) {
    return "()?";
  }
  if (method.proto_idx >= tables.proto_signatures.size()) {
    return "()?";
  }
  return tables.proto_signatures[method.proto_idx];
}

std::string BuildProtoSignature(const std::string& bytes,
                                const ParsedDexTables& tables,
                                const DexProtoId& proto) {
  std::ostringstream signature;
  signature << "(";
  if (proto.parameters_off != 0) {
    if (proto.parameters_off + 4u > bytes.size()) {
      return "()?";
    }
    const std::uint32_t parameter_count =
        ReadLe32(bytes, proto.parameters_off);
    const std::size_t parameters_offset =
        static_cast<std::size_t>(proto.parameters_off) + 4u;
    if (parameters_offset + static_cast<std::size_t>(parameter_count) * 2u >
        bytes.size()) {
      return "()?";
    }
    for (std::uint32_t index = 0; index < parameter_count; ++index) {
      const std::uint16_t type_index =
          ReadLe16(bytes, parameters_offset + index * 2u);
      if (type_index >= tables.type_descriptor_string_indices.size()) {
        return "()?";
      }
      const std::uint32_t string_index =
          tables.type_descriptor_string_indices[type_index];
      if (string_index >= tables.strings.size()) {
        return "()?";
      }
      signature << tables.strings[string_index];
    }
  }
  signature << ")";
  if (proto.return_type_idx >= tables.type_descriptor_string_indices.size()) {
    return "()?";
  }
  const std::uint32_t return_string_index =
      tables.type_descriptor_string_indices[proto.return_type_idx];
  if (return_string_index >= tables.strings.size()) {
    return "()?";
  }
  signature << tables.strings[return_string_index];
  return signature.str();
}

bool ResolveMethodReference(const ParsedDexTables& tables,
                            std::uint32_t method_index,
                            DexExecutionCandidate* candidate,
                            std::vector<std::string>* errors) {
  if (candidate == nullptr || errors == nullptr) {
    return false;
  }
  if (method_index >= tables.methods.size()) {
    AppendUnique(errors, "dex_invoke_method_index_invalid");
    return false;
  }
  const auto& method = tables.methods[method_index];
  if (method.class_idx >= tables.type_descriptor_string_indices.size()) {
    AppendUnique(errors, "dex_invoke_method_class_index_invalid");
    return false;
  }
  const std::uint32_t class_string_index =
      tables.type_descriptor_string_indices[method.class_idx];
  if (class_string_index >= tables.strings.size()) {
    AppendUnique(errors, "dex_invoke_method_class_string_invalid");
    return false;
  }
  if (method.name_idx >= tables.strings.size()) {
    AppendUnique(errors, "dex_invoke_method_name_invalid");
    return false;
  }
  *candidate = {.class_descriptor = tables.strings[class_string_index],
                .method_name = tables.strings[method.name_idx],
                .method_signature = BuildMethodSignature(tables, method),
                .code_off = method_index < tables.method_code_offsets.size()
                                ? tables.method_code_offsets[method_index]
                                : 0u};
  return true;
}

bool ResolveFieldReference(const ParsedDexTables& tables,
                           std::uint32_t field_index,
                           std::string* class_descriptor,
                           std::string* field_name,
                           std::string* field_signature,
                           std::vector<std::string>* errors) {
  if (class_descriptor == nullptr || field_name == nullptr ||
      field_signature == nullptr || errors == nullptr) {
    return false;
  }
  if (field_index >= tables.fields.size()) {
    AppendUnique(errors, "dex_field_index_invalid");
    return false;
  }
  const auto& field = tables.fields[field_index];
  if (field.class_idx >= tables.type_descriptor_string_indices.size()) {
    AppendUnique(errors, "dex_field_class_index_invalid");
    return false;
  }
  if (field.type_idx >= tables.type_descriptor_string_indices.size()) {
    AppendUnique(errors, "dex_field_type_index_invalid");
    return false;
  }
  const std::uint32_t class_string_index =
      tables.type_descriptor_string_indices[field.class_idx];
  const std::uint32_t type_string_index =
      tables.type_descriptor_string_indices[field.type_idx];
  if (class_string_index >= tables.strings.size()) {
    AppendUnique(errors, "dex_field_class_string_invalid");
    return false;
  }
  if (type_string_index >= tables.strings.size()) {
    AppendUnique(errors, "dex_field_type_string_invalid");
    return false;
  }
  if (field.name_idx >= tables.strings.size()) {
    AppendUnique(errors, "dex_field_name_invalid");
    return false;
  }
  *class_descriptor = tables.strings[class_string_index];
  *field_name = tables.strings[field.name_idx];
  *field_signature = tables.strings[type_string_index];
  return true;
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
  const std::uint32_t field_ids_size = ReadLe32(bytes, 80);
  const std::uint32_t field_ids_off = ReadLe32(bytes, 84);
  const std::uint32_t method_ids_size = ReadLe32(bytes, 88);
  const std::uint32_t method_ids_off = ReadLe32(bytes, 92);
  const std::uint32_t class_defs_off = ReadLe32(bytes, 100);

  file->proto_ids_size = proto_ids_size;
  file->field_ids_size = field_ids_size;
  file->method_ids_size = method_ids_size;

  if (string_ids_off + file->string_ids_size * 4u > bytes.size() ||
      type_ids_off + file->type_ids_size * 4u > bytes.size() ||
      proto_ids_off + proto_ids_size * 12u > bytes.size() ||
      field_ids_off + field_ids_size * 8u > bytes.size() ||
      method_ids_off + method_ids_size * 8u > bytes.size() ||
      class_defs_off + file->class_defs_count * 32u > bytes.size()) {
    file->errors.push_back("dex_table_bounds_invalid");
    return false;
  }

  tables->strings.clear();
  tables->type_descriptor_string_indices.clear();
  tables->protos.clear();
  tables->proto_signatures.clear();
  tables->fields.clear();
  tables->methods.clear();
  tables->method_code_offsets.clear();
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
  tables->proto_signatures.reserve(tables->protos.size());
  for (const auto& proto : tables->protos) {
    tables->proto_signatures.push_back(
        BuildProtoSignature(bytes, *tables, proto));
  }

  for (std::uint32_t index = 0; index < field_ids_size; ++index) {
    const std::size_t offset = field_ids_off + index * 8u;
    tables->fields.push_back(
        {.class_idx = ReadLe16(bytes, offset + 0u),
         .type_idx = ReadLe16(bytes, offset + 2u),
         .name_idx = ReadLe32(bytes, offset + 4u)});
  }

  for (std::uint32_t index = 0; index < method_ids_size; ++index) {
    const std::size_t offset = method_ids_off + index * 8u;
    tables->methods.push_back(
        {.class_idx = ReadLe16(bytes, offset + 0u),
         .proto_idx = ReadLe16(bytes, offset + 2u),
         .name_idx = ReadLe32(bytes, offset + 4u)});
  }
  tables->method_code_offsets.assign(tables->methods.size(), 0u);

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

  for (const auto& class_def : tables->class_defs) {
    if (class_def.class_data_off == 0) {
      continue;
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
      file->errors.push_back("dex_class_data_truncated");
      return false;
    }
    for (std::uint32_t index = 0; index < static_fields_size + instance_fields_size;
         ++index) {
      std::uint32_t ignored = 0;
      if (!ReadUleb128(bytes, &cursor, &ignored) ||
          !ReadUleb128(bytes, &cursor, &ignored)) {
        file->errors.push_back("dex_field_data_truncated");
        return false;
      }
    }
    auto scan_methods = [&](std::uint32_t methods_size) -> bool {
      std::uint32_t previous_method_index = 0u;
      for (std::uint32_t index = 0; index < methods_size; ++index) {
        std::uint32_t method_idx_diff = 0;
        std::uint32_t access_flags = 0;
        std::uint32_t code_off = 0;
        if (!ReadUleb128(bytes, &cursor, &method_idx_diff) ||
            !ReadUleb128(bytes, &cursor, &access_flags) ||
            !ReadUleb128(bytes, &cursor, &code_off)) {
          file->errors.push_back("dex_method_data_truncated");
          return false;
        }
        previous_method_index += method_idx_diff;
        if (previous_method_index >= tables->method_code_offsets.size()) {
          file->errors.push_back("dex_method_index_invalid");
          return false;
        }
        tables->method_code_offsets[previous_method_index] = code_off;
        (void)access_flags;
      }
      return true;
    };
    if (!scan_methods(direct_methods_size) || !scan_methods(virtual_methods_size)) {
      return false;
    }
  }
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
    case 0x01:
      return "move";
    case 0x0a:
      return "move-result";
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
    case 0x16:
      return "const-wide/16";
    case 0x84:
      return "long-to-int";
    case 0x31:
      return "cmp-long";
    case 0x34:
      return "if-lt";
    case 0x35:
      return "if-ge";
    case 0x36:
      return "if-gt";
    case 0x39:
      return "if-nez";
    case 0x3a:
      return "if-ltz";
    case 0x3c:
      return "if-gtz";
    case 0x9c:
      return "sub-long";
    case 0x90:
      return "add-int";
    case 0x91:
      return "sub-int";
    case 0xd1:
      return "rsub-int/lit16";
    case 0xb0:
      return "add-int/2addr";
    case 0xb1:
      return "sub-int/2addr";
    case 0xbb:
      return "add-long/2addr";
    case 0xbc:
      return "sub-long/2addr";
    case 0xc0:
      return "and-long/2addr";
    case 0x1a:
      return "const-string";
    case 0x21:
      return "array-length";
    case 0x2b:
      return "packed-switch";
    case 0x46:
      return "aget-object";
    case 0x28:
      return "goto";
    case 0xa1:
      return "or-long";
    case 0x22:
      return "new-instance";
    case 0x23:
      return "new-array";
    case 0x52:
      return "iget";
    case 0x53:
      return "iget-wide";
    case 0x54:
      return "iget-object";
    case 0x55:
      return "iget-boolean";
    case 0x59:
      return "iput";
    case 0x5a:
      return "iput-wide";
    case 0x5b:
      return "iput-object";
    case 0x60:
      return "sget";
    case 0x62:
      return "sget-object";
    case 0x63:
      return "sget-boolean";
    case 0x67:
      return "sput";
    case 0x69:
      return "sput-object";
    case 0x6a:
      return "sput-boolean";
    case 0x6e:
      return "invoke-virtual";
    case 0x6f:
      return "invoke-super";
    case 0x70:
      return "invoke-direct";
    case 0x71:
      return "invoke-static";
    case 0x77:
      return "invoke-static/range";
    case 0x72:
      return "invoke-interface";
    default:
      break;
  }
  std::ostringstream output;
  output << "opcode-0x" << std::hex << std::nouppercase << std::setfill('0')
         << std::setw(2) << opcode;
  return output.str();
}

std::string DetermineReturnTypeDescriptor(
    const std::string& method_signature) {
  const std::size_t separator = method_signature.find(')');
  if (separator == std::string::npos ||
      separator + 1 >= method_signature.size()) {
    return "";
  }
  return method_signature.substr(separator + 1);
}

std::vector<std::string> DetermineParameterTypeDescriptors(
    const std::string& method_signature) {
  std::vector<std::string> descriptors;
  const std::size_t open = method_signature.find('(');
  const std::size_t close = method_signature.find(')');
  if (open == std::string::npos || close == std::string::npos ||
      close <= open + 1u) {
    return descriptors;
  }

  std::size_t cursor = open + 1u;
  while (cursor < close) {
    const char first = method_signature[cursor];
    const std::size_t start = cursor;
    while (cursor < close && method_signature[cursor] == '[') {
      ++cursor;
    }
    if (cursor >= close) {
      descriptors.clear();
      return descriptors;
    }
    if (method_signature[cursor] == 'L') {
      const std::size_t terminator = method_signature.find(';', cursor);
      if (terminator == std::string::npos || terminator >= close + 1u) {
        descriptors.clear();
        return descriptors;
      }
      cursor = terminator + 1u;
      descriptors.push_back(method_signature.substr(start, cursor - start));
      continue;
    }
    if (first == '[' ||
        std::string("ZBCSIFJDV").find(method_signature[cursor]) !=
            std::string::npos) {
      ++cursor;
      descriptors.push_back(method_signature.substr(start, cursor - start));
      continue;
    }
    descriptors.clear();
    return descriptors;
  }
  return descriptors;
}

struct DexInlineInvocationResult {
  bool ready = false;
  bool reached_return = false;
  bool used_stubbed_boundary = false;
  bool returned_value_is_wide = false;
  std::string execution_state = "not_attempted";
  std::string exact_blocker = "none";
  std::string stubbed_boundary_reason = "none";
  std::string invoked_method_class_descriptor;
  std::string invoked_method_name;
  std::string invoked_method_signature;
  std::string object_register_field_operation;
  std::string object_register_field_state = "not_reached";
  std::string object_register_field_reason = "none";
  std::string object_class_descriptor;
  std::string field_class_descriptor;
  std::string field_name;
  std::string field_signature;
  DexRegisterValue returned_value;
  DexRegisterValue returned_value_high;
  int decoded_instruction_count = 0;
  int executed_instruction_count = 0;
  std::uint32_t last_instruction_offset = 0;
  std::uint16_t last_opcode_value = 0;
  std::string last_opcode_name;
  std::vector<std::string> diagnostics;
  std::vector<std::string> errors;
};

bool IsReferenceTypeDescriptor(const std::string& descriptor) {
  return !descriptor.empty() &&
         (descriptor.front() == 'L' || descriptor.front() == '[');
}

bool IsWideTypeDescriptor(const std::string& descriptor) {
  return descriptor == "J" || descriptor == "D";
}

std::string DetermineArrayElementDescriptor(
    const std::string& array_descriptor) {
  if (array_descriptor.size() < 2u || array_descriptor.front() != '[') {
    return "";
  }
  return array_descriptor.substr(1u);
}

DexRegisterValue MaterializePlaceholderObjectValue(
    const std::string& class_descriptor, std::uint32_t* next_object_id,
    std::map<std::uint32_t, PlaceholderObject>* objects,
    std::int32_t array_length = -1) {
  const std::uint32_t object_id = (*next_object_id)++;
  (*objects)[object_id] = {.object_id = object_id,
                           .class_descriptor = class_descriptor,
                           .array_length = array_length,
                           .fields = {}};
  return {.kind = DexRegisterValue::Kind::kObject,
          .int_value = 0,
          .object_id = object_id,
          .class_descriptor = class_descriptor};
}

DexInlineInvocationResult ExecuteInlineInvokedMethod(
    const std::string& bytes, const ParsedDexTables& tables,
    const DexExecutionCandidate& candidate,
    const std::vector<DexRegisterValue>& incoming_registers,
    std::map<std::uint32_t, PlaceholderObject>* objects) {
  DexInlineInvocationResult result;
  if (objects == nullptr) {
    result.execution_state = "object_state_missing";
    result.exact_blocker = "dex_invoked_method_object_state_missing";
    result.errors.push_back("dex_invoked_method_object_state_missing");
    return result;
  }
  if (candidate.code_off == 0) {
    result.execution_state = "code_item_missing";
    result.exact_blocker =
        "dex_invoked_method_code_item_missing:" +
        candidate.class_descriptor + "->" + candidate.method_name +
        candidate.method_signature;
    result.errors.push_back(result.exact_blocker);
    return result;
  }
  if (candidate.code_off + 16u > bytes.size()) {
    result.execution_state = "code_item_truncated";
    result.exact_blocker = "dex_invoked_method_code_item_truncated";
    result.errors.push_back("dex_invoked_method_code_item_truncated");
    return result;
  }
  const std::uint32_t insns_size = ReadLe32(bytes, candidate.code_off + 12u);
  const std::size_t insns_off = static_cast<std::size_t>(candidate.code_off) + 16u;
  if (insns_off + static_cast<std::size_t>(insns_size) * 2u > bytes.size()) {
    result.execution_state = "instructions_truncated";
    result.exact_blocker = "dex_invoked_method_instructions_truncated";
    result.errors.push_back("dex_invoked_method_instructions_truncated");
    return result;
  }

  const std::uint16_t registers_size = ReadLe16(bytes, candidate.code_off + 0u);
  const std::uint16_t ins_size = ReadLe16(bytes, candidate.code_off + 2u);
  std::vector<DexRegisterValue> registers(
      std::max<std::size_t>(registers_size,
                            std::max<std::size_t>(incoming_registers.size(), 1u)));
  std::size_t incoming_register_base = 0u;
  if (ins_size != 0u && registers_size >= ins_size) {
    incoming_register_base =
        static_cast<std::size_t>(registers_size - ins_size);
  }
  for (std::size_t index = 0; index < incoming_registers.size(); ++index) {
    const std::size_t destination = incoming_register_base + index;
    if (destination >= registers.size()) {
      break;
    }
    registers[destination] = incoming_registers[index];
  }
  std::uint32_t next_object_id =
      objects->empty() ? 1u : (objects->rbegin()->first + 1u);
  bool pending_result_valid = false;
  bool pending_result_is_wide = false;
  DexRegisterValue pending_result;
  DexRegisterValue pending_result_high;
  const std::string return_type_descriptor =
      DetermineReturnTypeDescriptor(candidate.method_signature);
  std::map<std::string, DexRegisterValue> static_fields;

  auto load_or_materialize_object_field =
      [&](std::uint32_t object_id, const std::string& object_class_descriptor,
          const std::string& field_class_descriptor,
          const std::string& field_name, const std::string& field_signature,
          const std::string& missing_error_prefix) -> DexRegisterValue {
    DexRegisterValue loaded_value = {.kind = DexRegisterValue::Kind::kUnknown};
    auto object_it = objects->find(object_id);
    if (object_it == objects->end()) {
      result.execution_state = "object_identity_missing";
      result.exact_blocker = missing_error_prefix + "_identity_missing";
      result.errors.push_back(result.exact_blocker);
      return loaded_value;
    }
    const std::string field_key =
        field_class_descriptor + "->" + field_name + ":" + field_signature;
    const auto field_it = object_it->second.fields.find(field_key);
    if (field_it != object_it->second.fields.end()) {
      return field_it->second;
    }
    if (IsReferenceTypeDescriptor(field_signature)) {
      loaded_value = MaterializePlaceholderObjectValue(field_signature,
                                                       &next_object_id, objects);
      object_it->second.fields[field_key] = loaded_value;
      result.used_stubbed_boundary = true;
      result.stubbed_boundary_reason =
          "placeholder_object_field_materialized_for_minimal_checkpoint";
      result.diagnostics.push_back(
          "Self-Healing Android Device DEX probe materialized a placeholder object field to keep a managed lifecycle checkpoint moving");
      return loaded_value;
    }
    return {.kind = DexRegisterValue::Kind::kInt, .int_value = 0};
  };

  auto load_or_materialize_static_field =
      [&](const std::string& field_class_descriptor,
          const std::string& field_name,
          const std::string& field_signature) -> DexRegisterValue {
    const std::string field_key =
        field_class_descriptor + "->" + field_name + ":" + field_signature;
    const auto field_it = static_fields.find(field_key);
    if (field_it != static_fields.end()) {
      return field_it->second;
    }

    DexRegisterValue loaded_value;
    if (IsReferenceTypeDescriptor(field_signature)) {
      loaded_value = MaterializePlaceholderObjectValue(field_signature,
                                                       &next_object_id, objects);
    } else {
      loaded_value = {.kind = DexRegisterValue::Kind::kInt, .int_value = 0};
    }
    static_fields[field_key] = loaded_value;
    result.used_stubbed_boundary = true;
    if (result.stubbed_boundary_reason == "none") {
      result.stubbed_boundary_reason =
          "placeholder_static_field_materialized_for_minimal_checkpoint";
    }
    result.diagnostics.push_back(
        "Self-Healing Android Device DEX probe materialized a placeholder static field to keep a managed lifecycle checkpoint moving");
    return loaded_value;
  };

  auto store_static_field = [&](const std::string& field_class_descriptor,
                                const std::string& field_name,
                                const std::string& field_signature,
                                const DexRegisterValue& value) {
    const std::string field_key =
        field_class_descriptor + "->" + field_name + ":" + field_signature;
    DexRegisterValue stored_value = value;
    if (field_signature == "Z") {
      stored_value.kind = DexRegisterValue::Kind::kInt;
      stored_value.int_value =
          (value.kind == DexRegisterValue::Kind::kObject ||
           (value.kind == DexRegisterValue::Kind::kInt && value.int_value != 0))
              ? 1
              : 0;
      stored_value.object_id = 0;
      stored_value.class_descriptor.clear();
    }
    static_fields[field_key] = stored_value;
  };

  auto try_stubbed_invocation =
      [&](const DexExecutionCandidate& invoked_method,
          const std::vector<DexRegisterValue>& invoked_registers,
          DexInlineInvocationResult* invoked_result) -> bool {
    if (invoked_result == nullptr) {
      return false;
    }
    auto set_void_stub = [&](const std::string& reason,
                             const std::string& diagnostic) {
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason = reason;
      if (!diagnostic.empty()) {
        invoked_result->diagnostics.push_back(diagnostic);
      }
    };
    auto set_int_stub = [&](const std::string& reason,
                            const std::string& diagnostic,
                            std::int32_t value) {
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason = reason;
      invoked_result->returned_value = {.kind = DexRegisterValue::Kind::kInt,
                                        .int_value = value};
      if (!diagnostic.empty()) {
        invoked_result->diagnostics.push_back(diagnostic);
      }
    };
    auto infer_placeholder_collection_size =
        [&](PlaceholderObject* collection,
            bool* size_materialized) -> std::int32_t {
      if (size_materialized != nullptr) {
        *size_materialized = false;
      }
      if (collection == nullptr) {
        return -1;
      }
      const std::int32_t original_length = collection->array_length;
      std::int32_t inferred_length = original_length;
      if (!collection->array_elements.empty()) {
        const auto max_element =
            std::max_element(collection->array_elements.begin(),
                             collection->array_elements.end(),
                             [](const auto& lhs, const auto& rhs) {
                               return lhs.first < rhs.first;
                             });
        inferred_length = std::max(inferred_length, max_element->first + 1);
      }
      if (inferred_length < 0 &&
          collection->fields.find("linuxoid.singleton.item") !=
              collection->fields.end()) {
        inferred_length = 1;
      }
      if (inferred_length < 0) {
        inferred_length = 0;
      }
      collection->array_length = inferred_length;
      if (size_materialized != nullptr && original_length < 0 &&
          inferred_length >= 0) {
        *size_materialized = true;
      }
      return collection->array_length;
    };

    if (invoked_method.class_descriptor == "Ljava/lang/Object;" &&
        invoked_method.method_name == "<init>" &&
        invoked_method.method_signature == "()V") {
      set_void_stub(
          "java_lang_object_constructor_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed java.lang.Object.<init>() to keep the first real app path moving");
      return true;
    }
    if (invoked_method.class_descriptor == "Landroid/app/Activity;" &&
        invoked_method.method_name == "onCreate" &&
        invoked_method.method_signature == "()V") {
      set_void_stub(
          "android_activity_oncreate_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe crossed a stubbed Android framework Activity.onCreate() boundary");
      return true;
    }
    if (invoked_method.class_descriptor == "Landroid/app/Activity;" &&
        invoked_method.method_name == "onCreate" &&
        invoked_method.method_signature == "(Landroid/os/Bundle;)V") {
      set_void_stub(
          "android_activity_oncreate_bundle_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe crossed a stubbed Android framework Activity.onCreate(Bundle) boundary");
      return true;
    }
    if (invoked_method.class_descriptor ==
            "Landroidx/core/app/ComponentActivity;" &&
        invoked_method.method_name == "onCreate" &&
        invoked_method.method_signature == "(Landroid/os/Bundle;)V") {
      set_void_stub(
          "component_activity_oncreate_bundle_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed the AndroidX core ComponentActivity.onCreate(Bundle) seam to keep the first keyboard launch path moving");
      return true;
    }
    if (invoked_method.class_descriptor == "Lorg/acra/scheduler/SchedulerStarter;" &&
        invoked_method.method_name == "performRestore" &&
        invoked_method.method_signature == "(Landroid/os/Bundle;)V") {
      set_void_stub(
          "saved_state_restore_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed a saved-state restore helper to keep the first keyboard launch path moving");
      return true;
    }
    if (invoked_method.class_descriptor ==
            "Landroidx/lifecycle/ReportFragment$Companion;" &&
        invoked_method.method_name == "injectIfNeededIn" &&
        invoked_method.method_signature ==
            "(Landroidx/core/app/ComponentActivity;)V") {
      set_void_stub(
          "report_fragment_injection_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed lifecycle report-fragment injection for the first keyboard launch path");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/lang/Object;" &&
        invoked_method.method_name == "getClass" &&
        invoked_method.method_signature == "()Ljava/lang/Class;") {
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "java_lang_object_getclass_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value =
          MaterializePlaceholderObjectValue("Ljava/lang/Class;",
                                            &next_object_id, objects);
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed java.lang.Object.getClass() with a placeholder Class object");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/lang/Thread;" &&
        invoked_method.method_name == "currentThread" &&
        invoked_method.method_signature == "()Ljava/lang/Thread;") {
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "java_lang_thread_currentthread_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value =
          MaterializePlaceholderObjectValue("Ljava/lang/Thread;",
                                            &next_object_id, objects);
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed java.lang.Thread.currentThread() with a placeholder Thread object");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/lang/Thread;" &&
        invoked_method.method_name == "getId" &&
        invoked_method.method_signature == "()J") {
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->returned_value_is_wide = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "java_lang_thread_getid_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value = {.kind = DexRegisterValue::Kind::kInt,
                                        .int_value = 1};
      invoked_result->returned_value_high = {
          .kind = DexRegisterValue::Kind::kInt,
          .int_value = 0};
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed java.lang.Thread.getId() with a deterministic placeholder thread id");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/lang/Math;" &&
        invoked_method.method_name == "min" &&
        invoked_method.method_signature == "(II)I") {
      if (invoked_registers.size() < 2u ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kInt ||
          invoked_registers[1].kind != DexRegisterValue::Kind::kInt) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_math_min_argument_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_math_min_argument_missing");
        return true;
      }
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "java_lang_math_min_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value = {
          .kind = DexRegisterValue::Kind::kInt,
          .int_value = std::min(invoked_registers[0].int_value,
                                invoked_registers[1].int_value)};
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed java.lang.Math.min(int, int) with deterministic integer math");
      return true;
    }
    if (invoked_method.class_descriptor ==
            "Ljava/util/concurrent/CopyOnWriteArraySet;" &&
        invoked_method.method_name == "iterator" &&
        invoked_method.method_signature == "()Ljava/util/Iterator;") {
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "copyonwritearrayset_iterator_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value =
          MaterializePlaceholderObjectValue("Ljava/util/Iterator;",
                                            &next_object_id, objects);
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed CopyOnWriteArraySet.iterator() with a placeholder Iterator object");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/lang/Iterable;" &&
        invoked_method.method_name == "iterator" &&
        invoked_method.method_signature == "()Ljava/util/Iterator;") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_iterator_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_iterator_argument_placeholder_missing");
        return true;
      }
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "iterable_iterator_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value =
          MaterializePlaceholderObjectValue("Ljava/util/Iterator;",
                                            &next_object_id, objects);
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed Iterable.iterator() with a placeholder Iterator object");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/util/List;" &&
        invoked_method.method_name == "iterator" &&
        invoked_method.method_signature == "()Ljava/util/Iterator;") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[0].object_id == 0) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_list_iterator_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_list_iterator_argument_placeholder_missing");
        return true;
      }
      auto collection_it = objects->find(invoked_registers[0].object_id);
      if (collection_it == objects->end()) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "list_iterator_object_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_list_iterator_object_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_list_iterator_object_missing");
        return true;
      }
      bool size_materialized = false;
      infer_placeholder_collection_size(&collection_it->second,
                                        &size_materialized);
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          size_materialized
              ? "placeholder_list_iterator_materialized_for_minimal_checkpoint"
              : "java_util_list_iterator_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value =
          MaterializePlaceholderObjectValue("Ljava/util/Iterator;",
                                            &next_object_id, objects);
      if (invoked_result->returned_value.kind ==
              DexRegisterValue::Kind::kObject &&
          invoked_result->returned_value.object_id != 0) {
        auto iterator_it =
            objects->find(invoked_result->returned_value.object_id);
        if (iterator_it != objects->end()) {
          iterator_it->second.fields
              ["Ljava/util/Iterator;->linuxoid.collection:Ljava/lang/Object;"] =
                  invoked_registers[0];
          iterator_it->second.fields["Ljava/util/Iterator;->linuxoid.index:I"] =
              {.kind = DexRegisterValue::Kind::kInt, .int_value = 0};
        }
      }
      invoked_result->object_register_field_operation =
          "invoke-interface+list-iterator";
      invoked_result->object_register_field_state = "object-placeholder";
      invoked_result->object_register_field_reason =
          size_materialized
              ? "placeholder_list_iterator_materialized_for_minimal_checkpoint"
              : "linuxoid_placeholder_list_iterator_state_for_minimal_checkpoint";
      invoked_result->object_class_descriptor = "Ljava/util/Iterator;";
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed List.iterator() with a placeholder Iterator object bound to placeholder list state");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/util/Iterator;" &&
        invoked_method.method_name == "hasNext" &&
        invoked_method.method_signature == "()Z") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[0].object_id == 0) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_iterator_hasnext_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_iterator_hasnext_argument_placeholder_missing");
        return true;
      }
      auto iterator_it = objects->find(invoked_registers[0].object_id);
      if (iterator_it == objects->end()) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "iterator_object_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_iterator_hasnext_object_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_iterator_hasnext_object_missing");
        return true;
      }
      std::int32_t has_next = 0;
      bool stateful_iterator = false;
      bool size_materialized = false;
      const auto collection_field_it = iterator_it->second.fields.find(
          "Ljava/util/Iterator;->linuxoid.collection:Ljava/lang/Object;");
      if (collection_field_it != iterator_it->second.fields.end() &&
          collection_field_it->second.kind == DexRegisterValue::Kind::kObject &&
          collection_field_it->second.object_id != 0) {
        auto collection_it = objects->find(collection_field_it->second.object_id);
        if (collection_it != objects->end()) {
          const std::int32_t collection_size =
              infer_placeholder_collection_size(&collection_it->second,
                                                &size_materialized);
          std::int32_t current_index = 0;
          const auto index_field_it = iterator_it->second.fields.find(
              "Ljava/util/Iterator;->linuxoid.index:I");
          if (index_field_it != iterator_it->second.fields.end() &&
              index_field_it->second.kind == DexRegisterValue::Kind::kInt) {
            current_index = index_field_it->second.int_value;
          }
          has_next = (current_index >= 0 && current_index < collection_size)
                         ? 1
                         : 0;
          stateful_iterator = true;
        }
      }
      set_int_stub(
          stateful_iterator
              ? (size_materialized
                     ? "placeholder_iterator_hasnext_materialized_for_minimal_checkpoint"
                     : "iterator_hasnext_stubbed_from_placeholder_collection_state")
              : "iterator_hasnext_stubbed_for_minimal_checkpoint",
          stateful_iterator
              ? "Self-Healing Android Device DEX probe stubbed Iterator.hasNext() from placeholder collection state"
              : "Self-Healing Android Device DEX probe stubbed Iterator.hasNext() to end a listener loop deterministically",
          has_next);
      invoked_result->object_register_field_operation =
          "invoke-interface+iterator-hasnext";
      invoked_result->object_register_field_state = "object-placeholder";
      invoked_result->object_register_field_reason =
          stateful_iterator
              ? (size_materialized
                     ? "placeholder_iterator_hasnext_materialized_for_minimal_checkpoint"
                     : "linuxoid_placeholder_iterator_state_for_minimal_checkpoint")
              : "linuxoid_placeholder_iterator_state_for_minimal_checkpoint";
      invoked_result->object_class_descriptor = "Ljava/util/Iterator;";
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/util/Iterator;" &&
        invoked_method.method_name == "next" &&
        invoked_method.method_signature == "()Ljava/lang/Object;") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[0].object_id == 0) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_iterator_next_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_iterator_next_argument_placeholder_missing");
        return true;
      }
      auto iterator_it = objects->find(invoked_registers[0].object_id);
      if (iterator_it == objects->end()) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "iterator_object_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_iterator_next_object_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_iterator_next_object_missing");
        return true;
      }
      const auto collection_field_it = iterator_it->second.fields.find(
          "Ljava/util/Iterator;->linuxoid.collection:Ljava/lang/Object;");
      if (collection_field_it == iterator_it->second.fields.end() ||
          collection_field_it->second.kind != DexRegisterValue::Kind::kObject ||
          collection_field_it->second.object_id == 0) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "iterator_collection_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_iterator_next_collection_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_iterator_next_collection_missing");
        return true;
      }
      auto collection_it = objects->find(collection_field_it->second.object_id);
      if (collection_it == objects->end()) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "iterator_collection_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_iterator_next_collection_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_iterator_next_collection_missing");
        return true;
      }
      bool size_materialized = false;
      const std::int32_t collection_size =
          infer_placeholder_collection_size(&collection_it->second,
                                            &size_materialized);
      std::int32_t current_index = 0;
      const auto index_field_it = iterator_it->second.fields.find(
          "Ljava/util/Iterator;->linuxoid.index:I");
      if (index_field_it != iterator_it->second.fields.end() &&
          index_field_it->second.kind == DexRegisterValue::Kind::kInt) {
        current_index = index_field_it->second.int_value;
      }
      if (current_index < 0 || current_index >= collection_size) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "iterator_next_out_of_range";
        invoked_result->exact_blocker =
            "dex_invoked_method_iterator_next_out_of_range";
        invoked_result->errors.push_back(
            "dex_invoked_method_iterator_next_out_of_range");
        return true;
      }
      bool placeholder_element_materialized = false;
      DexRegisterValue next_value;
      const auto element_it =
          collection_it->second.array_elements.find(current_index);
      if (element_it != collection_it->second.array_elements.end()) {
        next_value = element_it->second;
      } else if (current_index == 0) {
        const auto singleton_item_it =
            collection_it->second.fields.find("linuxoid.singleton.item");
        if (singleton_item_it != collection_it->second.fields.end()) {
          next_value = singleton_item_it->second;
          collection_it->second.array_elements[current_index] = next_value;
        }
      }
      if (next_value.kind == DexRegisterValue::Kind::kUnknown) {
        next_value = MaterializePlaceholderObjectValue(
            "Ljava/lang/Object;", &next_object_id, objects);
        collection_it->second.array_elements[current_index] = next_value;
        placeholder_element_materialized = true;
      }
      iterator_it->second.fields["Ljava/util/Iterator;->linuxoid.index:I"] = {
          .kind = DexRegisterValue::Kind::kInt,
          .int_value = current_index + 1};
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          placeholder_element_materialized
              ? "placeholder_iterator_next_materialized_for_minimal_checkpoint"
              : (size_materialized
                     ? "placeholder_iterator_collection_materialized_for_minimal_checkpoint"
                     : "iterator_next_stubbed_from_placeholder_collection_state");
      invoked_result->returned_value = next_value;
      invoked_result->object_register_field_operation =
          "invoke-interface+iterator-next";
      invoked_result->object_register_field_state = "object-placeholder";
      invoked_result->object_register_field_reason =
          placeholder_element_materialized
              ? "placeholder_iterator_next_materialized_for_minimal_checkpoint"
              : (size_materialized
                     ? "placeholder_iterator_collection_materialized_for_minimal_checkpoint"
                     : "linuxoid_placeholder_iterator_state_for_minimal_checkpoint");
      invoked_result->object_class_descriptor = "Ljava/util/Iterator;";
      invoked_result->diagnostics.push_back(
          placeholder_element_materialized
              ? "Self-Healing Android Device DEX probe materialized a placeholder Iterator.next() element from placeholder collection state"
              : "Self-Healing Android Device DEX probe stubbed Iterator.next() from placeholder collection state");
      return true;
    }
    if (invoked_method.class_descriptor == "Landroid/content/Context;" &&
        invoked_method.method_name == "getAssets" &&
        invoked_method.method_signature ==
            "()Landroid/content/res/AssetManager;") {
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "android_context_getassets_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value =
          MaterializePlaceholderObjectValue(
              "Landroid/content/res/AssetManager;", &next_object_id, objects);
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed Context.getAssets() with a placeholder AssetManager object");
      return true;
    }
    if (invoked_method.class_descriptor == "Landroid/content/res/AssetManager;" &&
        invoked_method.method_name == "open" &&
        invoked_method.method_signature ==
            "(Ljava/lang/String;)Ljava/io/InputStream;") {
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "android_assetmanager_open_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value =
          MaterializePlaceholderObjectValue("Ljava/io/InputStream;",
                                            &next_object_id, objects);
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed AssetManager.open(String) with a placeholder InputStream object");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/io/InputStreamReader;" &&
        invoked_method.method_name == "<init>" &&
        invoked_method.method_signature ==
            "(Ljava/io/InputStream;Ljava/nio/charset/Charset;)V") {
      if (invoked_registers.size() < 3u ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[1].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[2].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      set_void_stub(
          "java_io_inputstreamreader_constructor_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed InputStreamReader.<init>(InputStream, Charset) to keep the first real app path moving");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/io/BufferedReader;" &&
        invoked_method.method_name == "<init>" &&
        invoked_method.method_signature == "(Ljava/io/Reader;I)V") {
      if (invoked_registers.size() < 3u ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[1].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[2].kind != DexRegisterValue::Kind::kInt) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      set_void_stub(
          "java_io_bufferedreader_constructor_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed BufferedReader.<init>(Reader, int) to keep the first real app path moving");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/io/StringWriter;" &&
        invoked_method.method_name == "<init>" &&
        invoked_method.method_signature == "()V") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      set_void_stub(
          "java_io_stringwriter_constructor_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed StringWriter.<init>() to keep the first real app path moving");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/util/HashMap;" &&
        invoked_method.method_name == "<init>" &&
        invoked_method.method_signature == "()V") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      set_void_stub(
          "java_util_hashmap_constructor_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed HashMap.<init>() to keep the first real app path moving");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/util/LinkedHashMap;" &&
        invoked_method.method_name == "<init>" &&
        invoked_method.method_signature == "()V") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      set_void_stub(
          "java_util_linkedhashmap_constructor_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed LinkedHashMap.<init>() to keep the first real app path moving");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/util/LinkedHashSet;" &&
        invoked_method.method_name == "<init>" &&
        invoked_method.method_signature == "()V") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      set_void_stub(
          "java_util_linkedhashset_constructor_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed LinkedHashSet.<init>() to keep the first real app path moving");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/util/ArrayList;" &&
        invoked_method.method_name == "<init>" &&
        invoked_method.method_signature == "()V") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      set_void_stub(
          "java_util_arraylist_constructor_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed ArrayList.<init>() to keep the first real app path moving");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/util/ArrayList;" &&
        invoked_method.method_name == "<init>" &&
        invoked_method.method_signature == "(I)V") {
      if (invoked_registers.size() < 2u ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[1].kind != DexRegisterValue::Kind::kInt) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      auto object_it = objects->find(invoked_registers[0].object_id);
      if (object_it == objects->end()) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "object_identity_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_object_identity_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_object_identity_missing");
        return true;
      }
      object_it->second.fields["Ljava/util/ArrayList;->linuxoid.capacity:I"] =
          invoked_registers[1];
      if (object_it->second.array_length < 0) {
        object_it->second.array_length = 0;
      }
      set_void_stub(
          "java_util_arraylist_constructor_with_capacity_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed ArrayList.<init>(int) to keep the first real app path moving");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/util/Collections;" &&
        invoked_method.method_name == "singletonList" &&
        invoked_method.method_signature ==
            "(Ljava/lang/Object;)Ljava/util/List;") {
      if (invoked_registers.empty()) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_singleton_list_argument_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_singleton_list_argument_missing");
        return true;
      }
      DexRegisterValue singleton_item = invoked_registers[0];
      if (singleton_item.kind != DexRegisterValue::Kind::kObject ||
          singleton_item.object_id == 0) {
        singleton_item = MaterializePlaceholderObjectValue(
            "Ljava/lang/Object;", &next_object_id, objects);
        invoked_result->diagnostics.push_back(
            "Self-Healing Android Device DEX probe materialized a placeholder singletonList argument to keep the first real app path moving");
      }
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "java_util_collections_singletonlist_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value =
          MaterializePlaceholderObjectValue("Ljava/util/List;",
                                            &next_object_id, objects);
      if (invoked_result->returned_value.kind ==
              DexRegisterValue::Kind::kObject &&
          invoked_result->returned_value.object_id != 0) {
        auto list_it = objects->find(invoked_result->returned_value.object_id);
        if (list_it != objects->end()) {
          list_it->second.array_length = 1;
          list_it->second.array_elements[0] = singleton_item;
          list_it->second.fields["linuxoid.singleton.item"] =
              singleton_item;
        }
      }
      invoked_result->object_register_field_operation =
          "invoke-static+singleton-list";
      invoked_result->object_register_field_state = "object-placeholder";
      invoked_result->object_register_field_reason =
          "linuxoid_placeholder_singleton_list_state_for_minimal_checkpoint";
      invoked_result->object_class_descriptor = "Ljava/util/List;";
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed Collections.singletonList(Object) with a placeholder single-element List object");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/util/List;" &&
        invoked_method.method_name == "size" &&
        invoked_method.method_signature == "()I") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[0].object_id == 0) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_list_size_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_list_size_argument_placeholder_missing");
        return true;
      }
      auto list_it = objects->find(invoked_registers[0].object_id);
      if (list_it == objects->end()) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "list_size_object_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_list_size_object_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_list_size_object_missing");
        return true;
      }
      bool placeholder_size_materialized = false;
      std::int32_t list_size = list_it->second.array_length;
      if (!list_it->second.array_elements.empty()) {
        const auto max_element =
            std::max_element(list_it->second.array_elements.begin(),
                             list_it->second.array_elements.end(),
                             [](const auto& lhs, const auto& rhs) {
                               return lhs.first < rhs.first;
                             });
        list_size = std::max(list_size, max_element->first + 1);
      }
      if (list_size < 0 &&
          list_it->second.fields.find("linuxoid.singleton.item") !=
              list_it->second.fields.end()) {
        list_size = 1;
      }
      if (list_size < 0) {
        list_size = 0;
        placeholder_size_materialized = true;
      }
      list_it->second.array_length = list_size;
      set_int_stub(
          placeholder_size_materialized
              ? "placeholder_list_size_materialized_for_minimal_checkpoint"
              : "java_util_list_size_stubbed_for_minimal_checkpoint",
          placeholder_size_materialized
              ? "Self-Healing Android Device DEX probe materialized a deterministic placeholder List size to keep the first real app path moving"
              : "Self-Healing Android Device DEX probe stubbed List.size() from placeholder list state",
          list_size);
      invoked_result->object_register_field_operation =
          "invoke-interface+list-size";
      invoked_result->object_register_field_state = "object-placeholder";
      invoked_result->object_register_field_reason =
          placeholder_size_materialized
              ? "placeholder_list_size_materialized_for_minimal_checkpoint"
              : "linuxoid_placeholder_list_size_state_for_minimal_checkpoint";
      invoked_result->object_class_descriptor = list_it->second.class_descriptor;
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/io/StringWriter;" &&
        invoked_method.method_name == "toString" &&
        invoked_method.method_signature == "()Ljava/lang/String;") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "java_io_stringwriter_tostring_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value = MaterializePlaceholderObjectValue(
          "Ljava/lang/String;", &next_object_id, objects);
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed StringWriter.toString() with a placeholder String object");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/lang/String;" &&
        invoked_method.method_name == "toCharArray" &&
        invoked_method.method_signature == "()[C") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "java_lang_string_tochararray_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value =
          MaterializePlaceholderObjectValue("[C", &next_object_id, objects, 1);
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed String.toCharArray() with a placeholder char array object");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/lang/String;" &&
        invoked_method.method_name == "getBytes" &&
        invoked_method.method_signature == "(Ljava/nio/charset/Charset;)[B") {
      if (invoked_registers.size() < 2u ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[1].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "java_lang_string_getbytes_charset_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value =
          MaterializePlaceholderObjectValue("[B", &next_object_id, objects, 1);
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed String.getBytes(Charset) with a placeholder byte array object");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/io/Closeable;" &&
        invoked_method.method_name == "close" &&
        invoked_method.method_signature == "()V") {
      if (invoked_registers.empty() ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      set_void_stub(
          "java_io_closeable_close_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed Closeable.close() to keep the first real app path moving");
      return true;
    }
    if (invoked_method.class_descriptor ==
            "Ljava/util/concurrent/atomic/AtomicReference;" &&
        invoked_method.method_name == "getAndSet" &&
        invoked_method.method_signature ==
            "(Ljava/lang/Object;)Ljava/lang/Object;") {
      if (invoked_registers.size() < 2u ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[1].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      const auto receiver_it = objects->find(invoked_registers[0].object_id);
      if (receiver_it == objects->end()) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "object_identity_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_atomic_reference_receiver_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_atomic_reference_receiver_missing");
        return true;
      }
      const std::string field_key =
          "Ljava/util/concurrent/atomic/AtomicReference;->value:Ljava/lang/Object;";
      DexRegisterValue previous_value;
      const auto value_it = receiver_it->second.fields.find(field_key);
      if (value_it != receiver_it->second.fields.end()) {
        previous_value = value_it->second;
      } else {
        previous_value = MaterializePlaceholderObjectValue(
            invoked_registers[1].class_descriptor.empty()
                ? "Ljava/lang/Object;"
                : invoked_registers[1].class_descriptor,
            &next_object_id, objects);
      }
      receiver_it->second.fields[field_key] = invoked_registers[1];
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "java_util_concurrent_atomic_atomicreference_getandset_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value = previous_value;
      invoked_result->object_register_field_operation =
          "invoke-virtual+iget-object+iput-object";
      invoked_result->object_register_field_state = "object-placeholder";
      invoked_result->object_register_field_reason =
          "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint";
      invoked_result->object_class_descriptor =
          "Ljava/util/concurrent/atomic/AtomicReference;";
      invoked_result->field_class_descriptor =
          "Ljava/util/concurrent/atomic/AtomicReference;";
      invoked_result->field_name = "value";
      invoked_result->field_signature = "Ljava/lang/Object;";
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed AtomicReference.getAndSet(Object) with a placeholder previous value and stored the new object");
      return true;
    }
    if (invoked_method.class_descriptor ==
            "Ljava/util/concurrent/atomic/AtomicReference;" &&
        invoked_method.method_name == "set" &&
        invoked_method.method_signature == "(Ljava/lang/Object;)V") {
      if (invoked_registers.size() < 2u ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[1].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      const auto receiver_it = objects->find(invoked_registers[0].object_id);
      if (receiver_it == objects->end()) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "object_identity_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_atomic_reference_receiver_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_atomic_reference_receiver_missing");
        return true;
      }
      const std::string field_key =
          "Ljava/util/concurrent/atomic/AtomicReference;->value:Ljava/lang/Object;";
      receiver_it->second.fields[field_key] = invoked_registers[1];
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "java_util_concurrent_atomic_atomicreference_set_stubbed_for_minimal_checkpoint";
      invoked_result->object_register_field_operation =
          "invoke-virtual+iput-object";
      invoked_result->object_register_field_state = "object-placeholder";
      invoked_result->object_register_field_reason =
          "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint";
      invoked_result->object_class_descriptor =
          "Ljava/util/concurrent/atomic/AtomicReference;";
      invoked_result->field_class_descriptor =
          "Ljava/util/concurrent/atomic/AtomicReference;";
      invoked_result->field_name = "value";
      invoked_result->field_signature = "Ljava/lang/Object;";
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed AtomicReference.set(Object) and stored the new object");
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/lang/System;" &&
        invoked_method.method_name == "arraycopy" &&
        invoked_method.method_signature ==
            "(Ljava/lang/Object;ILjava/lang/Object;II)V") {
      if (invoked_registers.size() < 5u ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[1].kind != DexRegisterValue::Kind::kInt ||
          invoked_registers[2].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[3].kind != DexRegisterValue::Kind::kInt ||
          invoked_registers[4].kind != DexRegisterValue::Kind::kInt) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_system_arraycopy_argument_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_system_arraycopy_argument_missing");
        return true;
      }

      auto source_it = objects->find(invoked_registers[0].object_id);
      if (source_it == objects->end()) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "object_identity_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_system_arraycopy_source_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_system_arraycopy_source_missing");
        return true;
      }
      auto destination_it = objects->find(invoked_registers[2].object_id);
      if (destination_it == objects->end()) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "object_identity_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_system_arraycopy_destination_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_system_arraycopy_destination_missing");
        return true;
      }

      const std::string source_array_descriptor =
          !invoked_registers[0].class_descriptor.empty()
              ? invoked_registers[0].class_descriptor
              : source_it->second.class_descriptor;
      const std::string destination_array_descriptor =
          !invoked_registers[2].class_descriptor.empty()
              ? invoked_registers[2].class_descriptor
              : destination_it->second.class_descriptor;
      const std::string source_element_descriptor =
          DetermineArrayElementDescriptor(source_array_descriptor);
      const std::string destination_element_descriptor =
          DetermineArrayElementDescriptor(destination_array_descriptor);
      if (source_element_descriptor.empty() ||
          destination_element_descriptor.empty()) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "array_operand_invalid";
        invoked_result->exact_blocker =
            "dex_invoked_method_system_arraycopy_non_array_operand";
        invoked_result->errors.push_back(
            "dex_invoked_method_system_arraycopy_non_array_operand");
        return true;
      }

      const bool source_is_reference_array =
          IsReferenceTypeDescriptor(source_element_descriptor);
      const bool destination_is_reference_array =
          IsReferenceTypeDescriptor(destination_element_descriptor);
      if (source_is_reference_array != destination_is_reference_array ||
          (!source_is_reference_array &&
           source_element_descriptor != destination_element_descriptor)) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "arraycopy_type_mismatch";
        invoked_result->exact_blocker =
            "dex_invoked_method_system_arraycopy_type_mismatch";
        invoked_result->errors.push_back(
            "dex_invoked_method_system_arraycopy_type_mismatch");
        return true;
      }

      const std::int32_t source_index = invoked_registers[1].int_value;
      const std::int32_t destination_index = invoked_registers[3].int_value;
      const std::int32_t copy_length = invoked_registers[4].int_value;
      if (source_index < 0 || destination_index < 0 || copy_length < 0) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state = "arraycopy_range_invalid";
        invoked_result->exact_blocker =
            "dex_invoked_method_system_arraycopy_range_invalid";
        invoked_result->errors.push_back(
            "dex_invoked_method_system_arraycopy_range_invalid");
        return true;
      }

      bool source_length_materialized = false;
      bool destination_length_materialized = false;
      bool placeholder_element_materialized = false;
      const std::int32_t source_required_length = source_index + copy_length;
      const std::int32_t destination_required_length =
          destination_index + copy_length;
      if (source_it->second.array_length < source_required_length) {
        source_it->second.array_length = source_required_length;
        source_length_materialized = true;
      }
      if (destination_it->second.array_length < destination_required_length) {
        destination_it->second.array_length = destination_required_length;
        destination_length_materialized = true;
      }

      for (std::int32_t offset = 0; offset < copy_length; ++offset) {
        const std::int32_t source_slot = source_index + offset;
        const std::int32_t destination_slot = destination_index + offset;
        DexRegisterValue copied_value = {.kind = DexRegisterValue::Kind::kUnknown};
        const auto source_element_it =
            source_it->second.array_elements.find(source_slot);
        if (source_element_it != source_it->second.array_elements.end()) {
          copied_value = source_element_it->second;
        } else if (source_is_reference_array) {
          copied_value = MaterializePlaceholderObjectValue(
              source_element_descriptor, &next_object_id, objects);
          source_it->second.array_elements[source_slot] = copied_value;
          placeholder_element_materialized = true;
        } else {
          copied_value = {.kind = DexRegisterValue::Kind::kInt, .int_value = 0};
          source_it->second.array_elements[source_slot] = copied_value;
          placeholder_element_materialized = true;
        }
        destination_it->second.array_elements[destination_slot] = copied_value;
      }

      set_void_stub(
          "java_lang_system_arraycopy_stubbed_for_minimal_checkpoint",
          "Self-Healing Android Device DEX probe stubbed System.arraycopy(Object, int, Object, int, int) with deterministic placeholder array copies");
      invoked_result->object_register_field_operation = "invoke-static+arraycopy";
      invoked_result->object_register_field_state = "object-placeholder";
      invoked_result->object_register_field_reason =
          (source_length_materialized || destination_length_materialized)
              ? "placeholder_array_length_materialized_for_minimal_checkpoint"
              : "linuxoid_placeholder_arraycopy_state_for_minimal_checkpoint";
      invoked_result->object_class_descriptor = destination_array_descriptor;
      if (source_length_materialized) {
        invoked_result->diagnostics.push_back(
            "Self-Healing Android Device DEX probe materialized a deterministic placeholder source-array length for System.arraycopy()");
      }
      if (destination_length_materialized) {
        invoked_result->diagnostics.push_back(
            "Self-Healing Android Device DEX probe materialized a deterministic placeholder destination-array length for System.arraycopy()");
      }
      if (placeholder_element_materialized) {
        invoked_result->diagnostics.push_back(
            "Self-Healing Android Device DEX probe materialized placeholder array elements for System.arraycopy()");
      }
      return true;
    }
    if (invoked_method.class_descriptor == "Ljava/io/Reader;" &&
        invoked_method.method_name == "read" &&
        invoked_method.method_signature == "([C)I") {
      if (invoked_registers.size() < 2u ||
          invoked_registers[0].kind != DexRegisterValue::Kind::kObject ||
          invoked_registers[1].kind != DexRegisterValue::Kind::kObject) {
        invoked_result->ready = false;
        invoked_result->reached_return = false;
        invoked_result->execution_state =
            "invoke_argument_placeholder_missing";
        invoked_result->exact_blocker =
            "dex_invoked_method_constructor_argument_placeholder_missing";
        invoked_result->errors.push_back(
            "dex_invoked_method_constructor_argument_placeholder_missing");
        return true;
      }
      invoked_result->ready = true;
      invoked_result->reached_return = true;
      invoked_result->used_stubbed_boundary = true;
      invoked_result->execution_state = "returned";
      invoked_result->exact_blocker = "none";
      invoked_result->stubbed_boundary_reason =
          "java_io_reader_read_stubbed_for_minimal_checkpoint";
      invoked_result->returned_value = {.kind = DexRegisterValue::Kind::kInt,
                                        .int_value = -1};
      invoked_result->diagnostics.push_back(
          "Self-Healing Android Device DEX probe stubbed Reader.read(char[]) to keep the first real app path moving");
      return true;
    }
    (void)invoked_registers;
    return false;
  };

  auto execute_or_stub_invoked_method =
      [&](const DexExecutionCandidate& invoked_method,
          const std::vector<DexRegisterValue>& invoked_registers) {
        DexInlineInvocationResult invoked_result;
        if (try_stubbed_invocation(invoked_method, invoked_registers,
                                   &invoked_result)) {
          return invoked_result;
        }
        return ExecuteInlineInvokedMethod(bytes, tables, invoked_method,
                                          invoked_registers, objects);
      };

  result.ready = true;
  result.execution_state = "interpreting";
  auto mark_object_field_operation =
      [&](const std::string& operation, const std::string& state,
          const std::string& reason, const std::string& object_class,
          const std::string& field_class, const std::string& field_name,
          const std::string& field_signature) {
        result.object_register_field_operation = operation;
        result.object_register_field_state = state;
        result.object_register_field_reason = reason;
        if (!object_class.empty()) {
          result.object_class_descriptor = object_class;
        }
        if (!field_class.empty()) {
          result.field_class_descriptor = field_class;
        }
        if (!field_name.empty()) {
          result.field_name = field_name;
        }
        if (!field_signature.empty()) {
          result.field_signature = field_signature;
        }
      };
  std::uint32_t pc = 0u;
  while (pc < insns_size) {
    const std::uint16_t code_unit = ReadLe16(bytes, insns_off + pc * 2u);
    const std::uint16_t opcode = static_cast<std::uint16_t>(code_unit & 0x00ffu);
    const std::string opcode_name = DescribeOpcode(opcode);
    result.last_instruction_offset = pc * 2u;
    result.last_opcode_value = opcode;
    result.last_opcode_name = opcode_name;
    ++result.decoded_instruction_count;
    ++result.executed_instruction_count;

    switch (opcode) {
      case 0x01: {  // move
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination >= registers.size() || source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_move_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_move_register_out_of_range");
          return result;
        }
        registers[destination] = registers[source];
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder move inside an invoked method");
        ++pc;
        continue;
      }
      case 0x12: {  // const/4
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        std::int32_t literal =
            static_cast<std::int32_t>((code_unit >> 12u) & 0x0fu);
        if (literal >= 8) {
          literal -= 16;
        }
        if (destination >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker = "dex_invoked_method_register_out_of_range";
          result.errors.push_back("dex_invoked_method_register_out_of_range");
          return result;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = literal};
        ++pc;
        continue;
      }
      case 0x13: {  // const/16
        if (pc + 1u >= insns_size) {
          result.execution_state = "const16_truncated";
          result.exact_blocker = "dex_invoked_method_const16_truncated";
          result.errors.push_back("dex_invoked_method_const16_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::int16_t literal = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        if (destination >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_const16_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_const16_register_out_of_range");
          return result;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = literal};
        pc += 2u;
        continue;
      }
      case 0x16: {  // const-wide/16
        if (pc + 1u >= insns_size) {
          result.execution_state = "const_wide16_truncated";
          result.exact_blocker = "dex_invoked_method_const_wide16_truncated";
          result.errors.push_back(
              "dex_invoked_method_const_wide16_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::int64_t literal = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        if (destination + 1u >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_const_wide16_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_const_wide16_register_out_of_range");
          return result;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(literal & 0xffffffffll)};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value =
                static_cast<std::int32_t>((literal >> 32) & 0xffffffffll)};
        AppendUnique(
            &result.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder const-wide/16 inside an invoked method");
        pc += 2u;
        continue;
      }
      case 0x84: {  // long-to-int
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination >= registers.size() || source + 1u >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_long_to_int_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_long_to_int_register_out_of_range");
          return result;
        }
        if (registers[source].kind != DexRegisterValue::Kind::kInt ||
            registers[source + 1u].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "long_to_int_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_long_to_int_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_long_to_int_source_invalid");
          return result;
        }
        const std::uint64_t source_low =
            static_cast<std::uint32_t>(registers[source].int_value);
        const std::uint64_t source_high =
            static_cast<std::uint32_t>(registers[source + 1u].int_value);
        const std::int64_t source_value =
            static_cast<std::int64_t>((source_high << 32u) | source_low);
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(source_value)};
        AppendUnique(
            &result.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder long-to-int inside an invoked method");
        ++pc;
        continue;
      }
      case 0x1a: {  // const-string
        if (pc + 1u >= insns_size) {
          result.execution_state = "const_string_truncated";
          result.exact_blocker = "dex_invoked_method_const_string_truncated";
          result.errors.push_back("dex_invoked_method_const_string_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t string_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_const_string_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_const_string_register_out_of_range");
          return result;
        }
        if (string_index >= tables.strings.size()) {
          result.execution_state = "string_resolution_failed";
          result.exact_blocker =
              "dex_invoked_method_const_string_resolution_failed";
          result.errors.push_back(
              "dex_invoked_method_const_string_resolution_failed");
          return result;
        }
        registers[destination] = MaterializePlaceholderObjectValue(
            "Ljava/lang/String;", &next_object_id, objects);
        result.used_stubbed_boundary = true;
        if (result.stubbed_boundary_reason == "none") {
          result.stubbed_boundary_reason =
              "placeholder_const_string_materialized_for_minimal_checkpoint";
        }
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe materialized a placeholder java.lang.String for const-string");
        pc += 2u;
        continue;
      }
      case 0x1f: {  // check-cast
        if (pc + 1u >= insns_size) {
          result.execution_state = "check_cast_truncated";
          result.exact_blocker = "dex_invoked_method_check_cast_truncated";
          result.errors.push_back("dex_invoked_method_check_cast_truncated");
          return result;
        }
        const std::uint32_t source_register =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t type_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (source_register >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_check_cast_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_check_cast_register_out_of_range");
          return result;
        }
        if (registers[source_register].kind != DexRegisterValue::Kind::kObject ||
            registers[source_register].object_id == 0) {
          result.execution_state = "check_cast_operand_invalid";
          result.exact_blocker =
              "dex_invoked_method_check_cast_operand_invalid";
          result.errors.push_back(
              "dex_invoked_method_check_cast_operand_invalid");
          return result;
        }
        if (type_index >= tables.type_descriptor_string_indices.size()) {
          result.execution_state = "check_cast_type_invalid";
          result.exact_blocker =
              "dex_invoked_method_check_cast_type_index_invalid";
          result.errors.push_back(
              "dex_invoked_method_check_cast_type_index_invalid");
          return result;
        }
        const std::uint32_t type_string_index =
            tables.type_descriptor_string_indices[type_index];
        if (type_string_index >= tables.strings.size()) {
          result.execution_state = "check_cast_type_invalid";
          result.exact_blocker =
              "dex_invoked_method_check_cast_type_string_invalid";
          result.errors.push_back(
              "dex_invoked_method_check_cast_type_string_invalid");
          return result;
        }
        auto object_it = objects->find(registers[source_register].object_id);
        if (object_it == objects->end()) {
          result.execution_state = "check_cast_object_missing";
          result.exact_blocker =
              "dex_invoked_method_check_cast_object_missing";
          result.errors.push_back(
              "dex_invoked_method_check_cast_object_missing");
          return result;
        }
        const std::string target_descriptor = tables.strings[type_string_index];
        const std::string current_descriptor =
            !registers[source_register].class_descriptor.empty()
                ? registers[source_register].class_descriptor
                : object_it->second.class_descriptor;
        const bool compatible =
            current_descriptor == target_descriptor ||
            current_descriptor == "Ljava/lang/Object;" ||
            target_descriptor == "Ljava/lang/Object;";
        if (!compatible) {
          result.execution_state = "check_cast_type_mismatch";
          result.exact_blocker =
              "dex_invoked_method_check_cast_type_mismatch";
          result.errors.push_back(
              "dex_invoked_method_check_cast_type_mismatch");
          return result;
        }
        bool placeholder_materialized = false;
        if (current_descriptor == "Ljava/lang/Object;" &&
            target_descriptor != "Ljava/lang/Object;") {
          registers[source_register].class_descriptor = target_descriptor;
          object_it->second.class_descriptor = target_descriptor;
          placeholder_materialized = true;
        }
        if (placeholder_materialized) {
          result.used_stubbed_boundary = true;
          if (result.stubbed_boundary_reason == "none") {
            result.stubbed_boundary_reason =
                "placeholder_check_cast_materialized_for_minimal_checkpoint";
          }
          result.diagnostics.push_back(
              "Self-Healing Android Device DEX probe materialized a deterministic placeholder check-cast boundary inside an invoked method");
        }
        mark_object_field_operation(
            "check-cast",
            placeholder_materialized ? "object-placeholder" : "executed",
            placeholder_materialized
                ? "placeholder_check_cast_materialized_for_minimal_checkpoint"
                : "linuxoid_check_cast_verified_for_minimal_checkpoint",
            placeholder_materialized ? target_descriptor : current_descriptor,
            "", "", "");
        pc += 2u;
        continue;
      }
      case 0x20: {  // instance-of
        if (pc + 1u >= insns_size) {
          result.execution_state = "instance_of_truncated";
          result.exact_blocker = "dex_invoked_method_instance_of_truncated";
          result.errors.push_back("dex_invoked_method_instance_of_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t type_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size() || source_register >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_instance_of_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_instance_of_register_out_of_range");
          return result;
        }
        if (registers[source_register].kind != DexRegisterValue::Kind::kObject ||
            registers[source_register].object_id == 0) {
          result.execution_state = "instance_of_operand_invalid";
          result.exact_blocker =
              "dex_invoked_method_instance_of_operand_invalid";
          result.errors.push_back(
              "dex_invoked_method_instance_of_operand_invalid");
          return result;
        }
        if (type_index >= tables.type_descriptor_string_indices.size()) {
          result.execution_state = "instance_of_type_invalid";
          result.exact_blocker =
              "dex_invoked_method_instance_of_type_index_invalid";
          result.errors.push_back(
              "dex_invoked_method_instance_of_type_index_invalid");
          return result;
        }
        const std::uint32_t type_string_index =
            tables.type_descriptor_string_indices[type_index];
        if (type_string_index >= tables.strings.size()) {
          result.execution_state = "instance_of_type_invalid";
          result.exact_blocker =
              "dex_invoked_method_instance_of_type_string_invalid";
          result.errors.push_back(
              "dex_invoked_method_instance_of_type_string_invalid");
          return result;
        }
        auto object_it = objects->find(registers[source_register].object_id);
        if (object_it == objects->end()) {
          result.execution_state = "instance_of_object_missing";
          result.exact_blocker =
              "dex_invoked_method_instance_of_object_missing";
          result.errors.push_back(
              "dex_invoked_method_instance_of_object_missing");
          return result;
        }
        const std::string target_descriptor = tables.strings[type_string_index];
        const std::string current_descriptor =
            !registers[source_register].class_descriptor.empty()
                ? registers[source_register].class_descriptor
                : object_it->second.class_descriptor;
        bool placeholder_materialized = false;
        int instance_of_result = 0;
        if (current_descriptor == target_descriptor ||
            target_descriptor == "Ljava/lang/Object;") {
          instance_of_result = 1;
        } else if (current_descriptor == "Ljava/lang/Object;" &&
                   target_descriptor != "Ljava/lang/Object;") {
          registers[source_register].class_descriptor = target_descriptor;
          object_it->second.class_descriptor = target_descriptor;
          placeholder_materialized = true;
          instance_of_result = 1;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = instance_of_result};
        if (placeholder_materialized) {
          result.used_stubbed_boundary = true;
          if (result.stubbed_boundary_reason == "none") {
            result.stubbed_boundary_reason =
                "placeholder_instance_of_materialized_for_minimal_checkpoint";
          }
          result.diagnostics.push_back(
              "Self-Healing Android Device DEX probe materialized a deterministic placeholder instance-of boundary inside an invoked method");
        }
        mark_object_field_operation(
            "instance-of",
            placeholder_materialized ? "object-placeholder" : "executed",
            placeholder_materialized
                ? "placeholder_instance_of_materialized_for_minimal_checkpoint"
                : "linuxoid_instance_of_verified_for_minimal_checkpoint",
            placeholder_materialized ? target_descriptor : current_descriptor,
            "", "", "");
        pc += 2u;
        continue;
      }
      case 0x28: {  // goto
        const std::int8_t branch_offset =
            static_cast<std::int8_t>((code_unit >> 8u) & 0x00ffu);
        const std::int64_t next_pc =
            static_cast<std::int64_t>(pc) + branch_offset;
        if (next_pc < 0 || next_pc >= static_cast<std::int64_t>(insns_size)) {
          result.execution_state = "branch_out_of_range";
          result.exact_blocker = "dex_invoked_method_goto_branch_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_goto_branch_out_of_range");
          return result;
        }
        pc = static_cast<std::uint32_t>(next_pc);
        continue;
      }
      case 0x21: {  // array-length
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t array_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination >= registers.size() || array_register >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_array_length_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_array_length_register_out_of_range");
          return result;
        }
        if (registers[array_register].kind != DexRegisterValue::Kind::kObject ||
            registers[array_register].object_id == 0) {
          result.execution_state = "array_length_operand_invalid";
          result.exact_blocker =
              "dex_invoked_method_array_length_operand_invalid";
          result.errors.push_back(
              "dex_invoked_method_array_length_operand_invalid");
          return result;
        }
        const auto object_it = objects->find(registers[array_register].object_id);
        if (object_it == objects->end()) {
          result.execution_state = "array_length_object_missing";
          result.exact_blocker =
              "dex_invoked_method_array_length_object_missing";
          result.errors.push_back(
              "dex_invoked_method_array_length_object_missing");
          return result;
        }
        if (object_it->second.array_length < 0) {
          result.execution_state = "array_length_unknown";
          result.exact_blocker = "dex_invoked_method_array_length_unknown";
          result.errors.push_back("dex_invoked_method_array_length_unknown");
          return result;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = object_it->second.array_length};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder array-length inside an invoked method");
        ++pc;
        continue;
      }
      case 0x2b: {  // packed-switch
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_packed_switch_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_packed_switch_register_out_of_range");
          return result;
        }
        if (registers[source].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "packed_switch_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_packed_switch_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_packed_switch_source_invalid");
          return result;
        }
        std::uint32_t next_pc = 0u;
        std::string failure_reason;
        if (!ResolvePackedSwitchTarget(bytes, insns_off, insns_size, pc,
                                       registers[source].int_value, &next_pc,
                                       &failure_reason)) {
          result.execution_state = "packed_switch_payload_invalid";
          result.exact_blocker =
              "dex_invoked_method_" + failure_reason;
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder packed-switch inside an invoked method");
        pc = next_pc;
        continue;
      }
      case 0x46: {  // aget-object
        if (pc + 1u >= insns_size) {
          result.execution_state = "aget_object_truncated";
          result.exact_blocker =
              "dex_invoked_method_aget_object_truncated";
          result.errors.push_back(
              "dex_invoked_method_aget_object_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t registers_word =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint32_t array_register =
            static_cast<std::uint32_t>(registers_word & 0x00ffu);
        const std::uint32_t index_register =
            static_cast<std::uint32_t>((registers_word >> 8u) & 0x00ffu);
        if (destination >= registers.size() || array_register >= registers.size() ||
            index_register >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_aget_object_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_aget_object_register_out_of_range");
          return result;
        }
        if (registers[array_register].kind != DexRegisterValue::Kind::kObject ||
            registers[array_register].object_id == 0) {
          result.execution_state = "aget_object_operand_invalid";
          result.exact_blocker =
              "dex_invoked_method_aget_object_operand_invalid";
          result.errors.push_back(
              "dex_invoked_method_aget_object_operand_invalid");
          return result;
        }
        if (registers[index_register].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "aget_object_index_invalid";
          result.exact_blocker =
              "dex_invoked_method_aget_object_index_invalid";
          result.errors.push_back(
              "dex_invoked_method_aget_object_index_invalid");
          return result;
        }
        auto object_it = objects->find(registers[array_register].object_id);
        if (object_it == objects->end()) {
          result.execution_state = "aget_object_array_missing";
          result.exact_blocker =
              "dex_invoked_method_aget_object_array_missing";
          result.errors.push_back(
              "dex_invoked_method_aget_object_array_missing");
          return result;
        }
        const std::int32_t element_index = registers[index_register].int_value;
        const std::string element_descriptor =
            DetermineArrayElementDescriptor(object_it->second.class_descriptor);
        if (!IsReferenceTypeDescriptor(element_descriptor)) {
          result.execution_state = "aget_object_not_reference_array";
          result.exact_blocker =
              "dex_invoked_method_aget_object_not_reference_array";
          result.errors.push_back(
              "dex_invoked_method_aget_object_not_reference_array");
          return result;
        }
        bool array_length_materialized = false;
        if (object_it->second.array_length < 0) {
          if (element_index < 0) {
            result.execution_state = "aget_object_index_out_of_bounds";
            result.exact_blocker =
                "dex_invoked_method_aget_object_index_out_of_bounds";
            result.errors.push_back(
                "dex_invoked_method_aget_object_index_out_of_bounds");
            return result;
          }
          object_it->second.array_length = element_index + 1;
          array_length_materialized = true;
        }
        if (element_index < 0 ||
            element_index >= object_it->second.array_length) {
          result.execution_state = "aget_object_index_out_of_bounds";
          result.exact_blocker =
              "dex_invoked_method_aget_object_index_out_of_bounds";
          result.errors.push_back(
              "dex_invoked_method_aget_object_index_out_of_bounds");
          return result;
        }
        bool placeholder_materialized = false;
        DexRegisterValue loaded_value = {.kind = DexRegisterValue::Kind::kUnknown};
        const auto element_it =
            object_it->second.array_elements.find(element_index);
        if (element_it != object_it->second.array_elements.end()) {
          loaded_value = element_it->second;
        } else {
          loaded_value = MaterializePlaceholderObjectValue(
              element_descriptor, &next_object_id, objects);
          object_it->second.array_elements[element_index] = loaded_value;
          placeholder_materialized = true;
        }
        if (loaded_value.kind != DexRegisterValue::Kind::kObject) {
          result.execution_state = "aget_object_loaded_value_invalid";
          result.exact_blocker =
              "dex_invoked_method_aget_object_loaded_value_invalid";
          result.errors.push_back(
              "dex_invoked_method_aget_object_loaded_value_invalid");
          return result;
        }
        registers[destination] = loaded_value;
        if (array_length_materialized || placeholder_materialized) {
          result.used_stubbed_boundary = true;
          if (result.stubbed_boundary_reason == "none") {
            result.stubbed_boundary_reason =
                array_length_materialized
                    ? "placeholder_array_length_materialized_for_minimal_checkpoint"
                    : "placeholder_array_element_materialized_for_minimal_checkpoint";
          }
        }
        if (array_length_materialized) {
          result.diagnostics.push_back(
              "Self-Healing Android Device DEX probe materialized a deterministic placeholder object-array length inside an invoked method");
        }
        if (placeholder_materialized) {
          result.diagnostics.push_back(
              "Self-Healing Android Device DEX probe materialized a placeholder object array element inside an invoked method");
        }
        mark_object_field_operation(
            "aget-object",
            (array_length_materialized || placeholder_materialized)
                ? "object-placeholder"
                : "executed",
            array_length_materialized
                ? "placeholder_array_length_materialized_for_minimal_checkpoint"
                : (placeholder_materialized
                       ? "placeholder_array_element_materialized_for_minimal_checkpoint"
                       : "linuxoid_placeholder_array_element_state_for_minimal_checkpoint"),
            loaded_value.class_descriptor, "", "", "");
        pc += 2u;
        continue;
      }
      case 0x31: {  // cmp-long
        if (pc + 1u >= insns_size) {
          result.execution_state = "cmp_long_truncated";
          result.exact_blocker = "dex_invoked_method_cmp_long_truncated";
          result.errors.push_back("dex_invoked_method_cmp_long_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t registers_word =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint32_t source_left =
            static_cast<std::uint32_t>(registers_word & 0x00ffu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((registers_word >> 8u) & 0x00ffu);
        if (destination >= registers.size() ||
            source_left + 1u >= registers.size() ||
            source_right + 1u >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_cmp_long_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_cmp_long_register_out_of_range");
          return result;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_left + 1u].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right + 1u].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "cmp_long_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_cmp_long_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_cmp_long_source_invalid");
          return result;
        }
        const std::uint64_t left_low =
            static_cast<std::uint32_t>(registers[source_left].int_value);
        const std::uint64_t left_high =
            static_cast<std::uint32_t>(registers[source_left + 1u].int_value);
        const std::uint64_t right_low =
            static_cast<std::uint32_t>(registers[source_right].int_value);
        const std::uint64_t right_high =
            static_cast<std::uint32_t>(registers[source_right + 1u].int_value);
        const std::int64_t left_value = static_cast<std::int64_t>(
            (left_high << 32u) | left_low);
        const std::int64_t right_value = static_cast<std::int64_t>(
            (right_high << 32u) | right_low);
        const std::int32_t comparison =
            left_value < right_value ? -1 : (left_value > right_value ? 1 : 0);
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = comparison};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder cmp-long inside an invoked method");
        pc += 2u;
        continue;
      }
      case 0x81: {  // int-to-long
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination + 1u >= registers.size() || source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_int_to_long_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_int_to_long_register_out_of_range");
          return result;
        }
        if (registers[source].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "int_to_long_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_int_to_long_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_int_to_long_source_invalid");
          return result;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = registers[source].int_value};
        registers[destination + 1u] = {.kind = DexRegisterValue::Kind::kInt,
                                       .int_value = 0};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder int-to-long inside an invoked method");
        ++pc;
        continue;
      }
      case 0xa1: {  // or-long
        if (pc + 1u >= insns_size) {
          result.execution_state = "or_long_truncated";
          result.exact_blocker = "dex_invoked_method_or_long_truncated";
          result.errors.push_back("dex_invoked_method_or_long_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t registers_word =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint32_t source_left =
            static_cast<std::uint32_t>(registers_word & 0x00ffu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((registers_word >> 8u) & 0x00ffu);
        if (destination + 1u >= registers.size() ||
            source_left + 1u >= registers.size() ||
            source_right + 1u >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_or_long_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_or_long_register_out_of_range");
          return result;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_left + 1u].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right + 1u].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "or_long_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_or_long_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_or_long_source_invalid");
          return result;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value =
                                      registers[source_left].int_value |
                                      registers[source_right].int_value};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = registers[source_left + 1u].int_value |
                         registers[source_right + 1u].int_value};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder or-long inside an invoked method");
        pc += 2u;
        continue;
      }
      case 0x91: {  // sub-int
        if (pc + 1u >= insns_size) {
          result.execution_state = "sub_int_truncated";
          result.exact_blocker = "dex_invoked_method_sub_int_truncated";
          result.errors.push_back("dex_invoked_method_sub_int_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t registers_word =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint32_t source_left =
            static_cast<std::uint32_t>(registers_word & 0x00ffu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((registers_word >> 8u) & 0x00ffu);
        if (destination >= registers.size() || source_left >= registers.size() ||
            source_right >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_sub_int_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_sub_int_register_out_of_range");
          return result;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "sub_int_source_invalid";
          result.exact_blocker = "dex_invoked_method_sub_int_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_sub_int_source_invalid");
          return result;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = registers[source_left].int_value -
                         registers[source_right].int_value};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder sub-int inside an invoked method");
        pc += 2u;
        continue;
      }
      case 0x90: {  // add-int
        if (pc + 1u >= insns_size) {
          result.execution_state = "add_int_truncated";
          result.exact_blocker = "dex_invoked_method_add_int_truncated";
          result.errors.push_back("dex_invoked_method_add_int_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t registers_word =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint32_t source_left =
            static_cast<std::uint32_t>(registers_word & 0x00ffu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((registers_word >> 8u) & 0x00ffu);
        if (destination >= registers.size() || source_left >= registers.size() ||
            source_right >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_add_int_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_add_int_register_out_of_range");
          return result;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "add_int_source_invalid";
          result.exact_blocker = "dex_invoked_method_add_int_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_add_int_source_invalid");
          return result;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(
                registers[source_left].int_value +
                registers[source_right].int_value)};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder add-int inside an invoked method");
        pc += 2u;
        continue;
      }
      case 0xd1: {  // rsub-int/lit16
        if (pc + 1u >= insns_size) {
          result.execution_state = "rsub_int_lit16_truncated";
          result.exact_blocker =
              "dex_invoked_method_rsub_int_lit16_truncated";
          result.errors.push_back(
              "dex_invoked_method_rsub_int_lit16_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::int16_t literal = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        if (destination >= registers.size() || source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_rsub_int_lit16_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_rsub_int_lit16_register_out_of_range");
          return result;
        }
        if (registers[source].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "rsub_int_lit16_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_rsub_int_lit16_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_rsub_int_lit16_source_invalid");
          return result;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(literal) -
                         registers[source].int_value};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder rsub-int/lit16 inside an invoked method");
        pc += 2u;
        continue;
      }
      case 0x9c: {  // sub-long
        if (pc + 1u >= insns_size) {
          result.execution_state = "sub_long_truncated";
          result.exact_blocker = "dex_invoked_method_sub_long_truncated";
          result.errors.push_back("dex_invoked_method_sub_long_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t registers_word =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint32_t source_left =
            static_cast<std::uint32_t>(registers_word & 0x00ffu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((registers_word >> 8u) & 0x00ffu);
        if (destination + 1u >= registers.size() ||
            source_left + 1u >= registers.size() ||
            source_right + 1u >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_sub_long_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_sub_long_register_out_of_range");
          return result;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_left + 1u].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right + 1u].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "sub_long_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_sub_long_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_sub_long_source_invalid");
          return result;
        }
        const std::uint64_t left_low =
            static_cast<std::uint32_t>(registers[source_left].int_value);
        const std::uint64_t left_high =
            static_cast<std::uint32_t>(registers[source_left + 1u].int_value);
        const std::uint64_t right_low =
            static_cast<std::uint32_t>(registers[source_right].int_value);
        const std::uint64_t right_high =
            static_cast<std::uint32_t>(registers[source_right + 1u].int_value);
        const std::int64_t left_value =
            static_cast<std::int64_t>((left_high << 32u) | left_low);
        const std::int64_t right_value =
            static_cast<std::int64_t>((right_high << 32u) | right_low);
        const std::uint64_t raw_result = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(left_value - right_value));
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(raw_result & 0xffffffffu)};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>((raw_result >> 32u) &
                                                   0xffffffffu)};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder sub-long inside an invoked method");
        pc += 2u;
        continue;
      }
      case 0xb0: {  // add-int/2addr
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination >= registers.size() || source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_add_int_2addr_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_add_int_2addr_register_out_of_range");
          return result;
        }
        if (registers[destination].kind != DexRegisterValue::Kind::kInt ||
            registers[source].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "add_int_2addr_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_add_int_2addr_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_add_int_2addr_source_invalid");
          return result;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(
                registers[destination].int_value + registers[source].int_value)};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder add-int/2addr inside an invoked method");
        ++pc;
        continue;
      }
      case 0xb1: {  // sub-int/2addr
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination >= registers.size() || source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_sub_int_2addr_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_sub_int_2addr_register_out_of_range");
          return result;
        }
        if (registers[destination].kind != DexRegisterValue::Kind::kInt ||
            registers[source].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "sub_int_2addr_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_sub_int_2addr_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_sub_int_2addr_source_invalid");
          return result;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(
                registers[destination].int_value - registers[source].int_value)};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder sub-int/2addr inside an invoked method");
        ++pc;
        continue;
      }
      case 0xbb: {  // add-long/2addr
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination + 1u >= registers.size() ||
            source + 1u >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_add_long_2addr_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_add_long_2addr_register_out_of_range");
          return result;
        }
        if (registers[destination].kind != DexRegisterValue::Kind::kInt ||
            registers[destination + 1u].kind != DexRegisterValue::Kind::kInt ||
            registers[source].kind != DexRegisterValue::Kind::kInt ||
            registers[source + 1u].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "add_long_2addr_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_add_long_2addr_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_add_long_2addr_source_invalid");
          return result;
        }
        const std::uint64_t destination_low =
            static_cast<std::uint32_t>(registers[destination].int_value);
        const std::uint64_t destination_high =
            static_cast<std::uint32_t>(registers[destination + 1u].int_value);
        const std::uint64_t source_low =
            static_cast<std::uint32_t>(registers[source].int_value);
        const std::uint64_t source_high =
            static_cast<std::uint32_t>(registers[source + 1u].int_value);
        const std::int64_t destination_value = static_cast<std::int64_t>(
            (destination_high << 32u) | destination_low);
        const std::int64_t source_value =
            static_cast<std::int64_t>((source_high << 32u) | source_low);
        const std::uint64_t raw_result = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(destination_value + source_value));
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(raw_result & 0xffffffffu)};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>((raw_result >> 32u) &
                                                   0xffffffffu)};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder add-long/2addr inside an invoked method");
        ++pc;
        continue;
      }
      case 0xbc: {  // sub-long/2addr
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination + 1u >= registers.size() ||
            source + 1u >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_sub_long_2addr_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_sub_long_2addr_register_out_of_range");
          return result;
        }
        if (registers[destination].kind != DexRegisterValue::Kind::kInt ||
            registers[destination + 1u].kind != DexRegisterValue::Kind::kInt ||
            registers[source].kind != DexRegisterValue::Kind::kInt ||
            registers[source + 1u].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "sub_long_2addr_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_sub_long_2addr_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_sub_long_2addr_source_invalid");
          return result;
        }
        const std::uint64_t destination_low =
            static_cast<std::uint32_t>(registers[destination].int_value);
        const std::uint64_t destination_high =
            static_cast<std::uint32_t>(registers[destination + 1u].int_value);
        const std::uint64_t source_low =
            static_cast<std::uint32_t>(registers[source].int_value);
        const std::uint64_t source_high =
            static_cast<std::uint32_t>(registers[source + 1u].int_value);
        const std::int64_t destination_value = static_cast<std::int64_t>(
            (destination_high << 32u) | destination_low);
        const std::int64_t source_value =
            static_cast<std::int64_t>((source_high << 32u) | source_low);
        const std::uint64_t raw_result = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(destination_value - source_value));
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(raw_result & 0xffffffffu)};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>((raw_result >> 32u) &
                                                   0xffffffffu)};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder sub-long/2addr inside an invoked method");
        ++pc;
        continue;
      }
      case 0xc0: {  // and-long/2addr
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination + 1u >= registers.size() ||
            source + 1u >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_and_long_2addr_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_and_long_2addr_register_out_of_range");
          return result;
        }
        if (registers[destination].kind != DexRegisterValue::Kind::kInt ||
            registers[destination + 1u].kind != DexRegisterValue::Kind::kInt ||
            registers[source].kind != DexRegisterValue::Kind::kInt ||
            registers[source + 1u].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "and_long_2addr_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_and_long_2addr_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_and_long_2addr_source_invalid");
          return result;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(registers[destination].int_value) &
                static_cast<std::uint32_t>(registers[source].int_value))};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(registers[destination + 1u].int_value) &
                static_cast<std::uint32_t>(registers[source + 1u].int_value))};
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder and-long/2addr inside an invoked method");
        ++pc;
        continue;
      }
      case 0x0a:
      case 0x0b:
      case 0x0c: {  // move-result / move-result-wide / move-result-object
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (opcode == 0x0b) {
          if (destination + 1u >= registers.size()) {
            result.execution_state = "register_out_of_range";
            result.exact_blocker =
                "dex_invoked_method_move_result_wide_register_out_of_range";
            result.errors.push_back(
                "dex_invoked_method_move_result_wide_register_out_of_range");
            return result;
          }
        } else if (destination >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              opcode == 0x0a ? "dex_invoked_method_move_result_register_out_of_range"
                             : "dex_invoked_method_move_result_object_register_out_of_range";
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        if (!pending_result_valid || (opcode == 0x0b && !pending_result_is_wide)) {
          result.execution_state =
              opcode == 0x0b ? "move_result_wide_without_pending_value"
                             : "move_result_without_pending_value";
          result.exact_blocker =
              opcode == 0x0a
                  ? "dex_invoked_method_move_result_without_pending_value"
                  : (opcode == 0x0b
                         ? "dex_invoked_method_move_result_wide_without_pending_value"
                         : "dex_invoked_method_move_result_object_without_pending_value");
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        registers[destination] = pending_result;
        if (opcode == 0x0b) {
          registers[destination + 1u] = pending_result_high;
        }
        pending_result_valid = false;
        pending_result_is_wide = false;
        ++pc;
        continue;
      }
      case 0x22: {  // new-instance
        if (pc + 1u >= insns_size) {
          result.execution_state = "new_instance_truncated";
          result.exact_blocker = "dex_invoked_method_new_instance_truncated";
          result.errors.push_back("dex_invoked_method_new_instance_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t type_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_new_instance_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_new_instance_register_out_of_range");
          return result;
        }
        if (type_index >= tables.type_descriptor_string_indices.size()) {
          result.execution_state = "new_instance_type_invalid";
          result.exact_blocker =
              "dex_invoked_method_new_instance_type_index_invalid";
          result.errors.push_back(
              "dex_invoked_method_new_instance_type_index_invalid");
          return result;
        }
        const std::uint32_t type_string_index =
            tables.type_descriptor_string_indices[type_index];
        if (type_string_index >= tables.strings.size()) {
          result.execution_state = "new_instance_type_invalid";
          result.exact_blocker =
              "dex_invoked_method_new_instance_type_string_invalid";
          result.errors.push_back(
              "dex_invoked_method_new_instance_type_string_invalid");
          return result;
        }
        const std::string class_descriptor = tables.strings[type_string_index];
        registers[destination] = MaterializePlaceholderObjectValue(
            class_descriptor, &next_object_id, objects);
        mark_object_field_operation(
            "new-instance", "object-placeholder",
            "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint",
            class_descriptor, "", "", "");
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe modeled a placeholder object allocation inside an invoked method");
        pc += 2u;
        continue;
      }
      case 0x23: {  // new-array
        if (pc + 1u >= insns_size) {
          result.execution_state = "new_array_truncated";
          result.exact_blocker = "dex_invoked_method_new_array_truncated";
          result.errors.push_back("dex_invoked_method_new_array_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t size_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t type_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size() || size_register >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_new_array_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_new_array_register_out_of_range");
          return result;
        }
        if (registers[size_register].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "new_array_size_register_invalid";
          result.exact_blocker =
              "dex_invoked_method_new_array_size_register_invalid";
          result.errors.push_back(
              "dex_invoked_method_new_array_size_register_invalid");
          return result;
        }
        if (type_index >= tables.type_descriptor_string_indices.size()) {
          result.execution_state = "new_array_type_invalid";
          result.exact_blocker =
              "dex_invoked_method_new_array_type_index_invalid";
          result.errors.push_back(
              "dex_invoked_method_new_array_type_index_invalid");
          return result;
        }
        const std::uint32_t type_string_index =
            tables.type_descriptor_string_indices[type_index];
        if (type_string_index >= tables.strings.size()) {
          result.execution_state = "new_array_type_invalid";
          result.exact_blocker =
              "dex_invoked_method_new_array_type_string_invalid";
          result.errors.push_back(
              "dex_invoked_method_new_array_type_string_invalid");
          return result;
        }
        const std::string class_descriptor = tables.strings[type_string_index];
        registers[destination] = MaterializePlaceholderObjectValue(
            class_descriptor, &next_object_id, objects,
            registers[size_register].int_value);
        mark_object_field_operation(
            "new-array", "object-placeholder",
            "linuxoid_placeholder_array_state_for_minimal_checkpoint",
            class_descriptor, "", "", "");
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe modeled a placeholder array allocation inside an invoked method");
        pc += 2u;
        continue;
      }
      case 0x38: {  // if-eqz
        if (pc + 1u >= insns_size) {
          result.execution_state = "branch_truncated";
          result.exact_blocker = "dex_invoked_method_if_eqz_truncated";
          result.errors.push_back("dex_invoked_method_if_eqz_truncated");
          return result;
        }
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_if_eqz_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_if_eqz_register_out_of_range");
          return result;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_zero =
            registers[source].kind == DexRegisterValue::Kind::kUnknown ||
            (registers[source].kind == DexRegisterValue::Kind::kInt &&
             registers[source].int_value == 0);
        if (is_zero) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            result.execution_state = "branch_out_of_range";
            result.exact_blocker =
                "dex_invoked_method_if_eqz_branch_out_of_range";
            result.errors.push_back(
                "dex_invoked_method_if_eqz_branch_out_of_range");
            return result;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x39: {  // if-nez
        if (pc + 1u >= insns_size) {
          result.execution_state = "branch_truncated";
          result.exact_blocker = "dex_invoked_method_if_nez_truncated";
          result.errors.push_back("dex_invoked_method_if_nez_truncated");
          return result;
        }
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_if_nez_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_if_nez_register_out_of_range");
          return result;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_non_zero =
            registers[source].kind == DexRegisterValue::Kind::kObject ||
            (registers[source].kind == DexRegisterValue::Kind::kInt &&
             registers[source].int_value != 0);
        if (is_non_zero) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            result.execution_state = "branch_out_of_range";
            result.exact_blocker =
                "dex_invoked_method_if_nez_branch_out_of_range";
            result.errors.push_back(
                "dex_invoked_method_if_nez_branch_out_of_range");
            return result;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x3a: {  // if-ltz
        if (pc + 1u >= insns_size) {
          result.execution_state = "branch_truncated";
          result.exact_blocker = "dex_invoked_method_if_ltz_truncated";
          result.errors.push_back("dex_invoked_method_if_ltz_truncated");
          return result;
        }
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_if_ltz_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_if_ltz_register_out_of_range");
          return result;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_less_than_zero =
            registers[source].kind == DexRegisterValue::Kind::kInt &&
            registers[source].int_value < 0;
        if (is_less_than_zero) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            result.execution_state = "branch_out_of_range";
            result.exact_blocker =
                "dex_invoked_method_if_ltz_branch_out_of_range";
            result.errors.push_back(
                "dex_invoked_method_if_ltz_branch_out_of_range");
            return result;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x3c: {  // if-gtz
        if (pc + 1u >= insns_size) {
          result.execution_state = "branch_truncated";
          result.exact_blocker = "dex_invoked_method_if_gtz_truncated";
          result.errors.push_back("dex_invoked_method_if_gtz_truncated");
          return result;
        }
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_if_gtz_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_if_gtz_register_out_of_range");
          return result;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_greater_than_zero =
            registers[source].kind == DexRegisterValue::Kind::kInt &&
            registers[source].int_value > 0;
        if (is_greater_than_zero) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            result.execution_state = "branch_out_of_range";
            result.exact_blocker =
                "dex_invoked_method_if_gtz_branch_out_of_range";
            result.errors.push_back(
                "dex_invoked_method_if_gtz_branch_out_of_range");
            return result;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x34: {  // if-lt
        if (pc + 1u >= insns_size) {
          result.execution_state = "branch_truncated";
          result.exact_blocker = "dex_invoked_method_if_lt_truncated";
          result.errors.push_back("dex_invoked_method_if_lt_truncated");
          return result;
        }
        const std::uint32_t source_left =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (source_left >= registers.size() || source_right >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_if_lt_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_if_lt_register_out_of_range");
          return result;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "if_lt_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_if_lt_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_if_lt_source_invalid");
          return result;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_less_than =
            registers[source_left].int_value <
            registers[source_right].int_value;
        if (is_less_than) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            result.execution_state = "branch_out_of_range";
            result.exact_blocker =
                "dex_invoked_method_if_lt_branch_out_of_range";
            result.errors.push_back(
                "dex_invoked_method_if_lt_branch_out_of_range");
            return result;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x33: {  // if-ne
        if (pc + 1u >= insns_size) {
          result.execution_state = "branch_truncated";
          result.exact_blocker = "dex_invoked_method_if_ne_truncated";
          result.errors.push_back("dex_invoked_method_if_ne_truncated");
          return result;
        }
        const std::uint32_t source_left =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (source_left >= registers.size() || source_right >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_if_ne_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_if_ne_register_out_of_range");
          return result;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        bool is_not_equal = false;
        if (registers[source_left].kind == DexRegisterValue::Kind::kInt &&
            registers[source_right].kind == DexRegisterValue::Kind::kInt) {
          is_not_equal = registers[source_left].int_value !=
                         registers[source_right].int_value;
        } else if (registers[source_left].kind ==
                       DexRegisterValue::Kind::kObject &&
                   registers[source_right].kind ==
                       DexRegisterValue::Kind::kObject) {
          is_not_equal =
              registers[source_left].object_id !=
              registers[source_right].object_id;
        } else {
          result.execution_state = "if_ne_source_invalid";
          result.exact_blocker = "dex_invoked_method_if_ne_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_if_ne_source_invalid");
          return result;
        }
        if (is_not_equal) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            result.execution_state = "branch_out_of_range";
            result.exact_blocker =
                "dex_invoked_method_if_ne_branch_out_of_range";
            result.errors.push_back(
                "dex_invoked_method_if_ne_branch_out_of_range");
            return result;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x35: {  // if-ge
        if (pc + 1u >= insns_size) {
          result.execution_state = "branch_truncated";
          result.exact_blocker = "dex_invoked_method_if_ge_truncated";
          result.errors.push_back("dex_invoked_method_if_ge_truncated");
          return result;
        }
        const std::uint32_t source_left =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (source_left >= registers.size() || source_right >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_if_ge_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_if_ge_register_out_of_range");
          return result;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "if_ge_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_if_ge_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_if_ge_source_invalid");
          return result;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_greater_or_equal =
            registers[source_left].int_value >=
            registers[source_right].int_value;
        if (is_greater_or_equal) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            result.execution_state = "branch_out_of_range";
            result.exact_blocker =
                "dex_invoked_method_if_ge_branch_out_of_range";
            result.errors.push_back(
                "dex_invoked_method_if_ge_branch_out_of_range");
            return result;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x36: {  // if-gt
        if (pc + 1u >= insns_size) {
          result.execution_state = "branch_truncated";
          result.exact_blocker = "dex_invoked_method_if_gt_truncated";
          result.errors.push_back("dex_invoked_method_if_gt_truncated");
          return result;
        }
        const std::uint32_t source_left =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (source_left >= registers.size() || source_right >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_if_gt_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_if_gt_register_out_of_range");
          return result;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "if_gt_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_if_gt_source_invalid";
          result.errors.push_back(
              "dex_invoked_method_if_gt_source_invalid");
          return result;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_greater_than =
            registers[source_left].int_value >
            registers[source_right].int_value;
        if (is_greater_than) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            result.execution_state = "branch_out_of_range";
            result.exact_blocker =
                "dex_invoked_method_if_gt_branch_out_of_range";
            result.errors.push_back(
                "dex_invoked_method_if_gt_branch_out_of_range");
            return result;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x60:
      case 0x62:
      case 0x63: {  // sget / sget-object / sget-boolean
        if (pc + 1u >= insns_size) {
          result.execution_state = "static_field_load_truncated";
          result.exact_blocker =
              opcode == 0x60 ? "dex_invoked_method_sget_truncated"
              : (opcode == 0x62
                     ? "dex_invoked_method_sget_object_truncated"
                     : "dex_invoked_method_sget_boolean_truncated");
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              opcode == 0x60 ? "dex_invoked_method_sget_register_out_of_range"
              : (opcode == 0x62
                     ? "dex_invoked_method_sget_object_register_out_of_range"
                     : "dex_invoked_method_sget_boolean_register_out_of_range");
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &result.errors)) {
          result.execution_state = "field_resolution_failed";
          result.exact_blocker =
              result.errors.empty()
                  ? (opcode == 0x60
                         ? "dex_invoked_method_sget_field_resolution_failed"
                         : (opcode == 0x62
                                ? "dex_invoked_method_sget_object_field_resolution_failed"
                                : "dex_invoked_method_sget_boolean_field_resolution_failed"))
                  : result.errors.front();
          return result;
        }
        registers[destination] = load_or_materialize_static_field(
            field_class_descriptor, field_name, field_signature);
        pc += 2u;
        continue;
      }
      case 0x67:
      case 0x69:
      case 0x6a: {  // sput / sput-object / sput-boolean
        if (pc + 1u >= insns_size) {
          result.execution_state = "static_field_store_truncated";
          result.exact_blocker =
              opcode == 0x67 ? "dex_invoked_method_sput_truncated"
              : (opcode == 0x69
                     ? "dex_invoked_method_sput_object_truncated"
                     : "dex_invoked_method_sput_boolean_truncated");
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              opcode == 0x67 ? "dex_invoked_method_sput_register_out_of_range"
              : (opcode == 0x69
                     ? "dex_invoked_method_sput_object_register_out_of_range"
                     : "dex_invoked_method_sput_boolean_register_out_of_range");
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &result.errors)) {
          result.execution_state = "field_resolution_failed";
          result.exact_blocker =
              result.errors.empty()
                  ? (opcode == 0x67
                         ? "dex_invoked_method_sput_field_resolution_failed"
                         : (opcode == 0x69
                                ? "dex_invoked_method_sput_object_field_resolution_failed"
                                : "dex_invoked_method_sput_boolean_field_resolution_failed"))
                  : result.errors.front();
          return result;
        }
        store_static_field(field_class_descriptor, field_name, field_signature,
                           registers[source]);
        pc += 2u;
        continue;
      }
      case 0x52: {  // iget
        if (pc + 1u >= insns_size) {
          result.execution_state = "field_load_truncated";
          result.exact_blocker = "dex_invoked_method_iget_truncated";
          result.errors.push_back("dex_invoked_method_iget_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size() ||
            object_register >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_iget_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_iget_register_out_of_range");
          return result;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          result.execution_state = "invoke_receiver_missing";
          result.exact_blocker =
              "dex_invoked_method_iget_object_placeholder_missing";
          result.errors.push_back(
              "dex_invoked_method_iget_object_placeholder_missing");
          return result;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &result.errors)) {
          result.execution_state = "field_resolution_failed";
          result.exact_blocker =
              result.errors.empty()
                  ? "dex_invoked_method_iget_field_resolution_failed"
                  : result.errors.front();
          return result;
        }
        auto loaded_value = load_or_materialize_object_field(
            registers[object_register].object_id,
            registers[object_register].class_descriptor, field_class_descriptor,
            field_name, field_signature, "dex_invoked_method_iget_object");
        if (!result.exact_blocker.empty() && result.exact_blocker != "none") {
          return result;
        }
        registers[destination] = loaded_value;
        const std::string operation =
            result.object_register_field_operation ==
                    "new-instance+invoke-direct+iput"
                ? "new-instance+invoke-direct+iput+iget"
            : (result.object_register_field_operation ==
                       "new-instance+invoke-direct"
                   ? "new-instance+invoke-direct+iget"
            : (result.object_register_field_operation == "new-instance+iput"
                   ? "new-instance+iput+iget"
                   : "new-instance+iget"));
        mark_object_field_operation(
            operation, "object-placeholder",
            "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint",
            registers[object_register].class_descriptor, field_class_descriptor,
            field_name, field_signature);
        pc += 2u;
        continue;
      }
      case 0x53: {  // iget-wide
        if (pc + 1u >= insns_size) {
          result.execution_state = "field_load_truncated";
          result.exact_blocker = "dex_invoked_method_iget_wide_truncated";
          result.errors.push_back("dex_invoked_method_iget_wide_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination + 1u >= registers.size() ||
            object_register >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_iget_wide_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_iget_wide_register_out_of_range");
          return result;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          result.execution_state = "invoke_receiver_missing";
          result.exact_blocker =
              "dex_invoked_method_iget_wide_object_placeholder_missing";
          result.errors.push_back(
              "dex_invoked_method_iget_wide_object_placeholder_missing");
          return result;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &result.errors)) {
          result.execution_state = "field_resolution_failed";
          result.exact_blocker =
              result.errors.empty()
                  ? "dex_invoked_method_iget_wide_field_resolution_failed"
                  : result.errors.front();
          return result;
        }
        auto loaded_value = load_or_materialize_object_field(
            registers[object_register].object_id,
            registers[object_register].class_descriptor, field_class_descriptor,
            field_name, field_signature, "dex_invoked_method_iget_wide_object");
        if (!result.exact_blocker.empty() && result.exact_blocker != "none") {
          return result;
        }
        DexRegisterValue loaded_high = {.kind = DexRegisterValue::Kind::kInt,
                                        .int_value = 0};
        const auto object_it = objects->find(registers[object_register].object_id);
        if (object_it != objects->end()) {
          const std::string field_key =
              field_class_descriptor + "->" + field_name + ":" + field_signature;
          const auto high_it =
              object_it->second.fields.find(field_key + "#high");
          if (high_it != object_it->second.fields.end()) {
            loaded_high = high_it->second;
          }
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = loaded_value.kind == DexRegisterValue::Kind::kInt
                             ? loaded_value.int_value
                             : 0};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = loaded_high.kind == DexRegisterValue::Kind::kInt
                             ? loaded_high.int_value
                             : 0};
        mark_object_field_operation(
            "new-instance+iget-wide", "object-placeholder",
            "linuxoid_placeholder_object_and_wide_field_state_for_minimal_checkpoint",
            registers[object_register].class_descriptor, field_class_descriptor,
            field_name, field_signature);
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed placeholder wide object field access inside an invoked method");
        pc += 2u;
        continue;
      }
      case 0x54: {  // iget-object
        if (pc + 1u >= insns_size) {
          result.execution_state = "field_load_truncated";
          result.exact_blocker = "dex_invoked_method_iget_object_truncated";
          result.errors.push_back("dex_invoked_method_iget_object_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size() ||
            object_register >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_iget_object_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_iget_object_register_out_of_range");
          return result;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          result.execution_state = "invoke_receiver_missing";
          result.exact_blocker =
              "dex_invoked_method_iget_object_placeholder_missing";
          result.errors.push_back(
              "dex_invoked_method_iget_object_placeholder_missing");
          return result;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &result.errors)) {
          result.execution_state = "field_resolution_failed";
          result.exact_blocker =
              result.errors.empty()
                  ? "dex_invoked_method_iget_object_field_resolution_failed"
                  : result.errors.front();
          return result;
        }
        auto loaded_value = load_or_materialize_object_field(
            registers[object_register].object_id,
            registers[object_register].class_descriptor, field_class_descriptor,
            field_name, field_signature, "dex_invoked_method_iget_object");
        if (!result.exact_blocker.empty() && result.exact_blocker != "none") {
          return result;
        }
        registers[destination] = loaded_value;
        const std::string operation =
            result.object_register_field_operation ==
                    "new-instance+invoke-direct+iput-object"
                ? "new-instance+invoke-direct+iput-object+iget-object"
                : "new-instance+iget-object";
        mark_object_field_operation(
            operation, "object-placeholder",
            "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint",
            registers[object_register].class_descriptor, field_class_descriptor,
            field_name, field_signature);
        pc += 2u;
        continue;
      }
      case 0x55: {  // iget-boolean
        if (pc + 1u >= insns_size) {
          result.execution_state = "field_load_truncated";
          result.exact_blocker = "dex_invoked_method_iget_boolean_truncated";
          result.errors.push_back("dex_invoked_method_iget_boolean_truncated");
          return result;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size() ||
            object_register >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_iget_boolean_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_iget_boolean_register_out_of_range");
          return result;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          result.execution_state = "invoke_receiver_missing";
          result.exact_blocker =
              "dex_invoked_method_iget_boolean_object_placeholder_missing";
          result.errors.push_back(
              "dex_invoked_method_iget_boolean_object_placeholder_missing");
          return result;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &result.errors)) {
          result.execution_state = "field_resolution_failed";
          result.exact_blocker =
              result.errors.empty()
                  ? "dex_invoked_method_iget_boolean_field_resolution_failed"
                  : result.errors.front();
          return result;
        }
        auto loaded_value = load_or_materialize_object_field(
            registers[object_register].object_id,
            registers[object_register].class_descriptor, field_class_descriptor,
            field_name, field_signature,
            "dex_invoked_method_iget_boolean_object");
        if (!result.exact_blocker.empty() && result.exact_blocker != "none") {
          return result;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value =
                (loaded_value.kind == DexRegisterValue::Kind::kObject ||
                 (loaded_value.kind == DexRegisterValue::Kind::kInt &&
                  loaded_value.int_value != 0))
                    ? 1
                    : 0};
        const std::string operation =
            result.object_register_field_operation ==
                    "new-instance+invoke-direct+iput"
                ? "new-instance+invoke-direct+iput+iget-boolean"
            : (result.object_register_field_operation ==
                       "new-instance+invoke-direct"
                   ? "new-instance+invoke-direct+iget-boolean"
            : (result.object_register_field_operation == "new-instance+iput"
                   ? "new-instance+iput+iget-boolean"
                   : "new-instance+iget-boolean"));
        mark_object_field_operation(
            operation, "object-placeholder",
            "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint",
            registers[object_register].class_descriptor, field_class_descriptor,
            field_name, field_signature);
        pc += 2u;
        continue;
      }
      case 0x5a: {  // iput-wide
        if (pc + 1u >= insns_size) {
          result.execution_state = "field_store_truncated";
          result.exact_blocker = "dex_invoked_method_iput_wide_truncated";
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        const std::uint32_t value_register =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (value_register + 1u >= registers.size() ||
            object_register >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_iput_wide_register_out_of_range";
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          result.execution_state = "invoke_receiver_missing";
          result.exact_blocker =
              "dex_invoked_method_iput_wide_object_missing";
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        if (registers[value_register].kind != DexRegisterValue::Kind::kInt ||
            registers[value_register + 1u].kind != DexRegisterValue::Kind::kInt) {
          result.execution_state = "iput_wide_source_invalid";
          result.exact_blocker =
              "dex_invoked_method_iput_wide_source_invalid";
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &result.errors)) {
          result.execution_state = "field_resolution_failed";
          result.exact_blocker =
              result.errors.empty()
                  ? "dex_invoked_method_iput_wide_field_resolution_failed"
                  : result.errors.front();
          return result;
        }
        auto object_it = objects->find(registers[object_register].object_id);
        if (object_it == objects->end()) {
          result.execution_state = "object_identity_missing";
          result.exact_blocker =
              "dex_invoked_method_iput_wide_object_identity_missing";
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        const std::string field_key =
            field_class_descriptor + "->" + field_name + ":" + field_signature;
        object_it->second.fields[field_key] = registers[value_register];
        object_it->second.fields[field_key + "#high"] =
            registers[value_register + 1u];
        mark_object_field_operation(
            "new-instance+iput-wide", "object-placeholder",
            "linuxoid_placeholder_object_and_wide_field_state_for_minimal_checkpoint",
            object_it->second.class_descriptor, field_class_descriptor,
            field_name, field_signature);
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe stored a placeholder wide value into an instance field inside an invoked method");
        pc += 2u;
        continue;
      }
      case 0x59:
      case 0x5b: {  // iput / iput-object
        if (pc + 1u >= insns_size) {
          result.execution_state = "field_store_truncated";
          result.exact_blocker = opcode == 0x59
                                     ? "dex_invoked_method_iput_truncated"
                                     : "dex_invoked_method_iput_object_truncated";
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        const std::uint32_t value_register =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (value_register >= registers.size() ||
            object_register >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_register_out_of_range";
          result.errors.push_back("dex_invoked_method_register_out_of_range");
          return result;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          result.execution_state = "invoke_receiver_missing";
          result.exact_blocker =
              opcode == 0x59 ? "dex_invoked_method_iput_object_missing"
                             : "dex_invoked_method_iput_object_reference_missing";
          result.errors.push_back(result.exact_blocker);
          return result;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &result.errors)) {
          result.execution_state = "field_resolution_failed";
          result.exact_blocker =
              result.errors.empty()
                  ? "dex_invoked_method_field_resolution_failed"
                  : result.errors.front();
          return result;
        }
        auto object_it = objects->find(registers[object_register].object_id);
        if (object_it == objects->end()) {
          result.execution_state = "object_identity_missing";
          result.exact_blocker =
              "dex_invoked_method_object_identity_missing";
          result.errors.push_back("dex_invoked_method_object_identity_missing");
          return result;
        }
        const std::string field_key =
            field_class_descriptor + "->" + field_name + ":" + field_signature;
        object_it->second.fields[field_key] = registers[value_register];
        const std::string operation =
            result.object_register_field_operation == "new-instance+invoke-direct"
                ? "new-instance+invoke-direct+iput"
                : "new-instance+iput";
        mark_object_field_operation(
            operation, "object-placeholder",
            "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint",
            object_it->second.class_descriptor, field_class_descriptor,
            field_name, field_signature);
        pc += 2u;
        continue;
      }
      case 0x70: {  // invoke-direct
        if (pc + 2u >= insns_size) {
          result.execution_state = "invoke_truncated";
          result.exact_blocker = "dex_invoked_method_invoke_truncated";
          result.errors.push_back("dex_invoked_method_invoke_truncated");
          return result;
        }
        const std::uint16_t method_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint16_t register_word =
            ReadLe16(bytes, insns_off + (pc + 2u) * 2u);
        const std::uint32_t register_count =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint32_t registers_used[5] = {
            static_cast<std::uint32_t>(register_word & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 4u) & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 8u) & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 12u) & 0x0fu),
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu),
        };
        for (std::uint32_t index = 0; index < register_count && index < 5u;
             ++index) {
          if (registers_used[index] >= registers.size()) {
            result.execution_state = "register_out_of_range";
            result.exact_blocker =
                "dex_invoked_method_invoke_register_out_of_range";
            result.errors.push_back(
                "dex_invoked_method_invoke_register_out_of_range");
            return result;
          }
        }
        if (register_count == 0u ||
            registers[registers_used[0]].kind !=
                DexRegisterValue::Kind::kObject) {
          result.execution_state = "invoke_receiver_missing";
          result.exact_blocker = "dex_invoked_method_invoke_receiver_missing";
          result.errors.push_back("dex_invoked_method_invoke_receiver_missing");
          return result;
        }
        DexExecutionCandidate invoked_method;
        if (!ResolveMethodReference(tables, method_index, &invoked_method,
                                    &result.errors)) {
          result.execution_state = "invoke_unresolved";
          result.exact_blocker =
              result.errors.empty() ? "dex_invoked_method_resolution_failed"
                                    : result.errors.front();
          return result;
        }
        std::vector<DexRegisterValue> invoked_registers;
        invoked_registers.reserve(register_count);
        for (std::uint32_t index = 0; index < register_count && index < 5u;
             ++index) {
          invoked_registers.push_back(registers[registers_used[index]]);
        }
        const auto invoked_result =
            execute_or_stub_invoked_method(invoked_method, invoked_registers);
        result.decoded_instruction_count +=
            invoked_result.decoded_instruction_count;
        result.executed_instruction_count +=
            invoked_result.executed_instruction_count;
        result.used_stubbed_boundary =
            result.used_stubbed_boundary || invoked_result.used_stubbed_boundary;
        if (result.stubbed_boundary_reason == "none" &&
            invoked_result.stubbed_boundary_reason != "none") {
          result.stubbed_boundary_reason =
              invoked_result.stubbed_boundary_reason;
        }
        for (const auto& diagnostic : invoked_result.diagnostics) {
          AppendUnique(&result.diagnostics, diagnostic);
        }
        for (const auto& error : invoked_result.errors) {
          AppendUnique(&result.errors, error);
        }
        if (!invoked_result.ready || !invoked_result.reached_return) {
          result.execution_state = invoked_result.execution_state;
          result.exact_blocker = invoked_result.exact_blocker;
          return result;
        }
        if (invoked_method.method_name == "<init>") {
          std::string operation = "new-instance+invoke-direct";
          if (invoked_result.object_register_field_operation ==
              "new-instance+iput") {
            operation = "new-instance+invoke-direct+iput";
          } else if (invoked_result.object_register_field_operation ==
                     "new-instance+iput-object") {
            operation = "new-instance+invoke-direct+iput-object";
          }
          mark_object_field_operation(
              operation,
              invoked_result.object_register_field_state == "not_reached"
                  ? "object-placeholder"
                  : invoked_result.object_register_field_state,
              invoked_result.object_register_field_reason == "none"
                  ? "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint"
                  : invoked_result.object_register_field_reason,
              invoked_result.object_class_descriptor.empty()
                  ? invoked_method.class_descriptor
                  : invoked_result.object_class_descriptor,
              invoked_result.field_class_descriptor,
              invoked_result.field_name,
              invoked_result.field_signature);
        } else if (invoked_result.object_register_field_state != "not_reached") {
          mark_object_field_operation(
              invoked_result.object_register_field_operation,
              invoked_result.object_register_field_state,
              invoked_result.object_register_field_reason,
              invoked_result.object_class_descriptor,
              invoked_result.field_class_descriptor,
              invoked_result.field_name,
              invoked_result.field_signature);
        }
        pending_result = invoked_result.returned_value;
        pending_result_high = invoked_result.returned_value_high;
        pending_result_is_wide = invoked_result.returned_value_is_wide;
        pending_result_valid =
            DetermineReturnTypeDescriptor(invoked_method.method_signature) != "V";
        pc += 3u;
        continue;
      }
      case 0x6e:
      case 0x6f:
      case 0x71:
      case 0x72: {  // invoke-virtual / invoke-super / invoke-static / invoke-interface
        if (pc + 2u >= insns_size) {
          result.execution_state = "invoke_truncated";
          result.exact_blocker = "dex_invoked_method_invoke_truncated";
          result.errors.push_back("dex_invoked_method_invoke_truncated");
          return result;
        }
        const std::uint16_t method_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint16_t register_word =
            ReadLe16(bytes, insns_off + (pc + 2u) * 2u);
        const std::uint32_t register_count =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint32_t registers_used[5] = {
            static_cast<std::uint32_t>(register_word & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 4u) & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 8u) & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 12u) & 0x0fu),
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu),
        };
        for (std::uint32_t index = 0; index < register_count && index < 5u;
             ++index) {
          if (registers_used[index] >= registers.size()) {
            result.execution_state = "register_out_of_range";
            result.exact_blocker =
                "dex_invoked_method_invoke_register_out_of_range";
            result.errors.push_back(
                "dex_invoked_method_invoke_register_out_of_range");
            return result;
          }
        }
        if (opcode != 0x71 &&
            (register_count == 0u ||
             registers[registers_used[0]].kind !=
                 DexRegisterValue::Kind::kObject)) {
          result.execution_state = "invoke_receiver_missing";
          result.exact_blocker = "dex_invoked_method_invoke_receiver_missing";
          result.errors.push_back("dex_invoked_method_invoke_receiver_missing");
          return result;
        }
        DexExecutionCandidate invoked_method;
        if (!ResolveMethodReference(tables, method_index, &invoked_method,
                                    &result.errors)) {
          result.execution_state = "invoke_unresolved";
          result.exact_blocker =
              result.errors.empty() ? "dex_invoked_method_resolution_failed"
                                    : result.errors.front();
          return result;
        }
        std::vector<DexRegisterValue> invoked_registers;
        invoked_registers.reserve(register_count);
        for (std::uint32_t index = 0; index < register_count && index < 5u;
             ++index) {
          invoked_registers.push_back(registers[registers_used[index]]);
        }
        const auto invoked_result =
            execute_or_stub_invoked_method(invoked_method, invoked_registers);
        result.decoded_instruction_count +=
            invoked_result.decoded_instruction_count;
        result.executed_instruction_count +=
            invoked_result.executed_instruction_count;
        result.used_stubbed_boundary =
            result.used_stubbed_boundary || invoked_result.used_stubbed_boundary;
        if (result.stubbed_boundary_reason == "none" &&
            invoked_result.stubbed_boundary_reason != "none") {
          result.stubbed_boundary_reason =
              invoked_result.stubbed_boundary_reason;
        }
        for (const auto& diagnostic : invoked_result.diagnostics) {
          AppendUnique(&result.diagnostics, diagnostic);
        }
        for (const auto& error : invoked_result.errors) {
          AppendUnique(&result.errors, error);
        }
        if (!invoked_result.ready || !invoked_result.reached_return) {
          result.execution_state = invoked_result.execution_state;
          result.exact_blocker = invoked_result.exact_blocker;
          return result;
        }
        if (invoked_result.object_register_field_state != "not_reached") {
          mark_object_field_operation(
              invoked_result.object_register_field_operation,
              invoked_result.object_register_field_state,
              invoked_result.object_register_field_reason,
              invoked_result.object_class_descriptor,
              invoked_result.field_class_descriptor,
              invoked_result.field_name,
              invoked_result.field_signature);
        }
        pending_result = invoked_result.returned_value;
        pending_result_high = invoked_result.returned_value_high;
        pending_result_is_wide = invoked_result.returned_value_is_wide;
        pending_result_valid =
            DetermineReturnTypeDescriptor(invoked_method.method_signature) != "V";
        pc += 3u;
        continue;
      }
      case 0x77: {  // invoke-static/range
        if (pc + 2u >= insns_size) {
          result.execution_state = "invoke_truncated";
          result.exact_blocker = "dex_invoked_method_invoke_range_truncated";
          result.errors.push_back("dex_invoked_method_invoke_range_truncated");
          return result;
        }
        const std::uint16_t method_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint16_t first_register =
            ReadLe16(bytes, insns_off + (pc + 2u) * 2u);
        const std::uint32_t register_count =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (static_cast<std::size_t>(first_register) +
                static_cast<std::size_t>(register_count) >
            registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_invoke_range_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_invoke_range_register_out_of_range");
          return result;
        }
        DexExecutionCandidate invoked_method;
        if (!ResolveMethodReference(tables, method_index, &invoked_method,
                                    &result.errors)) {
          result.execution_state = "invoke_unresolved";
          result.exact_blocker =
              result.errors.empty() ? "dex_invoked_method_invoke_range_resolution_failed"
                                    : result.errors.front();
          return result;
        }
        result.invoked_method_class_descriptor = invoked_method.class_descriptor;
        result.invoked_method_name = invoked_method.method_name;
        result.invoked_method_signature = invoked_method.method_signature;
        std::vector<DexRegisterValue> invoked_registers;
        invoked_registers.reserve(register_count);
        for (std::uint32_t index = 0; index < register_count; ++index) {
          invoked_registers.push_back(
              registers[static_cast<std::size_t>(first_register) + index]);
        }
        const auto invoked_result =
            execute_or_stub_invoked_method(invoked_method, invoked_registers);
        result.decoded_instruction_count +=
            invoked_result.decoded_instruction_count;
        result.executed_instruction_count +=
            invoked_result.executed_instruction_count;
        if (!invoked_result.errors.empty()) {
          for (const auto& error : invoked_result.errors) {
            AppendUnique(&result.errors, error);
          }
        }
        for (const auto& diagnostic : invoked_result.diagnostics) {
          AppendUnique(&result.diagnostics, diagnostic);
        }
        if (!invoked_result.ready || !invoked_result.reached_return) {
          result.execution_state = invoked_result.execution_state;
          result.exact_blocker = invoked_result.exact_blocker;
          if (result.stubbed_boundary_reason == "none") {
            result.stubbed_boundary_reason =
                invoked_result.stubbed_boundary_reason;
          }
          return result;
        }
        if (invoked_result.object_register_field_state != "not_reached") {
          mark_object_field_operation(
              invoked_result.object_register_field_operation,
              invoked_result.object_register_field_state,
              invoked_result.object_register_field_reason,
              invoked_result.object_class_descriptor,
              invoked_result.field_class_descriptor,
              invoked_result.field_name,
              invoked_result.field_signature);
        }
        if (invoked_result.used_stubbed_boundary) {
          result.used_stubbed_boundary = true;
          if (result.stubbed_boundary_reason == "none") {
            result.stubbed_boundary_reason =
                invoked_result.stubbed_boundary_reason;
          }
        }
        pending_result = invoked_result.returned_value;
        pending_result_high = invoked_result.returned_value_high;
        pending_result_is_wide = invoked_result.returned_value_is_wide;
        pending_result_valid =
            DetermineReturnTypeDescriptor(invoked_method.method_signature) !=
            "V";
        AppendUnique(
            &result.diagnostics,
            "Self-Healing Android Device DEX probe executed an invoke-static/range method inside an invoked method");
        pc += 3u;
        continue;
      }
      case 0x0e:  // return-void
        result.returned_value = {.kind = DexRegisterValue::Kind::kUnknown};
        result.reached_return = true;
        result.execution_state = "returned";
        result.exact_blocker = "none";
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed an app-local invoked method");
        return result;
      case 0x0f: {  // return
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker = "dex_invoked_method_register_out_of_range";
          result.errors.push_back("dex_invoked_method_register_out_of_range");
          return result;
        }
        result.returned_value = registers[source];
        if (result.returned_value.kind == DexRegisterValue::Kind::kUnknown &&
            return_type_descriptor == "I") {
          result.returned_value = {.kind = DexRegisterValue::Kind::kInt,
                                   .int_value = 0};
        }
        result.reached_return = true;
        result.execution_state = "returned";
        result.exact_blocker = "none";
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed an app-local invoked method");
        return result;
      }
      case 0x11: {  // return-object
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (source >= registers.size()) {
          result.execution_state = "register_out_of_range";
          result.exact_blocker =
              "dex_invoked_method_return_object_register_out_of_range";
          result.errors.push_back(
              "dex_invoked_method_return_object_register_out_of_range");
          return result;
        }
        result.returned_value = registers[source];
        if (result.returned_value.kind == DexRegisterValue::Kind::kUnknown &&
            IsReferenceTypeDescriptor(return_type_descriptor)) {
          result.returned_value = MaterializePlaceholderObjectValue(
              return_type_descriptor, &next_object_id, objects);
          result.used_stubbed_boundary = true;
          if (result.stubbed_boundary_reason == "none") {
            result.stubbed_boundary_reason =
                "placeholder_return_object_materialized_for_minimal_checkpoint";
          }
          result.diagnostics.push_back(
              "Self-Healing Android Device DEX probe materialized a placeholder return object for an app-local invoked method");
        }
        result.reached_return = true;
        result.execution_state = "returned";
        result.exact_blocker = "none";
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed an app-local invoked method");
        return result;
      }
      default:
        result.execution_state = "unsupported_opcode";
        result.exact_blocker = "unsupported-dex-opcode:" + opcode_name;
        result.diagnostics.push_back(
            "Self-Healing Android Device DEX probe reached an exact unsupported opcode boundary inside an app-local invoked method");
        return result;
    }
  }

  result.execution_state = "fell_off_end";
  result.exact_blocker = "dex_invoked_method_fell_off_end";
  result.errors.push_back("dex_invoked_method_fell_off_end");
  return result;
}

std::string DetermineLookupStateFromBlocker(const std::string& blocker) {
  if (blocker == "dex_entrypoint_class_missing") {
    return "class_missing";
  }
  if (blocker == "dex_entrypoint_class_unknown") {
    return "class_unknown";
  }
  if (blocker == "dex_entrypoint_class_data_missing") {
    return "class_data_missing";
  }
  if (blocker == "dex_entrypoint_method_missing") {
    return "method_missing";
  }
  if (blocker == "dex_entrypoint_code_item_missing") {
    return "code_item_missing";
  }
  if (blocker == "dex_code_item_truncated") {
    return "code_item_truncated";
  }
  if (blocker == "dex_instructions_truncated") {
    return "instructions_truncated";
  }
  if (blocker == "dex_table_bounds_invalid" ||
      blocker == "dex_string_data_invalid" ||
      blocker == "dex_type_descriptor_invalid" ||
      blocker == "dex_class_descriptor_invalid" ||
      blocker == "dex_class_string_invalid" ||
      blocker == "dex_class_data_truncated" ||
      blocker == "dex_field_data_truncated" ||
      blocker == "dex_method_data_truncated" ||
      blocker == "dex_method_index_invalid") {
    return "dex_tables_invalid";
  }
  return "blocked";
}

NativeApkDexExecutionProbeReport RunExecutionProbe(
    const std::string& bytes, const NativeApkDexBridgeSessionConfig& config,
    const NativeApkDexFileReport& file, const ParsedDexTables& tables) {
  NativeApkDexExecutionProbeReport probe;
  probe.target_class_descriptor = config.entrypoint_class_descriptor;
  probe.target_method_name = config.entrypoint_method_name;
  probe.parse_state = "header_tables_methods_and_code_item";

  if (!file.valid_dex_magic) {
    probe.class_loading_state = "blocked";
    probe.target_class_lookup_state = "dex_magic_invalid";
    probe.execution_state = "dex_magic_invalid";
    probe.parse_state = "dex_magic_invalid";
    probe.exact_blocker = "dex_magic_invalid";
    probe.errors.push_back("dex_magic_invalid");
    return probe;
  }
  if (probe.target_class_descriptor.empty()) {
    probe.class_loading_state = "blocked";
    probe.target_class_lookup_state = "class_unknown";
    probe.execution_state = "entrypoint_class_unknown";
    probe.parse_state = "entrypoint_class_unknown";
    probe.exact_blocker = "dex_entrypoint_class_unknown";
    probe.errors.push_back("dex_entrypoint_class_unknown");
    return probe;
  }

  const bool class_present =
      std::find(tables.class_descriptors.begin(), tables.class_descriptors.end(),
                probe.target_class_descriptor) != tables.class_descriptors.end();
  if (!class_present) {
    probe.ready = true;
    probe.class_loading_state = "blocked";
    probe.target_class_lookup_state = "class_missing";
    probe.execution_state = "entrypoint_missing";
    probe.parse_state = "entrypoint_class_missing";
    probe.exact_blocker = "dex_entrypoint_class_missing";
    probe.errors.push_back("dex_entrypoint_class_missing");
    AppendUnique(&probe.diagnostics,
                 "Self-Healing Android Device DEX probe resolved the real activity target but could not find its class descriptor in staged DEX");
    return probe;
  }
  probe.class_loading_state = "resolved-from-staged-dex";
  probe.target_class_lookup_state = "class_resolved";

  DexExecutionCandidate candidate;
  if (!FindEntrypointMethod(bytes, tables, probe.target_class_descriptor,
                            probe.target_method_name, &candidate,
                            &probe.errors)) {
    probe.ready = true;
    probe.target_method_lookup_state =
        DetermineLookupStateFromBlocker(probe.errors.empty()
                                            ? "dex_entrypoint_missing"
                                            : probe.errors.front());
    if (probe.target_method_lookup_state == "method_missing" ||
        probe.target_method_lookup_state == "class_data_missing") {
      probe.parse_state = probe.errors.empty() ? "entrypoint_method_missing"
                                               : probe.errors.front().substr(4);
    } else {
      probe.parse_state = probe.errors.empty() ? "entrypoint_lookup_blocked"
                                               : probe.errors.front().substr(4);
    }
    if (probe.parse_state == "entrypoint_class_data_missing") {
      probe.target_method_lookup_state = "class_data_missing";
    }
    probe.execution_state = "entrypoint_missing";
    probe.exact_blocker = probe.errors.empty() ? "dex_entrypoint_missing"
                                               : probe.errors.front();
    return probe;
  }

  probe.ready = true;
  probe.target_method_found = true;
  probe.target_method_lookup_state = "method_resolved";
  probe.target_method_signature = candidate.method_signature;
  probe.code_item_found = candidate.code_off != 0;
  probe.code_item_offset = candidate.code_off;
  if (candidate.code_off == 0) {
    probe.code_item_lookup_state = "code_item_missing";
    probe.parse_state = "entrypoint_code_item_missing";
    probe.execution_state = "code_item_missing";
    probe.exact_blocker = "dex_entrypoint_code_item_missing";
    probe.errors.push_back("dex_entrypoint_code_item_missing");
    return probe;
  }

  if (candidate.code_off + 16u > bytes.size()) {
    probe.code_item_lookup_state = "code_item_truncated";
    probe.parse_state = "entrypoint_code_item_truncated";
    probe.execution_state = "code_item_truncated";
    probe.exact_blocker = "dex_code_item_truncated";
    probe.errors.push_back("dex_code_item_truncated");
    return probe;
  }

  const std::uint32_t insns_size = ReadLe32(bytes, candidate.code_off + 12u);
  const std::size_t insns_off = static_cast<std::size_t>(candidate.code_off) + 16u;
  if (insns_off + static_cast<std::size_t>(insns_size) * 2u > bytes.size()) {
    probe.code_item_lookup_state = "instructions_truncated";
    probe.parse_state = "entrypoint_instructions_truncated";
    probe.execution_state = "instructions_truncated";
    probe.exact_blocker = "dex_instructions_truncated";
    probe.errors.push_back("dex_instructions_truncated");
    return probe;
  }

  probe.code_item_lookup_state = "code_item_resolved";
  probe.parse_state = "entrypoint_code_item_resolved";
  probe.execution_attempted = true;
  probe.execution_state = "interpreting";
  const std::string return_type_descriptor =
      DetermineReturnTypeDescriptor(probe.target_method_signature);
  const std::vector<std::string> parameter_type_descriptors =
      DetermineParameterTypeDescriptors(probe.target_method_signature);
  const std::uint16_t registers_size =
      ReadLe16(bytes, candidate.code_off + 0u);
  const std::uint16_t ins_size = ReadLe16(bytes, candidate.code_off + 2u);
  std::vector<DexRegisterValue> registers(
      std::max<std::size_t>(registers_size, 16u));
  std::map<std::uint32_t, PlaceholderObject> objects;
  std::map<std::string, DexRegisterValue> static_fields;
  std::uint32_t next_object_id = 1u;
  bool pending_result_valid = false;
  bool pending_result_is_wide = false;
  DexRegisterValue pending_result;
  DexRegisterValue pending_result_high;

  const std::uint32_t lifecycle_receiver_object_id = next_object_id++;
  objects[lifecycle_receiver_object_id] = {
      .object_id = lifecycle_receiver_object_id,
      .class_descriptor = candidate.class_descriptor,
      .fields = {}};
  const bool parameter_register_window_valid =
      ins_size != 0u && registers_size >= ins_size;
  const bool instance_receiver_expected =
      parameter_register_window_valid &&
      ins_size > parameter_type_descriptors.size();
  const std::size_t parameter_register_base =
      parameter_register_window_valid
          ? static_cast<std::size_t>(registers_size - ins_size)
          : 0u;
  const std::size_t lifecycle_receiver_register =
      instance_receiver_expected ? parameter_register_base : 0u;
  registers[lifecycle_receiver_register] = {.kind = DexRegisterValue::Kind::kObject,
                                            .int_value = 0,
                                            .object_id = lifecycle_receiver_object_id,
                                            .class_descriptor = candidate.class_descriptor};
  probe.lifecycle_receiver_state = instance_receiver_expected
                                       ? "receiver-placeholder-materialized-in-parameter-register"
                                       : "receiver-placeholder-materialized";
  probe.lifecycle_receiver_class_descriptor = candidate.class_descriptor;
  probe.lifecycle_receiver_register =
      static_cast<int>(lifecycle_receiver_register);
  AppendUnique(&probe.diagnostics,
               "Self-Healing Android Device DEX probe resolved the lifecycle receiver class from staged DEX metadata");
  AppendUnique(&probe.diagnostics,
               "Self-Healing Android Device DEX probe materialized a deterministic lifecycle receiver placeholder");
  auto load_or_materialize_static_field =
      [&](const std::string& field_class_descriptor,
          const std::string& field_name,
          const std::string& field_signature) -> DexRegisterValue {
    const std::string field_key =
        field_class_descriptor + "->" + field_name + ":" + field_signature;
    const auto field_it = static_fields.find(field_key);
    if (field_it != static_fields.end()) {
      return field_it->second;
    }

    DexRegisterValue loaded_value;
    if (IsReferenceTypeDescriptor(field_signature)) {
      loaded_value = MaterializePlaceholderObjectValue(field_signature,
                                                       &next_object_id, &objects);
    } else {
      loaded_value = {.kind = DexRegisterValue::Kind::kInt, .int_value = 0};
    }
    static_fields[field_key] = loaded_value;
    AppendUnique(
        &probe.diagnostics,
        "Self-Healing Android Device DEX probe materialized a placeholder static field to keep a managed lifecycle checkpoint moving");
    return loaded_value;
  };

  auto store_static_field = [&](const std::string& field_class_descriptor,
                                const std::string& field_name,
                                const std::string& field_signature,
                                const DexRegisterValue& value) {
    const std::string field_key =
        field_class_descriptor + "->" + field_name + ":" + field_signature;
    DexRegisterValue stored_value = value;
    if (field_signature == "Z") {
      stored_value.kind = DexRegisterValue::Kind::kInt;
      stored_value.int_value =
          (value.kind == DexRegisterValue::Kind::kObject ||
           (value.kind == DexRegisterValue::Kind::kInt && value.int_value != 0))
              ? 1
              : 0;
      stored_value.object_id = 0;
      stored_value.class_descriptor.clear();
    }
    static_fields[field_key] = stored_value;
    AppendUnique(
        &probe.diagnostics,
        "Self-Healing Android Device DEX probe recorded a deterministic static field write");
  };
  if (parameter_register_window_valid && !parameter_type_descriptors.empty()) {
    const std::size_t lifecycle_parameter_register =
        parameter_register_base + (instance_receiver_expected ? 1u : 0u);
    if (lifecycle_parameter_register < registers.size()) {
      const std::string& parameter_descriptor =
          parameter_type_descriptors.front();
      if (!parameter_descriptor.empty() &&
          (parameter_descriptor.front() == 'L' ||
           parameter_descriptor.front() == '[')) {
        const std::uint32_t lifecycle_parameter_object_id = next_object_id++;
        objects[lifecycle_parameter_object_id] = {
            .object_id = lifecycle_parameter_object_id,
            .class_descriptor = parameter_descriptor,
            .fields = {}};
        registers[lifecycle_parameter_register] = {
            .kind = DexRegisterValue::Kind::kObject,
            .int_value = 0,
            .object_id = lifecycle_parameter_object_id,
            .class_descriptor = parameter_descriptor};
        probe.lifecycle_parameter_state =
            "parameter-placeholder-materialized";
        probe.lifecycle_parameter_class_descriptor = parameter_descriptor;
        probe.lifecycle_parameter_register =
            static_cast<int>(lifecycle_parameter_register);
        AppendUnique(&probe.diagnostics,
                     "Self-Healing Android Device DEX probe materialized a deterministic lifecycle parameter placeholder");
      }
    }
  }

  auto mark_object_field_operation = [&](const std::string& operation,
                                         const std::string& state,
                                         const std::string& reason,
                                         const std::string& object_class,
                                         const std::string& field_class,
                                         const std::string& field_name,
                                         const std::string& field_signature) {
    probe.object_register_field_operation = operation;
    probe.object_register_field_state = state;
    probe.object_register_field_reason = reason;
    if (!object_class.empty()) {
      probe.object_class_descriptor = object_class;
    }
    if (!field_class.empty()) {
      probe.field_class_descriptor = field_class;
    }
    if (!field_name.empty()) {
      probe.field_name = field_name;
    }
    if (!field_signature.empty()) {
      probe.field_signature = field_signature;
    }
  };

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
    probe.last_instruction_offset = pc * 2u;
    probe.last_opcode_value = opcode;
    probe.last_opcode_name = opcode_name;
    ++probe.decoded_instruction_count;
    ++probe.executed_instruction_count;

    switch (opcode) {
      case 0x00:  // nop
        ++pc;
        continue;
      case 0x01: {  // move
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination >= registers.size() || source >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_move_register_out_of_range";
          probe.errors.push_back("dex_move_register_out_of_range");
          return probe;
        }
        registers[destination] = registers[source];
        AppendUnique(&probe.diagnostics,
                     "Self-Healing Android Device DEX probe executed placeholder move");
        ++pc;
        continue;
      }
      case 0x12: {  // const/4
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        std::int32_t literal =
            static_cast<std::int32_t>((code_unit >> 12u) & 0x0fu);
        if (literal >= 8) {
          literal -= 16;
        }
        if (destination >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_register_out_of_range";
          probe.errors.push_back("dex_register_out_of_range");
          return probe;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = literal};
        ++pc;
        continue;
      }
      case 0x13: {  // const/16
        if (pc + 1u >= insns_size) {
          probe.execution_state = "const16_truncated";
          probe.exact_blocker = "dex_const16_truncated";
          probe.errors.push_back("dex_const16_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::int16_t literal = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        if (destination >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_const16_register_out_of_range";
          probe.errors.push_back("dex_const16_register_out_of_range");
          return probe;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = literal};
        pc += 2u;
        continue;
      }
      case 0x16: {  // const-wide/16
        if (pc + 1u >= insns_size) {
          probe.execution_state = "const_wide16_truncated";
          probe.exact_blocker = "dex_const_wide16_truncated";
          probe.errors.push_back("dex_const_wide16_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::int64_t literal = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        if (destination + 1u >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_const_wide16_register_out_of_range";
          probe.errors.push_back("dex_const_wide16_register_out_of_range");
          return probe;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(literal & 0xffffffffll)};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value =
                static_cast<std::int32_t>((literal >> 32) & 0xffffffffll)};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder const-wide/16");
        pc += 2u;
        continue;
      }
      case 0x84: {  // long-to-int
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination >= registers.size() || source + 1u >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_long_to_int_register_out_of_range";
          probe.errors.push_back("dex_long_to_int_register_out_of_range");
          return probe;
        }
        if (registers[source].kind != DexRegisterValue::Kind::kInt ||
            registers[source + 1u].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "long_to_int_source_invalid";
          probe.exact_blocker = "dex_long_to_int_source_invalid";
          probe.errors.push_back("dex_long_to_int_source_invalid");
          return probe;
        }
        const std::uint64_t source_low =
            static_cast<std::uint32_t>(registers[source].int_value);
        const std::uint64_t source_high =
            static_cast<std::uint32_t>(registers[source + 1u].int_value);
        const std::int64_t source_value =
            static_cast<std::int64_t>((source_high << 32u) | source_low);
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(source_value)};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder long-to-int");
        ++pc;
        continue;
      }
      case 0x1a: {  // const-string
        if (pc + 1u >= insns_size) {
          probe.execution_state = "const_string_truncated";
          probe.exact_blocker = "dex_const_string_truncated";
          probe.errors.push_back("dex_const_string_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t string_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_const_string_register_out_of_range";
          probe.errors.push_back("dex_const_string_register_out_of_range");
          return probe;
        }
        if (string_index >= tables.strings.size()) {
          probe.execution_state = "string_resolution_failed";
          probe.exact_blocker = "dex_const_string_resolution_failed";
          probe.errors.push_back("dex_const_string_resolution_failed");
          return probe;
        }
        registers[destination] = MaterializePlaceholderObjectValue(
            "Ljava/lang/String;", &next_object_id, &objects);
        probe.diagnostics.push_back(
            "Self-Healing Android Device DEX probe materialized a placeholder java.lang.String for const-string");
        ++pc;
        ++pc;
        continue;
      }
      case 0x28: {  // goto
        const std::int8_t branch_offset =
            static_cast<std::int8_t>((code_unit >> 8u) & 0x00ffu);
        const std::int64_t next_pc =
            static_cast<std::int64_t>(pc) + branch_offset;
        if (next_pc < 0 || next_pc >= static_cast<std::int64_t>(insns_size)) {
          probe.execution_state = "branch_out_of_range";
          probe.exact_blocker = "dex_goto_branch_out_of_range";
          probe.errors.push_back("dex_goto_branch_out_of_range");
          return probe;
        }
        pc = static_cast<std::uint32_t>(next_pc);
        continue;
      }
      case 0x21: {  // array-length
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t array_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination >= registers.size() || array_register >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_array_length_register_out_of_range";
          probe.errors.push_back("dex_array_length_register_out_of_range");
          return probe;
        }
        if (registers[array_register].kind != DexRegisterValue::Kind::kObject ||
            registers[array_register].object_id == 0) {
          probe.execution_state = "array_length_operand_invalid";
          probe.exact_blocker = "dex_array_length_operand_invalid";
          probe.errors.push_back("dex_array_length_operand_invalid");
          return probe;
        }
        const auto object_it = objects.find(registers[array_register].object_id);
        if (object_it == objects.end()) {
          probe.execution_state = "array_length_object_missing";
          probe.exact_blocker = "dex_array_length_object_missing";
          probe.errors.push_back("dex_array_length_object_missing");
          return probe;
        }
        if (object_it->second.array_length < 0) {
          probe.execution_state = "array_length_unknown";
          probe.exact_blocker = "dex_array_length_unknown";
          probe.errors.push_back("dex_array_length_unknown");
          return probe;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = object_it->second.array_length};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder array-length");
        ++pc;
        continue;
      }
      case 0x2b: {  // packed-switch
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (source >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_packed_switch_register_out_of_range";
          probe.errors.push_back("dex_packed_switch_register_out_of_range");
          return probe;
        }
        if (registers[source].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "packed_switch_source_invalid";
          probe.exact_blocker = "dex_packed_switch_source_invalid";
          probe.errors.push_back("dex_packed_switch_source_invalid");
          return probe;
        }
        std::uint32_t next_pc = 0u;
        std::string failure_reason;
        if (!ResolvePackedSwitchTarget(bytes, insns_off, insns_size, pc,
                                       registers[source].int_value, &next_pc,
                                       &failure_reason)) {
          probe.execution_state = "packed_switch_payload_invalid";
          probe.exact_blocker = "dex_" + failure_reason;
          probe.errors.push_back(probe.exact_blocker);
          return probe;
        }
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder packed-switch");
        pc = next_pc;
        continue;
      }
      case 0x1f: {  // check-cast
        if (pc + 1u >= insns_size) {
          probe.execution_state = "check_cast_truncated";
          probe.exact_blocker = "dex_check_cast_truncated";
          probe.errors.push_back("dex_check_cast_truncated");
          return probe;
        }
        const std::uint32_t source_register =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t type_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (source_register >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_check_cast_register_out_of_range";
          probe.errors.push_back("dex_check_cast_register_out_of_range");
          return probe;
        }
        if (registers[source_register].kind != DexRegisterValue::Kind::kObject ||
            registers[source_register].object_id == 0) {
          probe.execution_state = "check_cast_operand_invalid";
          probe.exact_blocker = "dex_check_cast_operand_invalid";
          probe.errors.push_back("dex_check_cast_operand_invalid");
          return probe;
        }
        if (type_index >= tables.type_descriptor_string_indices.size()) {
          probe.execution_state = "check_cast_type_invalid";
          probe.exact_blocker = "dex_check_cast_type_index_invalid";
          probe.errors.push_back("dex_check_cast_type_index_invalid");
          return probe;
        }
        const std::uint32_t type_string_index =
            tables.type_descriptor_string_indices[type_index];
        if (type_string_index >= tables.strings.size()) {
          probe.execution_state = "check_cast_type_invalid";
          probe.exact_blocker = "dex_check_cast_type_string_invalid";
          probe.errors.push_back("dex_check_cast_type_string_invalid");
          return probe;
        }
        auto object_it = objects.find(registers[source_register].object_id);
        if (object_it == objects.end()) {
          probe.execution_state = "check_cast_object_missing";
          probe.exact_blocker = "dex_check_cast_object_missing";
          probe.errors.push_back("dex_check_cast_object_missing");
          return probe;
        }
        const std::string target_descriptor = tables.strings[type_string_index];
        const std::string current_descriptor =
            !registers[source_register].class_descriptor.empty()
                ? registers[source_register].class_descriptor
                : object_it->second.class_descriptor;
        const bool compatible =
            current_descriptor == target_descriptor ||
            current_descriptor == "Ljava/lang/Object;" ||
            target_descriptor == "Ljava/lang/Object;";
        if (!compatible) {
          probe.execution_state = "check_cast_type_mismatch";
          probe.exact_blocker = "dex_check_cast_type_mismatch";
          probe.errors.push_back("dex_check_cast_type_mismatch");
          return probe;
        }
        bool placeholder_materialized = false;
        if (current_descriptor == "Ljava/lang/Object;" &&
            target_descriptor != "Ljava/lang/Object;") {
          registers[source_register].class_descriptor = target_descriptor;
          object_it->second.class_descriptor = target_descriptor;
          placeholder_materialized = true;
        }
        mark_object_field_operation(
            "check-cast",
            placeholder_materialized ? "object-placeholder" : "executed",
            placeholder_materialized
                ? "placeholder_check_cast_materialized_for_minimal_checkpoint"
                : "linuxoid_check_cast_verified_for_minimal_checkpoint",
            placeholder_materialized ? target_descriptor : current_descriptor,
            "", "", "");
        if (placeholder_materialized) {
          AppendUnique(
              &probe.diagnostics,
              "Self-Healing Android Device DEX probe materialized a deterministic placeholder check-cast boundary");
        }
        pc += 2u;
        continue;
      }
      case 0x20: {  // instance-of
        if (pc + 1u >= insns_size) {
          probe.execution_state = "instance_of_truncated";
          probe.exact_blocker = "dex_instance_of_truncated";
          probe.errors.push_back("dex_instance_of_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t type_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size() || source_register >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_instance_of_register_out_of_range";
          probe.errors.push_back("dex_instance_of_register_out_of_range");
          return probe;
        }
        if (registers[source_register].kind != DexRegisterValue::Kind::kObject ||
            registers[source_register].object_id == 0) {
          probe.execution_state = "instance_of_operand_invalid";
          probe.exact_blocker = "dex_instance_of_operand_invalid";
          probe.errors.push_back("dex_instance_of_operand_invalid");
          return probe;
        }
        if (type_index >= tables.type_descriptor_string_indices.size()) {
          probe.execution_state = "instance_of_type_invalid";
          probe.exact_blocker = "dex_instance_of_type_index_invalid";
          probe.errors.push_back("dex_instance_of_type_index_invalid");
          return probe;
        }
        const std::uint32_t type_string_index =
            tables.type_descriptor_string_indices[type_index];
        if (type_string_index >= tables.strings.size()) {
          probe.execution_state = "instance_of_type_invalid";
          probe.exact_blocker = "dex_instance_of_type_string_invalid";
          probe.errors.push_back("dex_instance_of_type_string_invalid");
          return probe;
        }
        auto object_it = objects.find(registers[source_register].object_id);
        if (object_it == objects.end()) {
          probe.execution_state = "instance_of_object_missing";
          probe.exact_blocker = "dex_instance_of_object_missing";
          probe.errors.push_back("dex_instance_of_object_missing");
          return probe;
        }
        const std::string target_descriptor = tables.strings[type_string_index];
        const std::string current_descriptor =
            !registers[source_register].class_descriptor.empty()
                ? registers[source_register].class_descriptor
                : object_it->second.class_descriptor;
        bool placeholder_materialized = false;
        int instance_of_result = 0;
        if (current_descriptor == target_descriptor ||
            target_descriptor == "Ljava/lang/Object;") {
          instance_of_result = 1;
        } else if (current_descriptor == "Ljava/lang/Object;" &&
                   target_descriptor != "Ljava/lang/Object;") {
          registers[source_register].class_descriptor = target_descriptor;
          object_it->second.class_descriptor = target_descriptor;
          placeholder_materialized = true;
          instance_of_result = 1;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = instance_of_result};
        mark_object_field_operation(
            "instance-of",
            placeholder_materialized ? "object-placeholder" : "executed",
            placeholder_materialized
                ? "placeholder_instance_of_materialized_for_minimal_checkpoint"
                : "linuxoid_instance_of_verified_for_minimal_checkpoint",
            placeholder_materialized ? target_descriptor : current_descriptor,
            "", "", "");
        if (placeholder_materialized) {
          AppendUnique(
              &probe.diagnostics,
              "Self-Healing Android Device DEX probe materialized a deterministic placeholder instance-of boundary");
        }
        pc += 2u;
        continue;
      }
      case 0x46: {  // aget-object
        if (pc + 1u >= insns_size) {
          probe.execution_state = "aget_object_truncated";
          probe.exact_blocker = "dex_aget_object_truncated";
          probe.errors.push_back("dex_aget_object_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t registers_word =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint32_t array_register =
            static_cast<std::uint32_t>(registers_word & 0x00ffu);
        const std::uint32_t index_register =
            static_cast<std::uint32_t>((registers_word >> 8u) & 0x00ffu);
        if (destination >= registers.size() || array_register >= registers.size() ||
            index_register >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_aget_object_register_out_of_range";
          probe.errors.push_back("dex_aget_object_register_out_of_range");
          return probe;
        }
        if (registers[array_register].kind != DexRegisterValue::Kind::kObject ||
            registers[array_register].object_id == 0) {
          probe.execution_state = "aget_object_operand_invalid";
          probe.exact_blocker = "dex_aget_object_operand_invalid";
          probe.errors.push_back("dex_aget_object_operand_invalid");
          return probe;
        }
        if (registers[index_register].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "aget_object_index_invalid";
          probe.exact_blocker = "dex_aget_object_index_invalid";
          probe.errors.push_back("dex_aget_object_index_invalid");
          return probe;
        }
        auto object_it = objects.find(registers[array_register].object_id);
        if (object_it == objects.end()) {
          probe.execution_state = "aget_object_array_missing";
          probe.exact_blocker = "dex_aget_object_array_missing";
          probe.errors.push_back("dex_aget_object_array_missing");
          return probe;
        }
        const std::int32_t element_index = registers[index_register].int_value;
        const std::string element_descriptor =
            DetermineArrayElementDescriptor(object_it->second.class_descriptor);
        if (!IsReferenceTypeDescriptor(element_descriptor)) {
          probe.execution_state = "aget_object_not_reference_array";
          probe.exact_blocker = "dex_aget_object_not_reference_array";
          probe.errors.push_back("dex_aget_object_not_reference_array");
          return probe;
        }
        bool array_length_materialized = false;
        if (object_it->second.array_length < 0) {
          if (element_index < 0) {
            probe.execution_state = "aget_object_index_out_of_bounds";
            probe.exact_blocker = "dex_aget_object_index_out_of_bounds";
            probe.errors.push_back("dex_aget_object_index_out_of_bounds");
            return probe;
          }
          object_it->second.array_length = element_index + 1;
          array_length_materialized = true;
        }
        if (element_index < 0 ||
            element_index >= object_it->second.array_length) {
          probe.execution_state = "aget_object_index_out_of_bounds";
          probe.exact_blocker = "dex_aget_object_index_out_of_bounds";
          probe.errors.push_back("dex_aget_object_index_out_of_bounds");
          return probe;
        }
        bool placeholder_materialized = false;
        DexRegisterValue loaded_value = {.kind = DexRegisterValue::Kind::kUnknown};
        const auto element_it =
            object_it->second.array_elements.find(element_index);
        if (element_it != object_it->second.array_elements.end()) {
          loaded_value = element_it->second;
        } else {
          loaded_value = MaterializePlaceholderObjectValue(
              element_descriptor, &next_object_id, &objects);
          object_it->second.array_elements[element_index] = loaded_value;
          placeholder_materialized = true;
        }
        if (loaded_value.kind != DexRegisterValue::Kind::kObject) {
          probe.execution_state = "aget_object_loaded_value_invalid";
          probe.exact_blocker = "dex_aget_object_loaded_value_invalid";
          probe.errors.push_back("dex_aget_object_loaded_value_invalid");
          return probe;
        }
        registers[destination] = loaded_value;
        mark_object_field_operation(
            "aget-object",
            (array_length_materialized || placeholder_materialized)
                ? "object-placeholder"
                : "executed",
            array_length_materialized
                ? "placeholder_array_length_materialized_for_minimal_checkpoint"
                : (placeholder_materialized
                       ? "placeholder_array_element_materialized_for_minimal_checkpoint"
                       : "linuxoid_placeholder_array_element_state_for_minimal_checkpoint"),
            loaded_value.class_descriptor, "", "", "");
        if (array_length_materialized) {
          AppendUnique(
              &probe.diagnostics,
              "Self-Healing Android Device DEX probe materialized a deterministic placeholder object-array length");
        }
        if (placeholder_materialized) {
          AppendUnique(
              &probe.diagnostics,
              "Self-Healing Android Device DEX probe materialized a placeholder object array element");
        }
        pc += 2u;
        continue;
      }
      case 0x31: {  // cmp-long
        if (pc + 1u >= insns_size) {
          probe.execution_state = "cmp_long_truncated";
          probe.exact_blocker = "dex_cmp_long_truncated";
          probe.errors.push_back("dex_cmp_long_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t registers_word =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint32_t source_left =
            static_cast<std::uint32_t>(registers_word & 0x00ffu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((registers_word >> 8u) & 0x00ffu);
        if (destination >= registers.size() ||
            source_left + 1u >= registers.size() ||
            source_right + 1u >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_cmp_long_register_out_of_range";
          probe.errors.push_back("dex_cmp_long_register_out_of_range");
          return probe;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_left + 1u].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right + 1u].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "cmp_long_source_invalid";
          probe.exact_blocker = "dex_cmp_long_source_invalid";
          probe.errors.push_back("dex_cmp_long_source_invalid");
          return probe;
        }
        const std::uint64_t left_low =
            static_cast<std::uint32_t>(registers[source_left].int_value);
        const std::uint64_t left_high =
            static_cast<std::uint32_t>(registers[source_left + 1u].int_value);
        const std::uint64_t right_low =
            static_cast<std::uint32_t>(registers[source_right].int_value);
        const std::uint64_t right_high =
            static_cast<std::uint32_t>(registers[source_right + 1u].int_value);
        const std::int64_t left_value = static_cast<std::int64_t>(
            (left_high << 32u) | left_low);
        const std::int64_t right_value = static_cast<std::int64_t>(
            (right_high << 32u) | right_low);
        const std::int32_t comparison =
            left_value < right_value ? -1 : (left_value > right_value ? 1 : 0);
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = comparison};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder cmp-long");
        ++pc;
        continue;
      }
      case 0x81: {  // int-to-long
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination + 1u >= registers.size() || source >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_int_to_long_register_out_of_range";
          probe.errors.push_back("dex_int_to_long_register_out_of_range");
          return probe;
        }
        if (registers[source].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "int_to_long_source_invalid";
          probe.exact_blocker = "dex_int_to_long_source_invalid";
          probe.errors.push_back("dex_int_to_long_source_invalid");
          return probe;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = registers[source].int_value};
        registers[destination + 1u] = {.kind = DexRegisterValue::Kind::kInt,
                                       .int_value = 0};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder int-to-long");
        ++pc;
        continue;
      }
      case 0xa1: {  // or-long
        if (pc + 1u >= insns_size) {
          probe.execution_state = "or_long_truncated";
          probe.exact_blocker = "dex_or_long_truncated";
          probe.errors.push_back("dex_or_long_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t registers_word =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint32_t source_left =
            static_cast<std::uint32_t>(registers_word & 0x00ffu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((registers_word >> 8u) & 0x00ffu);
        if (destination + 1u >= registers.size() ||
            source_left + 1u >= registers.size() ||
            source_right + 1u >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_or_long_register_out_of_range";
          probe.errors.push_back("dex_or_long_register_out_of_range");
          return probe;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_left + 1u].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right + 1u].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "or_long_source_invalid";
          probe.exact_blocker = "dex_or_long_source_invalid";
          probe.errors.push_back("dex_or_long_source_invalid");
          return probe;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value =
                                      registers[source_left].int_value |
                                      registers[source_right].int_value};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = registers[source_left + 1u].int_value |
                         registers[source_right + 1u].int_value};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder or-long");
        pc += 2u;
        continue;
      }
      case 0x91: {  // sub-int
        if (pc + 1u >= insns_size) {
          probe.execution_state = "sub_int_truncated";
          probe.exact_blocker = "dex_sub_int_truncated";
          probe.errors.push_back("dex_sub_int_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t registers_word =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint32_t source_left =
            static_cast<std::uint32_t>(registers_word & 0x00ffu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((registers_word >> 8u) & 0x00ffu);
        if (destination >= registers.size() || source_left >= registers.size() ||
            source_right >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_sub_int_register_out_of_range";
          probe.errors.push_back("dex_sub_int_register_out_of_range");
          return probe;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "sub_int_source_invalid";
          probe.exact_blocker = "dex_sub_int_source_invalid";
          probe.errors.push_back("dex_sub_int_source_invalid");
          return probe;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = registers[source_left].int_value -
                         registers[source_right].int_value};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder sub-int");
        pc += 2u;
        continue;
      }
      case 0x90: {  // add-int
        if (pc + 1u >= insns_size) {
          probe.execution_state = "add_int_truncated";
          probe.exact_blocker = "dex_add_int_truncated";
          probe.errors.push_back("dex_add_int_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t registers_word =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint32_t source_left =
            static_cast<std::uint32_t>(registers_word & 0x00ffu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((registers_word >> 8u) & 0x00ffu);
        if (destination >= registers.size() || source_left >= registers.size() ||
            source_right >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_add_int_register_out_of_range";
          probe.errors.push_back("dex_add_int_register_out_of_range");
          return probe;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "add_int_source_invalid";
          probe.exact_blocker = "dex_add_int_source_invalid";
          probe.errors.push_back("dex_add_int_source_invalid");
          return probe;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(
                registers[source_left].int_value +
                registers[source_right].int_value)};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder add-int");
        pc += 2u;
        continue;
      }
      case 0xd1: {  // rsub-int/lit16
        if (pc + 1u >= insns_size) {
          probe.execution_state = "rsub_int_lit16_truncated";
          probe.exact_blocker = "dex_rsub_int_lit16_truncated";
          probe.errors.push_back("dex_rsub_int_lit16_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::int16_t literal = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        if (destination >= registers.size() || source >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker =
              "dex_rsub_int_lit16_register_out_of_range";
          probe.errors.push_back(
              "dex_rsub_int_lit16_register_out_of_range");
          return probe;
        }
        if (registers[source].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "rsub_int_lit16_source_invalid";
          probe.exact_blocker = "dex_rsub_int_lit16_source_invalid";
          probe.errors.push_back("dex_rsub_int_lit16_source_invalid");
          return probe;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(literal) -
                         registers[source].int_value};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder rsub-int/lit16");
        pc += 2u;
        continue;
      }
      case 0x9c: {  // sub-long
        if (pc + 1u >= insns_size) {
          probe.execution_state = "sub_long_truncated";
          probe.exact_blocker = "dex_sub_long_truncated";
          probe.errors.push_back("dex_sub_long_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t registers_word =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint32_t source_left =
            static_cast<std::uint32_t>(registers_word & 0x00ffu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((registers_word >> 8u) & 0x00ffu);
        if (destination + 1u >= registers.size() ||
            source_left + 1u >= registers.size() ||
            source_right + 1u >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_sub_long_register_out_of_range";
          probe.errors.push_back("dex_sub_long_register_out_of_range");
          return probe;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_left + 1u].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right + 1u].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "sub_long_source_invalid";
          probe.exact_blocker = "dex_sub_long_source_invalid";
          probe.errors.push_back("dex_sub_long_source_invalid");
          return probe;
        }
        const std::uint64_t left_low =
            static_cast<std::uint32_t>(registers[source_left].int_value);
        const std::uint64_t left_high =
            static_cast<std::uint32_t>(registers[source_left + 1u].int_value);
        const std::uint64_t right_low =
            static_cast<std::uint32_t>(registers[source_right].int_value);
        const std::uint64_t right_high =
            static_cast<std::uint32_t>(registers[source_right + 1u].int_value);
        const std::int64_t left_value =
            static_cast<std::int64_t>((left_high << 32u) | left_low);
        const std::int64_t right_value =
            static_cast<std::int64_t>((right_high << 32u) | right_low);
        const std::uint64_t raw_result = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(left_value - right_value));
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(raw_result & 0xffffffffu)};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>((raw_result >> 32u) &
                                                   0xffffffffu)};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder sub-long");
        pc += 2u;
        continue;
      }
      case 0xb0: {  // add-int/2addr
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination >= registers.size() || source >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_add_int_2addr_register_out_of_range";
          probe.errors.push_back("dex_add_int_2addr_register_out_of_range");
          return probe;
        }
        if (registers[destination].kind != DexRegisterValue::Kind::kInt ||
            registers[source].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "add_int_2addr_source_invalid";
          probe.exact_blocker = "dex_add_int_2addr_source_invalid";
          probe.errors.push_back("dex_add_int_2addr_source_invalid");
          return probe;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(
                registers[destination].int_value + registers[source].int_value)};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder add-int/2addr");
        ++pc;
        continue;
      }
      case 0xb1: {  // sub-int/2addr
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination >= registers.size() || source >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker =
              "dex_sub_int_2addr_register_out_of_range";
          probe.errors.push_back("dex_sub_int_2addr_register_out_of_range");
          return probe;
        }
        if (registers[destination].kind != DexRegisterValue::Kind::kInt ||
            registers[source].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "sub_int_2addr_source_invalid";
          probe.exact_blocker = "dex_sub_int_2addr_source_invalid";
          probe.errors.push_back("dex_sub_int_2addr_source_invalid");
          return probe;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(
                registers[destination].int_value - registers[source].int_value)};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder sub-int/2addr");
        ++pc;
        continue;
      }
      case 0xbb: {  // add-long/2addr
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination + 1u >= registers.size() ||
            source + 1u >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_add_long_2addr_register_out_of_range";
          probe.errors.push_back("dex_add_long_2addr_register_out_of_range");
          return probe;
        }
        if (registers[destination].kind != DexRegisterValue::Kind::kInt ||
            registers[destination + 1u].kind != DexRegisterValue::Kind::kInt ||
            registers[source].kind != DexRegisterValue::Kind::kInt ||
            registers[source + 1u].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "add_long_2addr_source_invalid";
          probe.exact_blocker = "dex_add_long_2addr_source_invalid";
          probe.errors.push_back("dex_add_long_2addr_source_invalid");
          return probe;
        }
        const std::uint64_t destination_low =
            static_cast<std::uint32_t>(registers[destination].int_value);
        const std::uint64_t destination_high =
            static_cast<std::uint32_t>(registers[destination + 1u].int_value);
        const std::uint64_t source_low =
            static_cast<std::uint32_t>(registers[source].int_value);
        const std::uint64_t source_high =
            static_cast<std::uint32_t>(registers[source + 1u].int_value);
        const std::int64_t destination_value = static_cast<std::int64_t>(
            (destination_high << 32u) | destination_low);
        const std::int64_t source_value =
            static_cast<std::int64_t>((source_high << 32u) | source_low);
        const std::uint64_t raw_result = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(destination_value + source_value));
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(raw_result & 0xffffffffu)};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>((raw_result >> 32u) &
                                                   0xffffffffu)};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder add-long/2addr");
        ++pc;
        continue;
      }
      case 0xbc: {  // sub-long/2addr
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination + 1u >= registers.size() ||
            source + 1u >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_sub_long_2addr_register_out_of_range";
          probe.errors.push_back("dex_sub_long_2addr_register_out_of_range");
          return probe;
        }
        if (registers[destination].kind != DexRegisterValue::Kind::kInt ||
            registers[destination + 1u].kind != DexRegisterValue::Kind::kInt ||
            registers[source].kind != DexRegisterValue::Kind::kInt ||
            registers[source + 1u].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "sub_long_2addr_source_invalid";
          probe.exact_blocker = "dex_sub_long_2addr_source_invalid";
          probe.errors.push_back("dex_sub_long_2addr_source_invalid");
          return probe;
        }
        const std::uint64_t destination_low =
            static_cast<std::uint32_t>(registers[destination].int_value);
        const std::uint64_t destination_high =
            static_cast<std::uint32_t>(registers[destination + 1u].int_value);
        const std::uint64_t source_low =
            static_cast<std::uint32_t>(registers[source].int_value);
        const std::uint64_t source_high =
            static_cast<std::uint32_t>(registers[source + 1u].int_value);
        const std::int64_t destination_value = static_cast<std::int64_t>(
            (destination_high << 32u) | destination_low);
        const std::int64_t source_value =
            static_cast<std::int64_t>((source_high << 32u) | source_low);
        const std::uint64_t raw_result = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(destination_value - source_value));
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(raw_result & 0xffffffffu)};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>((raw_result >> 32u) &
                                                   0xffffffffu)};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder sub-long/2addr");
        ++pc;
        continue;
      }
      case 0xc0: {  // and-long/2addr
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (destination + 1u >= registers.size() ||
            source + 1u >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_and_long_2addr_register_out_of_range";
          probe.errors.push_back("dex_and_long_2addr_register_out_of_range");
          return probe;
        }
        if (registers[destination].kind != DexRegisterValue::Kind::kInt ||
            registers[destination + 1u].kind != DexRegisterValue::Kind::kInt ||
            registers[source].kind != DexRegisterValue::Kind::kInt ||
            registers[source + 1u].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "and_long_2addr_source_invalid";
          probe.exact_blocker = "dex_and_long_2addr_source_invalid";
          probe.errors.push_back("dex_and_long_2addr_source_invalid");
          return probe;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(registers[destination].int_value) &
                static_cast<std::uint32_t>(registers[source].int_value))};
        registers[destination + 1u] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(registers[destination + 1u].int_value) &
                static_cast<std::uint32_t>(registers[source + 1u].int_value))};
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder and-long/2addr");
        ++pc;
        continue;
      }
      case 0x39: {  // if-nez
        if (pc + 1u >= insns_size) {
          probe.execution_state = "branch_truncated";
          probe.exact_blocker = "dex_if_nez_truncated";
          probe.errors.push_back("dex_if_nez_truncated");
          return probe;
        }
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (source >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_if_nez_register_out_of_range";
          probe.errors.push_back("dex_if_nez_register_out_of_range");
          return probe;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_non_zero =
            registers[source].kind == DexRegisterValue::Kind::kObject ||
            (registers[source].kind == DexRegisterValue::Kind::kInt &&
             registers[source].int_value != 0);
        if (is_non_zero) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            probe.execution_state = "branch_out_of_range";
            probe.exact_blocker = "dex_if_nez_branch_out_of_range";
            probe.errors.push_back("dex_if_nez_branch_out_of_range");
            return probe;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x3a: {  // if-ltz
        if (pc + 1u >= insns_size) {
          probe.execution_state = "branch_truncated";
          probe.exact_blocker = "dex_if_ltz_truncated";
          probe.errors.push_back("dex_if_ltz_truncated");
          return probe;
        }
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (source >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_if_ltz_register_out_of_range";
          probe.errors.push_back("dex_if_ltz_register_out_of_range");
          return probe;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_less_than_zero =
            registers[source].kind == DexRegisterValue::Kind::kInt &&
            registers[source].int_value < 0;
        if (is_less_than_zero) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            probe.execution_state = "branch_out_of_range";
            probe.exact_blocker = "dex_if_ltz_branch_out_of_range";
            probe.errors.push_back("dex_if_ltz_branch_out_of_range");
            return probe;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x3c: {  // if-gtz
        if (pc + 1u >= insns_size) {
          probe.execution_state = "branch_truncated";
          probe.exact_blocker = "dex_if_gtz_truncated";
          probe.errors.push_back("dex_if_gtz_truncated");
          return probe;
        }
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (source >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_if_gtz_register_out_of_range";
          probe.errors.push_back("dex_if_gtz_register_out_of_range");
          return probe;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_greater_than_zero =
            registers[source].kind == DexRegisterValue::Kind::kInt &&
            registers[source].int_value > 0;
        if (is_greater_than_zero) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            probe.execution_state = "branch_out_of_range";
            probe.exact_blocker = "dex_if_gtz_branch_out_of_range";
            probe.errors.push_back("dex_if_gtz_branch_out_of_range");
            return probe;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x34: {  // if-lt
        if (pc + 1u >= insns_size) {
          probe.execution_state = "branch_truncated";
          probe.exact_blocker = "dex_if_lt_truncated";
          probe.errors.push_back("dex_if_lt_truncated");
          return probe;
        }
        const std::uint32_t source_left =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (source_left >= registers.size() || source_right >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_if_lt_register_out_of_range";
          probe.errors.push_back("dex_if_lt_register_out_of_range");
          return probe;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "if_lt_source_invalid";
          probe.exact_blocker = "dex_if_lt_source_invalid";
          probe.errors.push_back("dex_if_lt_source_invalid");
          return probe;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_less_than =
            registers[source_left].int_value <
            registers[source_right].int_value;
        if (is_less_than) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            probe.execution_state = "branch_out_of_range";
            probe.exact_blocker = "dex_if_lt_branch_out_of_range";
            probe.errors.push_back("dex_if_lt_branch_out_of_range");
            return probe;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x33: {  // if-ne
        if (pc + 1u >= insns_size) {
          probe.execution_state = "branch_truncated";
          probe.exact_blocker = "dex_if_ne_truncated";
          probe.errors.push_back("dex_if_ne_truncated");
          return probe;
        }
        const std::uint32_t source_left =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (source_left >= registers.size() || source_right >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_if_ne_register_out_of_range";
          probe.errors.push_back("dex_if_ne_register_out_of_range");
          return probe;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        bool is_not_equal = false;
        if (registers[source_left].kind == DexRegisterValue::Kind::kInt &&
            registers[source_right].kind == DexRegisterValue::Kind::kInt) {
          is_not_equal = registers[source_left].int_value !=
                         registers[source_right].int_value;
        } else if (registers[source_left].kind ==
                       DexRegisterValue::Kind::kObject &&
                   registers[source_right].kind ==
                       DexRegisterValue::Kind::kObject) {
          is_not_equal =
              registers[source_left].object_id !=
              registers[source_right].object_id;
        } else {
          probe.execution_state = "if_ne_source_invalid";
          probe.exact_blocker = "dex_if_ne_source_invalid";
          probe.errors.push_back("dex_if_ne_source_invalid");
          return probe;
        }
        if (is_not_equal) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            probe.execution_state = "branch_out_of_range";
            probe.exact_blocker = "dex_if_ne_branch_out_of_range";
            probe.errors.push_back("dex_if_ne_branch_out_of_range");
            return probe;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x35: {  // if-ge
        if (pc + 1u >= insns_size) {
          probe.execution_state = "branch_truncated";
          probe.exact_blocker = "dex_if_ge_truncated";
          probe.errors.push_back("dex_if_ge_truncated");
          return probe;
        }
        const std::uint32_t source_left =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (source_left >= registers.size() || source_right >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_if_ge_register_out_of_range";
          probe.errors.push_back("dex_if_ge_register_out_of_range");
          return probe;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "if_ge_source_invalid";
          probe.exact_blocker = "dex_if_ge_source_invalid";
          probe.errors.push_back("dex_if_ge_source_invalid");
          return probe;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_greater_or_equal =
            registers[source_left].int_value >=
            registers[source_right].int_value;
        if (is_greater_or_equal) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            probe.execution_state = "branch_out_of_range";
            probe.exact_blocker = "dex_if_ge_branch_out_of_range";
            probe.errors.push_back("dex_if_ge_branch_out_of_range");
            return probe;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x36: {  // if-gt
        if (pc + 1u >= insns_size) {
          probe.execution_state = "branch_truncated";
          probe.exact_blocker = "dex_if_gt_truncated";
          probe.errors.push_back("dex_if_gt_truncated");
          return probe;
        }
        const std::uint32_t source_left =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t source_right =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        if (source_left >= registers.size() || source_right >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_if_gt_register_out_of_range";
          probe.errors.push_back("dex_if_gt_register_out_of_range");
          return probe;
        }
        if (registers[source_left].kind != DexRegisterValue::Kind::kInt ||
            registers[source_right].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "if_gt_source_invalid";
          probe.exact_blocker = "dex_if_gt_source_invalid";
          probe.errors.push_back("dex_if_gt_source_invalid");
          return probe;
        }
        const std::int16_t branch_offset = static_cast<std::int16_t>(
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u));
        const bool is_greater_than =
            registers[source_left].int_value >
            registers[source_right].int_value;
        if (is_greater_than) {
          const std::int64_t next_pc =
              static_cast<std::int64_t>(pc) + branch_offset;
          if (next_pc < 0 ||
              next_pc >= static_cast<std::int64_t>(insns_size)) {
            probe.execution_state = "branch_out_of_range";
            probe.exact_blocker = "dex_if_gt_branch_out_of_range";
            probe.errors.push_back("dex_if_gt_branch_out_of_range");
            return probe;
          }
          pc = static_cast<std::uint32_t>(next_pc);
        } else {
          pc += 2u;
        }
        continue;
      }
      case 0x60:
      case 0x62:
      case 0x63: {  // sget / sget-object / sget-boolean
        if (pc + 1u >= insns_size) {
          probe.execution_state = "static_field_load_truncated";
          probe.exact_blocker =
              opcode == 0x60 ? "dex_sget_truncated"
              : (opcode == 0x62 ? "dex_sget_object_truncated"
                                : "dex_sget_boolean_truncated");
          probe.errors.push_back(probe.exact_blocker);
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker =
              opcode == 0x60 ? "dex_sget_register_out_of_range"
              : (opcode == 0x62 ? "dex_sget_object_register_out_of_range"
                                : "dex_sget_boolean_register_out_of_range");
          probe.errors.push_back(probe.exact_blocker);
          return probe;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &probe.errors)) {
          probe.execution_state = "sget_unresolved";
          probe.exact_blocker =
              probe.errors.empty()
                  ? (opcode == 0x60
                         ? "dex_sget_field_resolution_failed"
                         : (opcode == 0x62
                                ? "dex_sget_object_field_resolution_failed"
                                : "dex_sget_boolean_field_resolution_failed"))
                  : probe.errors.front();
          return probe;
        }
        registers[destination] = load_or_materialize_static_field(
            field_class_descriptor, field_name, field_signature);
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder static field access");
        pc += 2u;
        continue;
      }
      case 0x0a:
      case 0x0b:
      case 0x0c: {  // move-result / move-result-wide / move-result-object
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (opcode == 0x0b) {
          if (destination + 1u >= registers.size()) {
            probe.execution_state = "register_out_of_range";
            probe.exact_blocker = "dex_move_result_wide_register_out_of_range";
            probe.errors.push_back("dex_move_result_wide_register_out_of_range");
            return probe;
          }
        } else if (destination >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker =
              opcode == 0x0c ? "dex_move_result_object_register_out_of_range"
                             : "dex_move_result_register_out_of_range";
          probe.errors.push_back(probe.exact_blocker);
          return probe;
        }
        if (!pending_result_valid || (opcode == 0x0b && !pending_result_is_wide)) {
          probe.execution_state =
              opcode == 0x0b ? "move_result_wide_without_pending_value"
                             : "move_result_without_pending_value";
          probe.exact_blocker =
              opcode == 0x0b
                  ? "dex_move_result_wide_without_pending_value"
                  : (opcode == 0x0c
                         ? "dex_move_result_object_without_pending_value"
                         : "dex_move_result_without_pending_value");
          probe.errors.push_back(probe.exact_blocker);
          return probe;
        }
        registers[destination] = pending_result;
        if (opcode == 0x0b) {
          registers[destination + 1u] = pending_result_high;
        }
        pending_result_valid = false;
        pending_result_is_wide = false;
        ++pc;
        continue;
      }
      case 0x22: {  // new-instance
        if (pc + 1u >= insns_size) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "new_instance_truncated";
          probe.execution_state = "new_instance_truncated";
          probe.exact_blocker = "dex_new_instance_truncated";
          probe.errors.push_back("dex_new_instance_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t type_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "new_instance_register_out_of_range";
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_new_instance_register_out_of_range";
          probe.errors.push_back("dex_new_instance_register_out_of_range");
          return probe;
        }
        if (type_index >= tables.type_descriptor_string_indices.size()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "new_instance_type_index_invalid";
          probe.execution_state = "new_instance_type_invalid";
          probe.exact_blocker = "dex_new_instance_type_index_invalid";
          probe.errors.push_back("dex_new_instance_type_index_invalid");
          return probe;
        }
        const std::uint32_t type_string_index =
            tables.type_descriptor_string_indices[type_index];
        if (type_string_index >= tables.strings.size()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "new_instance_type_string_invalid";
          probe.execution_state = "new_instance_type_invalid";
          probe.exact_blocker = "dex_new_instance_type_string_invalid";
          probe.errors.push_back("dex_new_instance_type_string_invalid");
          return probe;
        }
        const std::string class_descriptor = tables.strings[type_string_index];
        const std::uint32_t object_id = next_object_id++;
        objects[object_id] = {.object_id = object_id,
                              .class_descriptor = class_descriptor,
                              .fields = {}};
        registers[destination] = {.kind = DexRegisterValue::Kind::kObject,
                                  .object_id = object_id,
                                  .class_descriptor = class_descriptor};
        mark_object_field_operation(
            "new-instance", "object-placeholder",
            "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint",
            class_descriptor, "", "", "");
        AppendUnique(&probe.diagnostics,
                     "Self-Healing Android Device DEX probe modeled a placeholder object allocation");
        pc += 2u;
        continue;
      }
      case 0x23: {  // new-array
        if (pc + 1u >= insns_size) {
          probe.execution_state = "new_array_truncated";
          probe.exact_blocker = "dex_new_array_truncated";
          probe.errors.push_back("dex_new_array_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t size_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t type_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size() || size_register >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_new_array_register_out_of_range";
          probe.errors.push_back("dex_new_array_register_out_of_range");
          return probe;
        }
        if (registers[size_register].kind != DexRegisterValue::Kind::kInt) {
          probe.execution_state = "new_array_size_register_invalid";
          probe.exact_blocker = "dex_new_array_size_register_invalid";
          probe.errors.push_back("dex_new_array_size_register_invalid");
          return probe;
        }
        if (type_index >= tables.type_descriptor_string_indices.size()) {
          probe.execution_state = "new_array_type_invalid";
          probe.exact_blocker = "dex_new_array_type_index_invalid";
          probe.errors.push_back("dex_new_array_type_index_invalid");
          return probe;
        }
        const std::uint32_t type_string_index =
            tables.type_descriptor_string_indices[type_index];
        if (type_string_index >= tables.strings.size()) {
          probe.execution_state = "new_array_type_invalid";
          probe.exact_blocker = "dex_new_array_type_string_invalid";
          probe.errors.push_back("dex_new_array_type_string_invalid");
          return probe;
        }
        const std::string class_descriptor = tables.strings[type_string_index];
        registers[destination] = MaterializePlaceholderObjectValue(
            class_descriptor, &next_object_id, &objects,
            registers[size_register].int_value);
        probe.object_register_field_operation = "new-array";
        probe.object_register_field_state = "object-placeholder";
        probe.object_register_field_reason =
            "linuxoid_placeholder_array_state_for_minimal_checkpoint";
        probe.object_class_descriptor = class_descriptor;
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe modeled a placeholder array allocation");
        pc += 2u;
        continue;
      }
      case 0x67:
      case 0x69:
      case 0x6a: {  // sput / sput-object / sput-boolean
        if (pc + 1u >= insns_size) {
          probe.execution_state = "static_field_store_truncated";
          probe.exact_blocker =
              opcode == 0x67 ? "dex_sput_truncated"
              : (opcode == 0x69 ? "dex_sput_object_truncated"
                                : "dex_sput_boolean_truncated");
          probe.errors.push_back(probe.exact_blocker);
          return probe;
        }
        const std::uint32_t source =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (source >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker =
              opcode == 0x67 ? "dex_sput_register_out_of_range"
              : (opcode == 0x69 ? "dex_sput_object_register_out_of_range"
                                : "dex_sput_boolean_register_out_of_range");
          probe.errors.push_back(probe.exact_blocker);
          return probe;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &probe.errors)) {
          probe.execution_state = "field_resolution_failed";
          probe.exact_blocker =
              probe.errors.empty()
                  ? (opcode == 0x67
                         ? "dex_sput_field_resolution_failed"
                         : (opcode == 0x69
                                ? "dex_sput_object_field_resolution_failed"
                                : "dex_sput_boolean_field_resolution_failed"))
                  : probe.errors.front();
          return probe;
        }
        store_static_field(field_class_descriptor, field_name, field_signature,
                           registers[source]);
        pc += 2u;
        continue;
      }
      case 0x52: {  // iget
        if (pc + 1u >= insns_size) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iget_truncated";
          probe.execution_state = "iget_truncated";
          probe.exact_blocker = "dex_iget_truncated";
          probe.errors.push_back("dex_iget_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size() ||
            object_register >= registers.size()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iget_register_out_of_range";
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_iget_register_out_of_range";
          probe.errors.push_back("dex_iget_register_out_of_range");
          return probe;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iget_object_placeholder_missing";
          probe.execution_state = "object_register_missing";
          probe.exact_blocker = "dex_iget_object_placeholder_missing";
          probe.errors.push_back("dex_iget_object_placeholder_missing");
          return probe;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &probe.errors)) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iget_field_resolution_failed";
          probe.execution_state = "iget_unresolved";
          probe.exact_blocker =
              probe.errors.empty() ? "dex_iget_field_resolution_failed"
                                   : probe.errors.front();
          return probe;
        }
        auto object_it = objects.find(registers[object_register].object_id);
        if (object_it == objects.end()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iget_object_identity_missing";
          probe.execution_state = "object_identity_missing";
          probe.exact_blocker = "dex_iget_object_identity_missing";
          probe.errors.push_back("dex_iget_object_identity_missing");
          return probe;
        }
        const std::string field_key =
            field_class_descriptor + "->" + field_name + ":" + field_signature;
        DexRegisterValue loaded_value = {.kind = DexRegisterValue::Kind::kInt,
                                         .int_value = 0};
        const auto field_it = object_it->second.fields.find(field_key);
        if (field_it != object_it->second.fields.end()) {
          loaded_value = field_it->second;
        }
        registers[destination] = loaded_value;
        const std::string operation =
            probe.object_register_field_operation ==
                    "new-instance+invoke-direct+iput-object+iget-object"
                ? "new-instance+invoke-direct+iput-object+iget-object+iget"
                : "new-instance+iput+iget";
        mark_object_field_operation(
            operation, "object-placeholder",
            "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint",
            object_it->second.class_descriptor, field_class_descriptor,
            field_name, field_signature);
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder object field access");
        pc += 2u;
        continue;
      }
      case 0x53: {  // iget-wide
        if (pc + 1u >= insns_size) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iget_wide_truncated";
          probe.execution_state = "iget_wide_truncated";
          probe.exact_blocker = "dex_iget_wide_truncated";
          probe.errors.push_back("dex_iget_wide_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination + 1u >= registers.size() ||
            object_register >= registers.size()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iget_wide_register_out_of_range";
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_iget_wide_register_out_of_range";
          probe.errors.push_back("dex_iget_wide_register_out_of_range");
          return probe;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iget_wide_object_placeholder_missing";
          probe.execution_state = "object_register_missing";
          probe.exact_blocker = "dex_iget_wide_object_placeholder_missing";
          probe.errors.push_back("dex_iget_wide_object_placeholder_missing");
          return probe;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &probe.errors)) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iget_wide_field_resolution_failed";
          probe.execution_state = "iget_wide_unresolved";
          probe.exact_blocker =
              probe.errors.empty()
                  ? "dex_iget_wide_field_resolution_failed"
                  : probe.errors.front();
          return probe;
        }
        auto object_it = objects.find(registers[object_register].object_id);
        if (object_it == objects.end()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iget_wide_object_identity_missing";
          probe.execution_state = "object_identity_missing";
          probe.exact_blocker = "dex_iget_wide_object_identity_missing";
          probe.errors.push_back("dex_iget_wide_object_identity_missing");
          return probe;
        }
        const std::string field_key =
            field_class_descriptor + "->" + field_name + ":" + field_signature;
        DexRegisterValue low_value = {.kind = DexRegisterValue::Kind::kInt,
                                      .int_value = 0};
        DexRegisterValue high_value = {.kind = DexRegisterValue::Kind::kInt,
                                       .int_value = 0};
        const auto field_it = object_it->second.fields.find(field_key);
        if (field_it != object_it->second.fields.end()) {
          low_value = field_it->second;
        }
        const auto high_it = object_it->second.fields.find(field_key + "#high");
        if (high_it != object_it->second.fields.end()) {
          high_value = high_it->second;
        }
        registers[destination] = {.kind = DexRegisterValue::Kind::kInt,
                                  .int_value = low_value.kind ==
                                                       DexRegisterValue::Kind::kInt
                                                   ? low_value.int_value
                                                   : 0};
        registers[destination + 1u] = {.kind = DexRegisterValue::Kind::kInt,
                                       .int_value = high_value.kind ==
                                                            DexRegisterValue::Kind::kInt
                                                        ? high_value.int_value
                                                        : 0};
        mark_object_field_operation(
            "new-instance+iget-wide", "object-placeholder",
            "linuxoid_placeholder_object_and_wide_field_state_for_minimal_checkpoint",
            object_it->second.class_descriptor, field_class_descriptor,
            field_name, field_signature);
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder wide object field access");
        pc += 2u;
        continue;
      }
      case 0x54: {  // iget-object
        if (pc + 1u >= insns_size) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iget_object_truncated";
          probe.execution_state = "iget_object_truncated";
          probe.exact_blocker = "dex_iget_object_truncated";
          probe.errors.push_back("dex_iget_object_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size() ||
            object_register >= registers.size()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iget_object_register_out_of_range";
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_iget_object_register_out_of_range";
          probe.errors.push_back("dex_iget_object_register_out_of_range");
          return probe;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iget_object_placeholder_missing";
          probe.execution_state = "object_register_missing";
          probe.exact_blocker = "dex_iget_object_placeholder_missing";
          probe.errors.push_back("dex_iget_object_placeholder_missing");
          return probe;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &probe.errors)) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iget_object_field_resolution_failed";
          probe.execution_state = "iget_object_unresolved";
          probe.exact_blocker =
              probe.errors.empty()
                  ? "dex_iget_object_field_resolution_failed"
                  : probe.errors.front();
          return probe;
        }
        auto object_it = objects.find(registers[object_register].object_id);
        if (object_it == objects.end()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iget_object_identity_missing";
          probe.execution_state = "object_identity_missing";
          probe.exact_blocker = "dex_iget_object_identity_missing";
          probe.errors.push_back("dex_iget_object_identity_missing");
          return probe;
        }
        const std::string field_key =
            field_class_descriptor + "->" + field_name + ":" + field_signature;
        DexRegisterValue loaded_value = {.kind = DexRegisterValue::Kind::kUnknown};
        const auto field_it = object_it->second.fields.find(field_key);
        if (field_it != object_it->second.fields.end()) {
          loaded_value = field_it->second;
        }
        registers[destination] = loaded_value;
        mark_object_field_operation(
            "new-instance+invoke-direct+iput-object+iget-object",
            "object-placeholder",
            "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint",
            object_it->second.class_descriptor, field_class_descriptor,
            field_name, field_signature);
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder object reference field access");
        pc += 2u;
        continue;
      }
      case 0x55: {  // iget-boolean
        if (pc + 1u >= insns_size) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iget_boolean_truncated";
          probe.execution_state = "iget_boolean_truncated";
          probe.exact_blocker = "dex_iget_boolean_truncated";
          probe.errors.push_back("dex_iget_boolean_truncated");
          return probe;
        }
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (destination >= registers.size() ||
            object_register >= registers.size()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iget_boolean_register_out_of_range";
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_iget_boolean_register_out_of_range";
          probe.errors.push_back("dex_iget_boolean_register_out_of_range");
          return probe;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iget_boolean_object_placeholder_missing";
          probe.execution_state = "object_register_missing";
          probe.exact_blocker = "dex_iget_boolean_object_placeholder_missing";
          probe.errors.push_back("dex_iget_boolean_object_placeholder_missing");
          return probe;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &probe.errors)) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iget_boolean_field_resolution_failed";
          probe.execution_state = "iget_boolean_unresolved";
          probe.exact_blocker =
              probe.errors.empty()
                  ? "dex_iget_boolean_field_resolution_failed"
                  : probe.errors.front();
          return probe;
        }
        auto object_it = objects.find(registers[object_register].object_id);
        if (object_it == objects.end()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iget_boolean_object_identity_missing";
          probe.execution_state = "object_identity_missing";
          probe.exact_blocker = "dex_iget_boolean_object_identity_missing";
          probe.errors.push_back("dex_iget_boolean_object_identity_missing");
          return probe;
        }
        const std::string field_key =
            field_class_descriptor + "->" + field_name + ":" + field_signature;
        DexRegisterValue loaded_value = {.kind = DexRegisterValue::Kind::kInt,
                                         .int_value = 0};
        const auto field_it = object_it->second.fields.find(field_key);
        if (field_it != object_it->second.fields.end()) {
          loaded_value = field_it->second;
        }
        registers[destination] = {
            .kind = DexRegisterValue::Kind::kInt,
            .int_value =
                (loaded_value.kind == DexRegisterValue::Kind::kObject ||
                 (loaded_value.kind == DexRegisterValue::Kind::kInt &&
                  loaded_value.int_value != 0))
                    ? 1
                    : 0};
        mark_object_field_operation(
            "new-instance+iget-boolean", "object-placeholder",
            "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint",
            object_it->second.class_descriptor, field_class_descriptor,
            field_name, field_signature);
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed placeholder boolean field access");
        pc += 2u;
        continue;
      }
      case 0x5a: {  // iput-wide
        if (pc + 1u >= insns_size) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iput_wide_truncated";
          probe.execution_state = "iput_wide_truncated";
          probe.exact_blocker = "dex_iput_wide_truncated";
          probe.errors.push_back("dex_iput_wide_truncated");
          return probe;
        }
        const std::uint32_t value_register =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (value_register + 1u >= registers.size() ||
            object_register >= registers.size()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iput_wide_register_out_of_range";
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_iput_wide_register_out_of_range";
          probe.errors.push_back("dex_iput_wide_register_out_of_range");
          return probe;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iput_wide_object_placeholder_missing";
          probe.execution_state = "object_register_missing";
          probe.exact_blocker = "dex_iput_wide_object_placeholder_missing";
          probe.errors.push_back("dex_iput_wide_object_placeholder_missing");
          return probe;
        }
        if (registers[value_register].kind != DexRegisterValue::Kind::kInt ||
            registers[value_register + 1u].kind != DexRegisterValue::Kind::kInt) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iput_wide_source_invalid";
          probe.execution_state = "iput_wide_source_invalid";
          probe.exact_blocker = "dex_iput_wide_source_invalid";
          probe.errors.push_back("dex_iput_wide_source_invalid");
          return probe;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &probe.errors)) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iput_wide_field_resolution_failed";
          probe.execution_state = "iput_wide_unresolved";
          probe.exact_blocker =
              probe.errors.empty()
                  ? "dex_iput_wide_field_resolution_failed"
                  : probe.errors.front();
          return probe;
        }
        auto object_it = objects.find(registers[object_register].object_id);
        if (object_it == objects.end()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iput_wide_object_identity_missing";
          probe.execution_state = "object_identity_missing";
          probe.exact_blocker = "dex_iput_wide_object_identity_missing";
          probe.errors.push_back("dex_iput_wide_object_identity_missing");
          return probe;
        }
        const std::string field_key =
            field_class_descriptor + "->" + field_name + ":" + field_signature;
        object_it->second.fields[field_key] = registers[value_register];
        object_it->second.fields[field_key + "#high"] =
            registers[value_register + 1u];
        mark_object_field_operation(
            "new-instance+iput-wide", "object-placeholder",
            "linuxoid_placeholder_object_and_wide_field_state_for_minimal_checkpoint",
            object_it->second.class_descriptor, field_class_descriptor,
            field_name, field_signature);
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe stored a placeholder wide value into an instance field");
        pc += 2u;
        continue;
      }
      case 0x59: {  // iput
        if (pc + 1u >= insns_size) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iput_truncated";
          probe.execution_state = "iput_truncated";
          probe.exact_blocker = "dex_iput_truncated";
          probe.errors.push_back("dex_iput_truncated");
          return probe;
        }
        const std::uint32_t value_register =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (value_register >= registers.size() ||
            object_register >= registers.size()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iput_register_out_of_range";
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_iput_register_out_of_range";
          probe.errors.push_back("dex_iput_register_out_of_range");
          return probe;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iput_object_placeholder_missing";
          probe.execution_state = "object_register_missing";
          probe.exact_blocker = "dex_iput_object_placeholder_missing";
          probe.errors.push_back("dex_iput_object_placeholder_missing");
          return probe;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &probe.errors)) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iput_field_resolution_failed";
          probe.execution_state = "iput_unresolved";
          probe.exact_blocker =
              probe.errors.empty() ? "dex_iput_field_resolution_failed"
                                   : probe.errors.front();
          return probe;
        }
        auto object_it = objects.find(registers[object_register].object_id);
        if (object_it == objects.end()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iput_object_identity_missing";
          probe.execution_state = "object_identity_missing";
          probe.exact_blocker = "dex_iput_object_identity_missing";
          probe.errors.push_back("dex_iput_object_identity_missing");
          return probe;
        }
        const std::string field_key =
            field_class_descriptor + "->" + field_name + ":" + field_signature;
        object_it->second.fields[field_key] = registers[value_register];
        mark_object_field_operation(
            "new-instance+iput", "object-placeholder",
            "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint",
            object_it->second.class_descriptor, field_class_descriptor,
            field_name, field_signature);
        pc += 2u;
        continue;
      }
      case 0x5b: {  // iput-object
        if (pc + 1u >= insns_size) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iput_object_truncated";
          probe.execution_state = "iput_object_truncated";
          probe.exact_blocker = "dex_iput_object_truncated";
          probe.errors.push_back("dex_iput_object_truncated");
          return probe;
        }
        const std::uint32_t value_register =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu);
        const std::uint32_t object_register =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint16_t field_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        if (value_register >= registers.size() ||
            object_register >= registers.size()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iput_object_register_out_of_range";
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_iput_object_register_out_of_range";
          probe.errors.push_back("dex_iput_object_register_out_of_range");
          return probe;
        }
        if (registers[object_register].kind != DexRegisterValue::Kind::kObject) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iput_object_placeholder_missing";
          probe.execution_state = "object_register_missing";
          probe.exact_blocker = "dex_iput_object_placeholder_missing";
          probe.errors.push_back("dex_iput_object_placeholder_missing");
          return probe;
        }
        std::string field_class_descriptor;
        std::string field_name;
        std::string field_signature;
        if (!ResolveFieldReference(tables, field_index, &field_class_descriptor,
                                   &field_name, &field_signature,
                                   &probe.errors)) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason =
              "iput_object_field_resolution_failed";
          probe.execution_state = "iput_object_unresolved";
          probe.exact_blocker =
              probe.errors.empty()
                  ? "dex_iput_object_field_resolution_failed"
                  : probe.errors.front();
          return probe;
        }
        auto object_it = objects.find(registers[object_register].object_id);
        if (object_it == objects.end()) {
          probe.object_register_field_state = "blocked";
          probe.object_register_field_reason = "iput_object_identity_missing";
          probe.execution_state = "object_identity_missing";
          probe.exact_blocker = "dex_iput_object_identity_missing";
          probe.errors.push_back("dex_iput_object_identity_missing");
          return probe;
        }
        const std::string field_key =
            field_class_descriptor + "->" + field_name + ":" + field_signature;
        object_it->second.fields[field_key] = registers[value_register];
        mark_object_field_operation(
            "new-instance+invoke-direct+iput-object", "object-placeholder",
            "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint",
            object_it->second.class_descriptor, field_class_descriptor,
            field_name, field_signature);
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe stored a placeholder object reference into an instance field");
        pc += 2u;
        continue;
      }
      case 0x6f: {  // invoke-super
        if (pc + 2u >= insns_size) {
          probe.framework_boundary_state = "blocked";
          probe.framework_boundary_reason = "invoke_instruction_truncated";
          probe.execution_state = "invoke_truncated";
          probe.exact_blocker = "dex_invoke_instruction_truncated";
          probe.errors.push_back("dex_invoke_instruction_truncated");
          return probe;
        }
        const std::uint16_t method_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint16_t register_word =
            ReadLe16(bytes, insns_off + (pc + 2u) * 2u);
        const std::uint32_t register_count =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint32_t registers_used[5] = {
            static_cast<std::uint32_t>(register_word & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 4u) & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 8u) & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 12u) & 0x0fu),
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu),
        };
        for (std::uint32_t index = 0; index < register_count && index < 5u;
             ++index) {
          if (registers_used[index] >= registers.size()) {
            probe.framework_boundary_state = "blocked";
            probe.framework_boundary_reason = "invoke_register_out_of_range";
            probe.execution_state = "register_out_of_range";
            probe.exact_blocker = "dex_invoke_register_out_of_range";
            probe.errors.push_back("dex_invoke_register_out_of_range");
            return probe;
          }
        }
        if (register_count == 0u ||
            registers[registers_used[0]].kind != DexRegisterValue::Kind::kObject) {
          probe.framework_boundary_state = "blocked";
          probe.framework_boundary_reason = "invoke_receiver_missing";
          probe.execution_state = "framework_boundary_blocked";
          probe.exact_blocker = "dex_invoke_receiver_missing";
          probe.errors.push_back("dex_invoke_receiver_missing");
          return probe;
        }

        DexExecutionCandidate invoked_method;
        if (!ResolveMethodReference(tables, method_index, &invoked_method,
                                    &probe.errors)) {
          probe.framework_boundary_state = "blocked";
          probe.framework_boundary_reason = "invoke_method_resolution_failed";
          probe.execution_state = "invoke_unresolved";
          probe.exact_blocker = probe.errors.empty()
                                    ? "dex_invoke_method_resolution_failed"
                                    : probe.errors.front();
          return probe;
        }
        probe.invoked_method_class_descriptor = invoked_method.class_descriptor;
        probe.invoked_method_name = invoked_method.method_name;
        probe.invoked_method_signature = invoked_method.method_signature;

        if (invoked_method.class_descriptor == "Landroid/app/Activity;" &&
            invoked_method.method_name == "onCreate" &&
            invoked_method.method_signature == "()V") {
          probe.framework_boundary_state = "framework-stubbed";
          probe.framework_boundary_reason =
              "android_activity_oncreate_stubbed_for_minimal_checkpoint";
          probe.diagnostics.push_back(
              "Self-Healing Android Device DEX probe crossed a stubbed Android framework lifecycle boundary");
          pc += 3u;
          continue;
        }
        if (invoked_method.class_descriptor == "Landroid/app/Activity;" &&
            invoked_method.method_name == "onCreate" &&
            invoked_method.method_signature == "(Landroid/os/Bundle;)V") {
          if (register_count < 2u ||
              registers[registers_used[1]].kind !=
                  DexRegisterValue::Kind::kObject) {
            probe.framework_boundary_state = "blocked";
            probe.framework_boundary_reason =
                "invoke_argument_placeholder_missing";
            probe.execution_state = "framework_boundary_blocked";
            probe.exact_blocker = "dex_invoke_argument_placeholder_missing";
            probe.errors.push_back("dex_invoke_argument_placeholder_missing");
            return probe;
          }
          probe.framework_boundary_state = "framework-stubbed";
          probe.framework_boundary_reason =
              "android_activity_oncreate_bundle_stubbed_for_minimal_checkpoint";
          probe.execution_state = "framework_boundary_stubbed";
          probe.exact_blocker =
              "framework-boundary-stubbed:Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V";
          probe.diagnostics.push_back(
              "Self-Healing Android Device DEX probe crossed a stubbed Android framework lifecycle boundary with receiver and Bundle parameter placeholders");
          return probe;
        }
        if (invoked_method.class_descriptor == "Landroid/content/Context;" &&
            invoked_method.method_name == "getAssets" &&
            invoked_method.method_signature ==
                "()Landroid/content/res/AssetManager;") {
          probe.framework_boundary_state = "framework-stubbed";
          probe.framework_boundary_reason =
              "android_context_getassets_stubbed_for_minimal_checkpoint";
          pending_result = MaterializePlaceholderObjectValue(
              "Landroid/content/res/AssetManager;", &next_object_id, &objects);
          pending_result_valid = true;
          probe.execution_state = "framework_boundary_stubbed";
          probe.exact_blocker =
              "framework-boundary-stubbed:Landroid/content/Context;->getAssets()Landroid/content/res/AssetManager;";
          probe.diagnostics.push_back(
              "Self-Healing Android Device DEX probe stubbed Context.getAssets() with a placeholder AssetManager object");
          return probe;
        }

        if (invoked_method.code_off != 0) {
          std::vector<DexRegisterValue> incoming_registers;
          incoming_registers.reserve(register_count);
          for (std::uint32_t index = 0; index < register_count && index < 5u;
               ++index) {
            incoming_registers.push_back(registers[registers_used[index]]);
          }
          const auto invoked_result =
              ExecuteInlineInvokedMethod(bytes, tables, invoked_method,
                                         incoming_registers, &objects);
          probe.decoded_instruction_count +=
              invoked_result.decoded_instruction_count;
          probe.executed_instruction_count +=
              invoked_result.executed_instruction_count;
          if (!invoked_result.errors.empty()) {
            for (const auto& error : invoked_result.errors) {
              AppendUnique(&probe.errors, error);
            }
          }
          for (const auto& diagnostic : invoked_result.diagnostics) {
            AppendUnique(&probe.diagnostics, diagnostic);
          }
          if (!invoked_result.ready || !invoked_result.reached_return) {
            probe.framework_boundary_state = "blocked";
            probe.framework_boundary_reason =
                invoked_result.stubbed_boundary_reason == "none"
                    ? "framework_or_invoke_target_unimplemented"
                    : invoked_result.stubbed_boundary_reason;
            probe.execution_state = invoked_result.execution_state;
            probe.exact_blocker = invoked_result.exact_blocker;
            return probe;
          }
          if (invoked_result.object_register_field_state != "not_reached") {
            mark_object_field_operation(
                invoked_result.object_register_field_operation,
                invoked_result.object_register_field_state,
                invoked_result.object_register_field_reason,
                invoked_result.object_class_descriptor,
                invoked_result.field_class_descriptor,
                invoked_result.field_name,
                invoked_result.field_signature);
          }
          probe.framework_boundary_state =
              invoked_result.used_stubbed_boundary ? "framework-stubbed"
                                                   : "executed";
          probe.framework_boundary_reason =
              invoked_result.stubbed_boundary_reason == "none"
                  ? "inline_super_method_returned"
                  : invoked_result.stubbed_boundary_reason;
          pc += 3u;
          continue;
        }

        probe.framework_boundary_state = "blocked";
        probe.framework_boundary_reason =
            "framework_or_invoke_target_unimplemented";
        probe.execution_state = "framework_boundary_blocked";
        probe.exact_blocker =
            "framework-boundary-unimplemented:" +
            invoked_method.class_descriptor + "->" +
            invoked_method.method_name + invoked_method.method_signature;
        probe.diagnostics.push_back(
            "Self-Healing Android Device DEX probe reached a framework or invoke target boundary that Linuxoid has not implemented yet");
        return probe;
      }
      case 0x71: {  // invoke-static
        if (pc + 2u >= insns_size) {
          probe.app_method_invocation_state = "blocked";
          probe.execution_state = "invoke_truncated";
          probe.exact_blocker = "dex_invoke_instruction_truncated";
          probe.errors.push_back("dex_invoke_instruction_truncated");
          return probe;
        }
        const std::uint16_t method_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint16_t register_word =
            ReadLe16(bytes, insns_off + (pc + 2u) * 2u);
        const std::uint32_t register_count =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint32_t registers_used[5] = {
            static_cast<std::uint32_t>(register_word & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 4u) & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 8u) & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 12u) & 0x0fu),
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu),
        };
        for (std::uint32_t index = 0; index < register_count && index < 5u;
             ++index) {
          if (registers_used[index] >= registers.size()) {
            probe.app_method_invocation_state = "blocked";
            probe.execution_state = "register_out_of_range";
            probe.exact_blocker = "dex_invoke_register_out_of_range";
            probe.errors.push_back("dex_invoke_register_out_of_range");
            return probe;
          }
        }
        DexExecutionCandidate invoked_method;
        if (!ResolveMethodReference(tables, method_index, &invoked_method,
                                    &probe.errors)) {
          probe.app_method_invocation_state = "blocked";
          probe.execution_state = "invoke_unresolved";
          probe.exact_blocker = probe.errors.empty()
                                    ? "dex_invoke_method_resolution_failed"
                                    : probe.errors.front();
          return probe;
        }
        probe.app_invoked_method_class_descriptor = invoked_method.class_descriptor;
        probe.app_invoked_method_name = invoked_method.method_name;
        probe.app_invoked_method_signature = invoked_method.method_signature;
        if (invoked_method.code_off != 0) {
          std::vector<DexRegisterValue> incoming_registers;
          incoming_registers.reserve(register_count);
          for (std::uint32_t index = 0; index < register_count && index < 5u;
               ++index) {
            incoming_registers.push_back(registers[registers_used[index]]);
          }

          const auto invoked_result =
              ExecuteInlineInvokedMethod(bytes, tables, invoked_method,
                                         incoming_registers, &objects);
          probe.decoded_instruction_count +=
              invoked_result.decoded_instruction_count;
          probe.executed_instruction_count +=
              invoked_result.executed_instruction_count;
          if (!invoked_result.errors.empty()) {
            for (const auto& error : invoked_result.errors) {
              AppendUnique(&probe.errors, error);
            }
          }
          for (const auto& diagnostic : invoked_result.diagnostics) {
            AppendUnique(&probe.diagnostics, diagnostic);
          }
          if (!invoked_result.ready || !invoked_result.reached_return) {
            probe.app_method_invocation_state = "blocked";
            probe.execution_state = invoked_result.execution_state;
            probe.exact_blocker = invoked_result.exact_blocker;
            return probe;
          }
          if (invoked_result.object_register_field_state != "not_reached") {
            mark_object_field_operation(
                invoked_result.object_register_field_operation,
                invoked_result.object_register_field_state,
                invoked_result.object_register_field_reason,
                invoked_result.object_class_descriptor,
                invoked_result.field_class_descriptor,
                invoked_result.field_name,
                invoked_result.field_signature);
          }
          if (!invoked_result.invoked_method_name.empty()) {
            probe.invoked_method_class_descriptor =
                invoked_result.invoked_method_class_descriptor;
            probe.invoked_method_name = invoked_result.invoked_method_name;
            probe.invoked_method_signature =
                invoked_result.invoked_method_signature;
          }
          pending_result = invoked_result.returned_value;
          pending_result_high = invoked_result.returned_value_high;
          pending_result_is_wide = invoked_result.returned_value_is_wide;
          pending_result_valid =
              DetermineReturnTypeDescriptor(invoked_method.method_signature) !=
              "V";
          probe.app_method_invocation_state = "invoke-static-returned";
          AppendUnique(
              &probe.diagnostics,
              "Self-Healing Android Device DEX probe executed an app-local invoke-static method and propagated its return value");
          pc += 3u;
          continue;
        }

        probe.app_method_invocation_state = "invoke-static-blocked";
        probe.execution_state = "invoke_static_unimplemented";
        probe.exact_blocker =
            "invoke-static-unimplemented:" + invoked_method.class_descriptor +
            "->" + invoked_method.method_name + invoked_method.method_signature;
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe reached an exact invoke-static boundary that Linuxoid has not implemented yet");
        return probe;
      }
      case 0x77: {  // invoke-static/range
        if (pc + 2u >= insns_size) {
          probe.app_method_invocation_state = "blocked";
          probe.execution_state = "invoke_truncated";
          probe.exact_blocker = "dex_invoke_range_instruction_truncated";
          probe.errors.push_back("dex_invoke_range_instruction_truncated");
          return probe;
        }
        const std::uint16_t method_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint16_t first_register =
            ReadLe16(bytes, insns_off + (pc + 2u) * 2u);
        const std::uint32_t register_count =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (static_cast<std::size_t>(first_register) +
                static_cast<std::size_t>(register_count) >
            registers.size()) {
          probe.app_method_invocation_state = "blocked";
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_invoke_range_register_out_of_range";
          probe.errors.push_back("dex_invoke_range_register_out_of_range");
          return probe;
        }
        DexExecutionCandidate invoked_method;
        if (!ResolveMethodReference(tables, method_index, &invoked_method,
                                    &probe.errors)) {
          probe.app_method_invocation_state = "blocked";
          probe.execution_state = "invoke_unresolved";
          probe.exact_blocker =
              probe.errors.empty() ? "dex_invoke_range_method_resolution_failed"
                                   : probe.errors.front();
          return probe;
        }
        probe.app_invoked_method_class_descriptor =
            invoked_method.class_descriptor;
        probe.app_invoked_method_name = invoked_method.method_name;
        probe.app_invoked_method_signature = invoked_method.method_signature;
        std::vector<DexRegisterValue> incoming_registers;
        incoming_registers.reserve(register_count);
        for (std::uint32_t index = 0; index < register_count; ++index) {
          incoming_registers.push_back(
              registers[static_cast<std::size_t>(first_register) + index]);
        }
        const auto invoked_result =
            ExecuteInlineInvokedMethod(bytes, tables, invoked_method,
                                       incoming_registers, &objects);
        probe.decoded_instruction_count +=
            invoked_result.decoded_instruction_count;
        probe.executed_instruction_count +=
            invoked_result.executed_instruction_count;
        if (!invoked_result.errors.empty()) {
          for (const auto& error : invoked_result.errors) {
            AppendUnique(&probe.errors, error);
          }
        }
        for (const auto& diagnostic : invoked_result.diagnostics) {
          AppendUnique(&probe.diagnostics, diagnostic);
        }
        if (!invoked_result.ready || !invoked_result.reached_return) {
          probe.app_method_invocation_state = "blocked";
          probe.execution_state = invoked_result.execution_state;
          probe.exact_blocker = invoked_result.exact_blocker;
          return probe;
        }
        if (invoked_result.object_register_field_state != "not_reached") {
          mark_object_field_operation(
              invoked_result.object_register_field_operation,
              invoked_result.object_register_field_state,
              invoked_result.object_register_field_reason,
              invoked_result.object_class_descriptor,
              invoked_result.field_class_descriptor,
              invoked_result.field_name,
              invoked_result.field_signature);
        }
        pending_result = invoked_result.returned_value;
        pending_result_high = invoked_result.returned_value_high;
        pending_result_is_wide = invoked_result.returned_value_is_wide;
        pending_result_valid =
            DetermineReturnTypeDescriptor(invoked_method.method_signature) !=
            "V";
        probe.app_method_invocation_state = "invoke-static/range-returned";
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed an app-local invoke-static/range method and propagated its return value");
        pc += 3u;
        continue;
      }
      case 0x70: {  // invoke-direct
        if (pc + 2u >= insns_size) {
          probe.app_method_invocation_state = "blocked";
          probe.execution_state = "invoke_truncated";
          probe.exact_blocker = "dex_invoke_instruction_truncated";
          probe.errors.push_back("dex_invoke_instruction_truncated");
          return probe;
        }
        const std::uint16_t method_index =
            ReadLe16(bytes, insns_off + (pc + 1u) * 2u);
        const std::uint16_t register_word =
            ReadLe16(bytes, insns_off + (pc + 2u) * 2u);
        const std::uint32_t register_count =
            static_cast<std::uint32_t>((code_unit >> 12u) & 0x0fu);
        const std::uint32_t registers_used[5] = {
            static_cast<std::uint32_t>(register_word & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 4u) & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 8u) & 0x0fu),
            static_cast<std::uint32_t>((register_word >> 12u) & 0x0fu),
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x0fu),
        };
        for (std::uint32_t index = 0; index < register_count && index < 5u;
             ++index) {
          if (registers_used[index] >= registers.size()) {
            probe.app_method_invocation_state = "blocked";
            probe.execution_state = "register_out_of_range";
            probe.exact_blocker = "dex_invoke_register_out_of_range";
            probe.errors.push_back("dex_invoke_register_out_of_range");
            return probe;
          }
        }
        if (register_count == 0u ||
            registers[registers_used[0]].kind != DexRegisterValue::Kind::kObject) {
          probe.app_method_invocation_state = "blocked";
          probe.execution_state = "invoke_receiver_missing";
          probe.exact_blocker = "dex_invoke_receiver_missing";
          probe.errors.push_back("dex_invoke_receiver_missing");
          return probe;
        }
        DexExecutionCandidate invoked_method;
        if (!ResolveMethodReference(tables, method_index, &invoked_method,
                                    &probe.errors)) {
          probe.app_method_invocation_state = "blocked";
          probe.execution_state = "invoke_unresolved";
          probe.exact_blocker = probe.errors.empty()
                                    ? "dex_invoke_method_resolution_failed"
                                    : probe.errors.front();
          return probe;
        }
        probe.app_invoked_method_class_descriptor = invoked_method.class_descriptor;
        probe.app_invoked_method_name = invoked_method.method_name;
        probe.app_invoked_method_signature = invoked_method.method_signature;

        std::vector<DexRegisterValue> incoming_registers;
        incoming_registers.reserve(register_count);
        for (std::uint32_t index = 0; index < register_count && index < 5u;
             ++index) {
          incoming_registers.push_back(registers[registers_used[index]]);
        }

        const auto invoked_result =
            ExecuteInlineInvokedMethod(bytes, tables, invoked_method,
                                       incoming_registers, &objects);
        probe.decoded_instruction_count += invoked_result.decoded_instruction_count;
        probe.executed_instruction_count += invoked_result.executed_instruction_count;
        if (!invoked_result.errors.empty()) {
          for (const auto& error : invoked_result.errors) {
            AppendUnique(&probe.errors, error);
          }
        }
        for (const auto& diagnostic : invoked_result.diagnostics) {
          AppendUnique(&probe.diagnostics, diagnostic);
        }
        if (!invoked_result.ready || !invoked_result.reached_return) {
          probe.app_method_invocation_state = "blocked";
          probe.execution_state = invoked_result.execution_state;
          probe.exact_blocker = invoked_result.exact_blocker;
          return probe;
        }
        if (invoked_method.method_name == "<init>") {
          mark_object_field_operation(
              "new-instance+invoke-direct", "object-placeholder",
              "linuxoid_placeholder_object_and_field_state_for_minimal_checkpoint",
              invoked_method.class_descriptor, "", "", "");
        }
        if (invoked_result.object_register_field_state != "not_reached") {
          mark_object_field_operation(
              invoked_result.object_register_field_operation,
              invoked_result.object_register_field_state,
              invoked_result.object_register_field_reason,
              invoked_result.object_class_descriptor,
              invoked_result.field_class_descriptor,
              invoked_result.field_name,
              invoked_result.field_signature);
        }
        pending_result = invoked_result.returned_value;
        pending_result_high = invoked_result.returned_value_high;
        pending_result_is_wide = invoked_result.returned_value_is_wide;
        pending_result_valid =
            DetermineReturnTypeDescriptor(invoked_method.method_signature) != "V";
        probe.app_method_invocation_state = "invoke-direct-returned";
        AppendUnique(
            &probe.diagnostics,
            "Self-Healing Android Device DEX probe executed an app-local invoke-direct method and propagated its return value");
        pc += 3u;
        continue;
      }
      case 0x0e:  // return-void
        probe.returned_value_type = "V";
        probe.reached_return = true;
        probe.execution_state = "returned";
        probe.exact_blocker = "none";
        probe.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed a real bytecode instruction path");
        return probe;
      case 0x0f: {  // return
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (destination >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_register_out_of_range";
          probe.errors.push_back("dex_register_out_of_range");
          return probe;
        }
        probe.returned_value_type =
            return_type_descriptor.empty() ? "I" : return_type_descriptor;
        probe.returned_value =
            std::to_string(registers[destination].int_value);
        probe.reached_return = true;
        probe.execution_state = "returned";
        probe.exact_blocker = "none";
        probe.diagnostics.push_back(
            "Self-Healing Android Device DEX probe executed a real bytecode instruction path");
        return probe;
      }
      case 0x10:  // return-wide
      case 0x11:  // return-object
        probe.returned_value_type = return_type_descriptor;
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
         << "    \"class_loading_state\": \""
         << EscapeJson(probe.class_loading_state) << "\",\n"
         << "    \"target_class_lookup_state\": \""
         << EscapeJson(probe.target_class_lookup_state) << "\",\n"
         << "    \"target_method_lookup_state\": \""
         << EscapeJson(probe.target_method_lookup_state) << "\",\n"
         << "    \"code_item_lookup_state\": \""
         << EscapeJson(probe.code_item_lookup_state) << "\",\n"
         << "    \"lifecycle_receiver_state\": \""
         << EscapeJson(probe.lifecycle_receiver_state) << "\",\n"
         << "    \"lifecycle_receiver_class_descriptor\": \""
         << EscapeJson(probe.lifecycle_receiver_class_descriptor) << "\",\n"
         << "    \"lifecycle_receiver_register\": "
         << probe.lifecycle_receiver_register << ",\n"
         << "    \"lifecycle_parameter_state\": \""
         << EscapeJson(probe.lifecycle_parameter_state) << "\",\n"
         << "    \"lifecycle_parameter_class_descriptor\": \""
         << EscapeJson(probe.lifecycle_parameter_class_descriptor)
         << "\",\n"
         << "    \"lifecycle_parameter_register\": "
         << probe.lifecycle_parameter_register << ",\n"
         << "    \"app_method_invocation_state\": \""
         << EscapeJson(probe.app_method_invocation_state) << "\",\n"
         << "    \"app_invoked_method_class_descriptor\": \""
         << EscapeJson(probe.app_invoked_method_class_descriptor) << "\",\n"
         << "    \"app_invoked_method_name\": \""
         << EscapeJson(probe.app_invoked_method_name) << "\",\n"
         << "    \"app_invoked_method_signature\": \""
         << EscapeJson(probe.app_invoked_method_signature) << "\",\n"
         << "    \"invoked_method_class_descriptor\": \""
         << EscapeJson(probe.invoked_method_class_descriptor) << "\",\n"
         << "    \"invoked_method_name\": \""
         << EscapeJson(probe.invoked_method_name) << "\",\n"
         << "    \"invoked_method_signature\": \""
         << EscapeJson(probe.invoked_method_signature) << "\",\n"
         << "    \"framework_boundary_state\": \""
         << EscapeJson(probe.framework_boundary_state) << "\",\n"
         << "    \"framework_boundary_reason\": \""
         << EscapeJson(probe.framework_boundary_reason) << "\",\n"
         << "    \"object_register_field_operation\": \""
         << EscapeJson(probe.object_register_field_operation) << "\",\n"
         << "    \"object_register_field_state\": \""
         << EscapeJson(probe.object_register_field_state) << "\",\n"
         << "    \"object_register_field_reason\": \""
         << EscapeJson(probe.object_register_field_reason) << "\",\n"
         << "    \"object_class_descriptor\": \""
         << EscapeJson(probe.object_class_descriptor) << "\",\n"
         << "    \"field_class_descriptor\": \""
         << EscapeJson(probe.field_class_descriptor) << "\",\n"
         << "    \"field_name\": \"" << EscapeJson(probe.field_name)
         << "\",\n"
         << "    \"field_signature\": \""
         << EscapeJson(probe.field_signature) << "\",\n"
         << "    \"code_item_offset\": " << probe.code_item_offset << ",\n"
         << "    \"instruction_offset\": " << probe.instruction_offset
         << ",\n"
         << "    \"opcode_value\": " << probe.opcode_value << ",\n"
         << "    \"opcode_name\": \"" << EscapeJson(probe.opcode_name)
         << "\",\n"
         << "    \"last_instruction_offset\": "
         << probe.last_instruction_offset << ",\n"
         << "    \"last_opcode_value\": " << probe.last_opcode_value
         << ",\n"
         << "    \"last_opcode_name\": \""
         << EscapeJson(probe.last_opcode_name) << "\",\n"
         << "    \"decoded_instruction_count\": "
         << probe.decoded_instruction_count << ",\n"
         << "    \"executed_instruction_count\": "
         << probe.executed_instruction_count << ",\n"
         << "    \"returned_value_type\": \""
         << EscapeJson(probe.returned_value_type) << "\",\n"
         << "    \"returned_value\": \"" << EscapeJson(probe.returned_value)
         << "\",\n"
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
      if (!execution_probe_resolved &&
          !report.execution_probe.target_class_descriptor.empty()) {
        report.execution_probe.ready = true;
        report.execution_probe.class_loading_state = "blocked";
        report.execution_probe.target_class_lookup_state = "dex_tables_invalid";
        report.execution_probe.parse_state = "dex_tables_invalid";
        report.execution_probe.execution_state = "entrypoint_lookup_blocked";
        report.execution_probe.exact_blocker =
            file.errors.empty() ? "dex_tables_invalid" : file.errors.front();
        if (!file.errors.empty()) {
          report.execution_probe.errors.push_back(file.errors.front());
        }
        AppendUnique(
            &report.execution_probe.diagnostics,
            "Self-Healing Android Device DEX probe could not reach class lookup because staged DEX table parsing failed");
        execution_probe_resolved = true;
        report.parse_state = report.execution_probe.parse_state;
      }
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
    report.execution_probe.ready = true;
    report.execution_probe.class_loading_state = "blocked";
    report.execution_probe.target_class_lookup_state = "class_missing";
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
