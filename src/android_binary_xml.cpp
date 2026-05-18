#include "wfa/android_binary_xml.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace wfa {

namespace {

constexpr std::uint16_t kResStringPoolType = 0x0001u;
constexpr std::uint16_t kResXmlType = 0x0003u;
constexpr std::uint16_t kResXmlStartNamespaceType = 0x0100u;
constexpr std::uint16_t kResXmlEndNamespaceType = 0x0101u;
constexpr std::uint16_t kResXmlStartElementType = 0x0102u;
constexpr std::uint16_t kResXmlEndElementType = 0x0103u;
constexpr std::uint32_t kNoStringIndex = 0xFFFFFFFFu;
constexpr std::uint32_t kStringPoolUtf8Flag = 0x00000100u;

constexpr std::uint8_t kTypeNull = 0x00u;
constexpr std::uint8_t kTypeReference = 0x01u;
constexpr std::uint8_t kTypeAttribute = 0x02u;
constexpr std::uint8_t kTypeString = 0x03u;
constexpr std::uint8_t kTypeIntDec = 0x10u;
constexpr std::uint8_t kTypeIntHex = 0x11u;
constexpr std::uint8_t kTypeIntBoolean = 0x12u;

struct ChunkHeader {
  std::uint16_t type = 0u;
  std::uint16_t header_size = 0u;
  std::uint32_t size = 0u;
};

void AppendUnique(std::vector<std::string>* values, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(value);
  }
}

bool ReadLe16(std::string_view bytes, std::size_t offset,
              std::uint16_t* value) {
  if (offset + 2 > bytes.size()) {
    return false;
  }
  *value = static_cast<std::uint8_t>(bytes[offset]) |
           (static_cast<std::uint16_t>(
                static_cast<std::uint8_t>(bytes[offset + 1]))
            << 8);
  return true;
}

bool ReadLe32(std::string_view bytes, std::size_t offset,
              std::uint32_t* value) {
  if (offset + 4 > bytes.size()) {
    return false;
  }
  *value = static_cast<std::uint8_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(
                static_cast<std::uint8_t>(bytes[offset + 1]))
            << 8) |
           (static_cast<std::uint32_t>(
                static_cast<std::uint8_t>(bytes[offset + 2]))
            << 16) |
           (static_cast<std::uint32_t>(
                static_cast<std::uint8_t>(bytes[offset + 3]))
            << 24);
  return true;
}

bool ParseChunkHeader(std::string_view bytes, std::size_t offset,
                      ChunkHeader* header) {
  return ReadLe16(bytes, offset, &header->type) &&
         ReadLe16(bytes, offset + 2, &header->header_size) &&
         ReadLe32(bytes, offset + 4, &header->size) &&
         header->size >= header->header_size &&
         offset + header->size <= bytes.size();
}

std::string EscapeXml(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (const char character : value) {
    switch (character) {
      case '&':
        escaped += "&amp;";
        break;
      case '<':
        escaped += "&lt;";
        break;
      case '>':
        escaped += "&gt;";
        break;
      case '"':
        escaped += "&quot;";
        break;
      case '\'':
        escaped += "&apos;";
        break;
      default:
        escaped.push_back(character);
        break;
    }
  }
  return escaped;
}

std::string EncodeCodePoint(std::uint32_t code_point) {
  std::string output;
  if (code_point <= 0x7Fu) {
    output.push_back(static_cast<char>(code_point));
  } else if (code_point <= 0x7FFu) {
    output.push_back(static_cast<char>(0xC0u | (code_point >> 6)));
    output.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
  } else if (code_point <= 0xFFFFu) {
    output.push_back(static_cast<char>(0xE0u | (code_point >> 12)));
    output.push_back(static_cast<char>(0x80u | ((code_point >> 6) & 0x3Fu)));
    output.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
  } else {
    output.push_back(static_cast<char>(0xF0u | (code_point >> 18)));
    output.push_back(static_cast<char>(0x80u | ((code_point >> 12) & 0x3Fu)));
    output.push_back(static_cast<char>(0x80u | ((code_point >> 6) & 0x3Fu)));
    output.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
  }
  return output;
}

