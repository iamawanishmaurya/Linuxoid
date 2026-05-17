#ifndef WFA_APK_ARCHIVE_HPP
#define WFA_APK_ARCHIVE_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace wfa {

struct ApkArchiveEntry {
  std::string path;
  std::uint16_t compression_method = 0;
  std::uint32_t compressed_size = 0;
  std::uint32_t uncompressed_size = 0;
  std::uint32_t local_header_offset = 0;
  bool is_directory = false;
};

struct ApkArchiveReadResult {
  bool found = false;
  bool readable = false;
  ApkArchiveEntry entry;
  std::string contents;
  std::string failure_reason;
};

struct OpenedApkArchive {
  std::string apk_path;
  std::vector<unsigned char> bytes;
  std::vector<ApkArchiveEntry> entries;
};

OpenedApkArchive OpenApkArchive(const std::string& apk_path);
std::vector<ApkArchiveEntry> ListApkArchiveEntries(const std::string& apk_path);
const std::vector<ApkArchiveEntry>& ListApkArchiveEntries(
    const OpenedApkArchive& archive);
ApkArchiveReadResult ReadApkArchiveEntry(const std::string& apk_path,
                                         const std::string& entry_path);
ApkArchiveReadResult ReadApkArchiveEntry(const OpenedApkArchive& archive,
                                         const std::string& entry_path);

}  // namespace wfa

#endif  // WFA_APK_ARCHIVE_HPP
