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
  std::map<std::string, DexRegisterValue> fields;
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
    case 0x22:
      return "new-instance";
    case 0x52:
      return "iget";
    case 0x54:
      return "iget-object";
    case 0x59:
      return "iput";
    case 0x5b:
      return "iput-object";
    case 0x6e:
      return "invoke-virtual";
    case 0x6f:
      return "invoke-super";
    case 0x70:
      return "invoke-direct";
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
  std::string execution_state = "not_attempted";
  std::string exact_blocker = "none";
  DexRegisterValue returned_value;
  int decoded_instruction_count = 0;
  int executed_instruction_count = 0;
  std::uint32_t last_instruction_offset = 0;
  std::uint16_t last_opcode_value = 0;
  std::string last_opcode_name;
  std::vector<std::string> diagnostics;
  std::vector<std::string> errors;
};

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
    result.exact_blocker = "dex_invoked_method_code_item_missing";
    result.errors.push_back("dex_invoked_method_code_item_missing");
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
  std::vector<DexRegisterValue> registers(
      std::max<std::size_t>(registers_size,
                            std::max<std::size_t>(incoming_registers.size(), 1u)));
  for (std::size_t index = 0;
       index < incoming_registers.size() && index < registers.size(); ++index) {
    registers[index] = incoming_registers[index];
  }
  const std::string return_type_descriptor =
      DetermineReturnTypeDescriptor(candidate.method_signature);

  result.ready = true;
  result.execution_state = "interpreting";
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
        pc += 2u;
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
  std::uint32_t next_object_id = 1u;
  bool pending_result_valid = false;
  DexRegisterValue pending_result;

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
      case 0x0a: {  // move-result
        const std::uint32_t destination =
            static_cast<std::uint32_t>((code_unit >> 8u) & 0x00ffu);
        if (destination >= registers.size()) {
          probe.execution_state = "register_out_of_range";
          probe.exact_blocker = "dex_move_result_register_out_of_range";
          probe.errors.push_back("dex_move_result_register_out_of_range");
          return probe;
        }
        if (!pending_result_valid) {
          probe.execution_state = "move_result_without_pending_value";
          probe.exact_blocker = "dex_move_result_without_pending_value";
          probe.errors.push_back("dex_move_result_without_pending_value");
          return probe;
        }
        registers[destination] = pending_result;
        pending_result_valid = false;
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
        pending_result = invoked_result.returned_value;
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