bool ReadUtf8Length(std::string_view bytes, std::size_t* cursor,
                    std::uint32_t* value) {
  if (*cursor >= bytes.size()) {
    return false;
  }
  const std::uint8_t first = static_cast<std::uint8_t>(bytes[*cursor]);
  ++(*cursor);
  if ((first & 0x80u) == 0u) {
    *value = first;
    return true;
  }
  if (*cursor >= bytes.size()) {
    return false;
  }
  const std::uint8_t second = static_cast<std::uint8_t>(bytes[*cursor]);
  ++(*cursor);
  *value = ((first & 0x7Fu) << 7u) | second;
  return true;
}

bool ReadUtf16Length(std::string_view bytes, std::size_t* cursor,
                     std::uint32_t* value) {
  std::uint16_t first = 0u;
  if (!ReadLe16(bytes, *cursor, &first)) {
    return false;
  }
  *cursor += 2;
  if ((first & 0x8000u) == 0u) {
    *value = first;
    return true;
  }
  std::uint16_t second = 0u;
  if (!ReadLe16(bytes, *cursor, &second)) {
    return false;
  }
  *cursor += 2;
  *value = ((first & 0x7FFFu) << 16u) | second;
  return true;
}

bool ParseStringPool(std::string_view bytes, std::size_t chunk_offset,
                     const ChunkHeader& header,
                     std::vector<std::string>* strings,
                     std::vector<std::string>* errors) {
  if (header.header_size < 28u) {
    AppendUnique(errors, "binary_xml_string_pool_header_too_small");
    return false;
  }

  std::uint32_t string_count = 0u;
  std::uint32_t flags = 0u;
  std::uint32_t strings_start = 0u;
  if (!ReadLe32(bytes, chunk_offset + 8u, &string_count) ||
      !ReadLe32(bytes, chunk_offset + 16u, &flags) ||
      !ReadLe32(bytes, chunk_offset + 20u, &strings_start)) {
    AppendUnique(errors, "binary_xml_string_pool_truncated");
    return false;
  }

  const std::size_t offsets_base = chunk_offset + header.header_size;
  const std::size_t strings_base = chunk_offset + strings_start;
  const bool utf8 = (flags & kStringPoolUtf8Flag) != 0u;
  strings->clear();
  strings->reserve(string_count);

  for (std::uint32_t index = 0u; index < string_count; ++index) {
    std::uint32_t string_offset = 0u;
    if (!ReadLe32(bytes, offsets_base + index * 4u, &string_offset)) {
      AppendUnique(errors, "binary_xml_string_offsets_truncated");
      return false;
    }
    std::size_t cursor = strings_base + string_offset;
    if (cursor >= chunk_offset + header.size) {
      AppendUnique(errors, "binary_xml_string_offset_out_of_bounds");
      return false;
    }

    std::string decoded;
    if (utf8) {
      std::uint32_t ignored_utf16_length = 0u;
      std::uint32_t byte_length = 0u;
      if (!ReadUtf8Length(bytes, &cursor, &ignored_utf16_length) ||
          !ReadUtf8Length(bytes, &cursor, &byte_length) ||
          cursor + byte_length > bytes.size()) {
        AppendUnique(errors, "binary_xml_utf8_string_truncated");
        return false;
      }
      decoded.assign(bytes.substr(cursor, byte_length));
    } else {
      std::uint32_t code_unit_length = 0u;
      if (!ReadUtf16Length(bytes, &cursor, &code_unit_length)) {
        AppendUnique(errors, "binary_xml_utf16_length_truncated");
        return false;
      }
      for (std::uint32_t unit_index = 0u; unit_index < code_unit_length;
           ++unit_index) {
        std::uint16_t code_unit = 0u;
        if (!ReadLe16(bytes, cursor, &code_unit)) {
          AppendUnique(errors, "binary_xml_utf16_string_truncated");
          return false;
        }
        cursor += 2;
        if (code_unit >= 0xD800u && code_unit <= 0xDBFFu &&
            unit_index + 1u < code_unit_length) {
          std::uint16_t low_surrogate = 0u;
          if (!ReadLe16(bytes, cursor, &low_surrogate)) {
            AppendUnique(errors, "binary_xml_utf16_surrogate_truncated");
            return false;
          }
          cursor += 2;
          ++unit_index;
          const std::uint32_t code_point =
              0x10000u + (((code_unit - 0xD800u) << 10u) |
                          (low_surrogate - 0xDC00u));
          decoded += EncodeCodePoint(code_point);
          continue;
        }
        decoded += EncodeCodePoint(code_unit);
      }
    }
    strings->push_back(decoded);
  }
  return true;
}

