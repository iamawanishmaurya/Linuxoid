#include "wfa/apk_archive.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace wfa {

namespace {

namespace fs = std::filesystem;

constexpr std::uint32_t kLocalFileHeaderSignature = 0x04034b50;
constexpr std::uint32_t kCentralDirectorySignature = 0x02014b50;
constexpr std::uint32_t kEndOfCentralDirectorySignature = 0x06054b50;
constexpr std::size_t kLocalFileHeaderFixedSize = 30;
constexpr std::size_t kCentralDirectoryFixedSize = 46;
constexpr std::size_t kEndOfCentralDirectoryFixedSize = 22;
constexpr std::size_t kEndOfCentralDirectorySearchWindow =
    0xFFFF + kEndOfCentralDirectoryFixedSize;

std::vector<unsigned char> ReadBinaryFile(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("unable to open APK archive: " + path);
  }
  return std::vector<unsigned char>(std::istreambuf_iterator<char>(input),
                                    std::istreambuf_iterator<char>());
}

std::uint16_t ReadLe16(const std::vector<unsigned char>& bytes,
                       std::size_t offset) {
  if (offset + 2 > bytes.size()) {
    throw std::runtime_error("unexpected end of archive while reading u16");
  }
  return static_cast<std::uint16_t>(bytes[offset]) |
         (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::uint32_t ReadLe32(const std::vector<unsigned char>& bytes,
                       std::size_t offset) {
  if (offset + 4 > bytes.size()) {
    throw std::runtime_error("unexpected end of archive while reading u32");
  }
  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::string NormalizeArchivePath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  while (path.rfind("./", 0) == 0) {
    path.erase(0, 2);
  }
  while (!path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  return path;
}

std::size_t FindEndOfCentralDirectory(const std::vector<unsigned char>& bytes) {
  if (bytes.size() < kEndOfCentralDirectoryFixedSize) {
    throw std::runtime_error("APK archive is too small to contain EOCD");
  }

  const std::size_t search_start =
      bytes.size() > kEndOfCentralDirectorySearchWindow
          ? bytes.size() - kEndOfCentralDirectorySearchWindow
          : 0;

  for (std::size_t index = bytes.size() - kEndOfCentralDirectoryFixedSize + 1;
       index-- > search_start;) {
    if (ReadLe32(bytes, index) == kEndOfCentralDirectorySignature) {
      return index;
    }
  }

  throw std::runtime_error("unable to locate ZIP end-of-central-directory");
}

}  // namespace

std::vector<ApkArchiveEntry> ListApkArchiveEntries(const std::string& apk_path) {
  const auto bytes = ReadBinaryFile(apk_path);
  const std::size_t eocd_offset = FindEndOfCentralDirectory(bytes);
  const std::uint16_t entry_count = ReadLe16(bytes, eocd_offset + 10);
  const std::uint32_t central_directory_size =
      ReadLe32(bytes, eocd_offset + 12);
  const std::uint32_t central_directory_offset =
      ReadLe32(bytes, eocd_offset + 16);

  if (central_directory_offset + central_directory_size > bytes.size()) {
    throw std::runtime_error(
        "central directory extends beyond APK archive boundary");
  }

  std::vector<ApkArchiveEntry> entries;
  entries.reserve(entry_count);

  std::size_t cursor = central_directory_offset;
  for (std::uint16_t index = 0; index < entry_count; ++index) {
    if (cursor + kCentralDirectoryFixedSize > bytes.size() ||
        ReadLe32(bytes, cursor) != kCentralDirectorySignature) {
      throw std::runtime_error(
          "invalid central directory entry in APK archive");
    }

    const std::uint16_t compression_method = ReadLe16(bytes, cursor + 10);
    const std::uint32_t compressed_size = ReadLe32(bytes, cursor + 20);
    const std::uint32_t uncompressed_size = ReadLe32(bytes, cursor + 24);
    const std::uint16_t file_name_length = ReadLe16(bytes, cursor + 28);
    const std::uint16_t extra_field_length = ReadLe16(bytes, cursor + 30);
    const std::uint16_t comment_length = ReadLe16(bytes, cursor + 32);
    const std::uint32_t local_header_offset = ReadLe32(bytes, cursor + 42);

    const std::size_t file_name_offset = cursor + kCentralDirectoryFixedSize;
    const std::size_t record_end = file_name_offset + file_name_length +
                                   extra_field_length + comment_length;
    if (record_end > bytes.size()) {
      throw std::runtime_error("truncated APK central directory record");
    }

    std::string path(bytes.begin() + static_cast<std::ptrdiff_t>(file_name_offset),
                     bytes.begin() +
                         static_cast<std::ptrdiff_t>(file_name_offset +
                                                     file_name_length));
    path = NormalizeArchivePath(path);

    entries.push_back(ApkArchiveEntry{
        .path = path,
        .compression_method = compression_method,
        .compressed_size = compressed_size,
        .uncompressed_size = uncompressed_size,
        .local_header_offset = local_header_offset,
        .is_directory = !path.empty() && path.back() == '/',
    });
    cursor = record_end;
  }

  std::sort(entries.begin(), entries.end(),
            [](const ApkArchiveEntry& left, const ApkArchiveEntry& right) {
              return left.path < right.path;
            });
  return entries;
}

ApkArchiveReadResult ReadApkArchiveEntry(const std::string& apk_path,
                                         const std::string& entry_path) {
  ApkArchiveReadResult result;
  const std::string normalized_path = NormalizeArchivePath(entry_path);
  const auto entries = ListApkArchiveEntries(apk_path);
  const auto it = std::find_if(entries.begin(), entries.end(),
                               [&](const ApkArchiveEntry& entry) {
                                 return entry.path == normalized_path;
                               });
  if (it == entries.end()) {
    result.failure_reason = "archive entry not found";
    return result;
  }

  result.found = true;
  result.entry = *it;
  if (it->is_directory) {
    result.failure_reason = "archive entry is a directory";
    return result;
  }
  if (it->compression_method != 0) {
    result.failure_reason = "unsupported_zip_compression_method_" +
                            std::to_string(it->compression_method);
    return result;
  }

  const auto bytes = ReadBinaryFile(apk_path);
  const std::size_t local_header_offset = it->local_header_offset;
  if (local_header_offset + kLocalFileHeaderFixedSize > bytes.size() ||
      ReadLe32(bytes, local_header_offset) != kLocalFileHeaderSignature) {
    result.failure_reason = "invalid_local_file_header";
    return result;
  }

  const std::uint16_t file_name_length =
      ReadLe16(bytes, local_header_offset + 26);
  const std::uint16_t extra_field_length =
      ReadLe16(bytes, local_header_offset + 28);
  const std::size_t data_offset = local_header_offset +
                                  kLocalFileHeaderFixedSize + file_name_length +
                                  extra_field_length;
  const std::size_t data_end =
      data_offset + static_cast<std::size_t>(it->compressed_size);
  if (data_end > bytes.size()) {
    result.failure_reason = "archive entry extends beyond APK boundary";
    return result;
  }

  result.readable = true;
  result.contents.assign(bytes.begin() + static_cast<std::ptrdiff_t>(data_offset),
                         bytes.begin() + static_cast<std::ptrdiff_t>(data_end));
  return result;
}

}  // namespace wfa