std::string StringAt(const std::vector<std::string>& strings,
                     std::uint32_t index) {
  if (index == kNoStringIndex || index >= strings.size()) {
    return {};
  }
  return strings[index];
}

std::string RenderTypedValue(const std::vector<std::string>& strings,
                             std::uint32_t raw_value_index,
                             std::uint8_t data_type, std::uint32_t data) {
  if (raw_value_index != kNoStringIndex) {
    return StringAt(strings, raw_value_index);
  }
  switch (data_type) {
    case kTypeNull:
      return "";
    case kTypeString:
      return StringAt(strings, data);
    case kTypeIntDec:
      return std::to_string(data);
    case kTypeIntHex: {
      std::ostringstream output;
      output << "0x" << std::hex << std::nouppercase << data;
      return output.str();
    }
    case kTypeIntBoolean:
      return data != 0u ? "true" : "false";
    case kTypeReference: {
      std::ostringstream output;
      output << "@0x" << std::hex << std::nouppercase << data;
      return output.str();
    }
    case kTypeAttribute: {
      std::ostringstream output;
      output << "?0x" << std::hex << std::nouppercase << data;
      return output.str();
    }
    default: {
      std::ostringstream output;
      output << "0x" << std::hex << std::nouppercase << data;
      return output.str();
    }
  }
}

}  // namespace

bool LooksLikeAndroidBinaryXml(std::string_view bytes) {
  ChunkHeader header;
  return ParseChunkHeader(bytes, 0u, &header) && header.type == kResXmlType;
}

AndroidBinaryXmlDecodeResult DecodeAndroidBinaryXmlToText(
    std::string_view bytes) {
  AndroidBinaryXmlDecodeResult result;
  ChunkHeader file_header;
  if (!ParseChunkHeader(bytes, 0u, &file_header) ||
      file_header.type != kResXmlType) {
    AppendUnique(&result.errors, "binary_xml_header_invalid");
    return result;
  }

  std::vector<std::string> strings;
  std::map<std::uint32_t, std::vector<std::string>> namespace_prefix_stack;
  std::ostringstream xml;
  std::vector<std::string> element_stack;

  std::size_t cursor = file_header.header_size;
  while (cursor < file_header.size) {
    ChunkHeader chunk;
    if (!ParseChunkHeader(bytes, cursor, &chunk)) {
      AppendUnique(&result.errors, "binary_xml_chunk_truncated");
      return result;
    }

    if (chunk.type == kResStringPoolType) {
      if (!ParseStringPool(bytes, cursor, chunk, &strings, &result.errors)) {
        return result;
      }
      cursor += chunk.size;
      continue;
    }

    if (chunk.type == kResXmlStartNamespaceType ||
        chunk.type == kResXmlEndNamespaceType) {
      if (chunk.size < 24u) {
        AppendUnique(&result.errors, "binary_xml_namespace_chunk_invalid");
        return result;
      }
      std::uint32_t prefix_index = 0u;
      std::uint32_t uri_index = 0u;
      if (!ReadLe32(bytes, cursor + 16u, &prefix_index) ||
          !ReadLe32(bytes, cursor + 20u, &uri_index)) {
        AppendUnique(&result.errors, "binary_xml_namespace_chunk_truncated");
        return result;
      }
      if (chunk.type == kResXmlStartNamespaceType) {
        namespace_prefix_stack[uri_index].push_back(
            StringAt(strings, prefix_index));
      } else {
        auto it = namespace_prefix_stack.find(uri_index);
        if (it != namespace_prefix_stack.end() && !it->second.empty()) {
          it->second.pop_back();
          if (it->second.empty()) {
            namespace_prefix_stack.erase(it);
          }
        }
      }
      cursor += chunk.size;
      continue;
    }

    if (chunk.type == kResXmlStartElementType) {
      if (chunk.size < 36u) {
        AppendUnique(&result.errors, "binary_xml_start_element_invalid");
        return result;
      }
      std::uint32_t name_index = 0u;
      std::uint16_t attribute_start = 0u;
      std::uint16_t attribute_size = 0u;
      std::uint16_t attribute_count = 0u;
      if (!ReadLe32(bytes, cursor + 20u, &name_index) ||
          !ReadLe16(bytes, cursor + 24u, &attribute_start) ||
          !ReadLe16(bytes, cursor + 26u, &attribute_size) ||
          !ReadLe16(bytes, cursor + 28u, &attribute_count)) {
        AppendUnique(&result.errors, "binary_xml_start_element_truncated");
        return result;
      }
      const std::string element_name = StringAt(strings, name_index);
      if (element_name.empty()) {
        AppendUnique(&result.errors, "binary_xml_element_name_missing");
        return result;
      }

      xml << "<" << element_name;
      if (element_stack.empty()) {
        for (const auto& [uri_index, prefixes] : namespace_prefix_stack) {
          if (prefixes.empty()) {
            continue;
          }
          const std::string uri = StringAt(strings, uri_index);
          if (uri.empty()) {
            continue;
          }
          xml << " xmlns";
          if (!prefixes.back().empty()) {
            xml << ":" << prefixes.back();
          }
          xml << "=\"" << EscapeXml(uri) << "\"";
        }
      }

      const std::size_t attributes_offset = cursor + 16u + attribute_start;
      for (std::uint16_t index = 0u; index < attribute_count; ++index) {
        const std::size_t attribute_offset =
            attributes_offset + static_cast<std::size_t>(index) * attribute_size;
        std::uint32_t attribute_ns_index = 0u;
        std::uint32_t attribute_name_index = 0u;
        std::uint32_t raw_value_index = 0u;
        std::uint32_t data = 0u;
        std::uint8_t data_type = 0u;
        if (!ReadLe32(bytes, attribute_offset, &attribute_ns_index) ||
            !ReadLe32(bytes, attribute_offset + 4u, &attribute_name_index) ||
            !ReadLe32(bytes, attribute_offset + 8u, &raw_value_index) ||
            attribute_offset + 16u > bytes.size() ||
            !ReadLe32(bytes, attribute_offset + 16u, &data)) {
          AppendUnique(&result.errors, "binary_xml_attribute_truncated");
          return result;
        }
        data_type = static_cast<std::uint8_t>(bytes[attribute_offset + 15u]);
        std::string attribute_name = StringAt(strings, attribute_name_index);
        if (attribute_name.empty()) {
          AppendUnique(&result.errors, "binary_xml_attribute_name_missing");
          return result;
        }
        if (attribute_ns_index != kNoStringIndex) {
          const auto it = namespace_prefix_stack.find(attribute_ns_index);
          if (it != namespace_prefix_stack.end() && !it->second.empty() &&
              !it->second.back().empty()) {
            attribute_name = it->second.back() + ":" + attribute_name;
          }
        }
        const std::string attribute_value = RenderTypedValue(
            strings, raw_value_index, data_type, data);
        xml << " " << attribute_name << "=\""
            << EscapeXml(attribute_value) << "\"";
      }

      xml << ">\n";
      element_stack.push_back(element_name);
      cursor += chunk.size;
      continue;
    }

    if (chunk.type == kResXmlEndElementType) {
      if (chunk.size < 24u) {
        AppendUnique(&result.errors, "binary_xml_end_element_invalid");
        return result;
      }
      std::uint32_t name_index = 0u;
      if (!ReadLe32(bytes, cursor + 20u, &name_index)) {
        AppendUnique(&result.errors, "binary_xml_end_element_truncated");
        return result;
      }
      const std::string element_name = StringAt(strings, name_index);
      if (element_stack.empty()) {
        AppendUnique(&result.errors, "binary_xml_end_element_without_start");
        return result;
      }
      xml << "</" << (element_name.empty() ? element_stack.back() : element_name)
          << ">\n";
      element_stack.pop_back();
      cursor += chunk.size;
      continue;
    }

    cursor += chunk.size;
  }

  if (!element_stack.empty()) {
    AppendUnique(&result.errors, "binary_xml_element_stack_unbalanced");
    return result;
  }

  result.xml_text = xml.str();
  result.success = !result.xml_text.empty();
  if (!result.success) {
    AppendUnique(&result.errors, "binary_xml_decode_produced_empty_xml");
  }
  return result;
}

}  // namespace wfa
