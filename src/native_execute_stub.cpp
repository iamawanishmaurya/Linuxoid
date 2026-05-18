#include "wfa/native_execute_stub.hpp"

#include "wfa/asset_manager_stub.hpp"
#include "wfa/jni_stub.hpp"
#include "wfa/native_types.hpp"
#include "wfa/signal_handler.hpp"

#include <dlfcn.h>
#include <elf.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

std::string BuildSignalTrapDetail(const NativeSignalTrapInfo& trap);

namespace {

using JniOnLoadFn = int (*)(JavaVM*, void*);
using JniRegistrationCallbackFn = void (*)(JNIEnv*);

struct LoadedLibraryHandle {
  std::string path;
  void* handle = nullptr;
};

struct PostJniDispatchBoundary {
  std::string state = "managed_activity_dispatch_required";
  std::string symbol_kind = "none";
  std::string symbol_name;
  std::string reason = "no_post_jni_dispatch_symbols_detected";
};

struct RegistrationDispatchOutcome {
  std::string dispatch_state = "not_attempted";
  std::string dispatch_symbol_kind = "none";
  std::string dispatch_symbol;
  std::string outcome_state = "not_applicable";
  std::string outcome_reason = "none";
  std::string class_name;
  int method_count = 0;
  std::string error_detail;
};

struct ManagedActivityDispatchAttempt {
  std::string dispatch_state = "not_attempted";
  std::string dispatch_reason = "none";
  std::string component;
  std::string class_name;
  std::string class_descriptor;
  std::string method_name = "onCreate";
  std::string method_signature = "(Landroid/os/Bundle;)V";
  std::string runtime_binding_state = "not_attempted";
};

#ifndef SHT_GNU_versym
#define SHT_GNU_versym 0x6fffffff
#endif

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

std::string ReplaceAll(std::string value, char from, char to) {
  std::replace(value.begin(), value.end(), from, to);
  return value;
}

std::string ResolveActivityClassName(const std::string& package_name,
                                     const std::string& launcher_component) {
  if (launcher_component.empty()) {
    return "";
  }
  const std::size_t slash = launcher_component.find('/');
  std::string class_name =
      slash == std::string::npos ? launcher_component
                                 : launcher_component.substr(slash + 1);
  if (class_name.empty()) {
    return "";
  }
  if (class_name.front() == '.') {
    return package_name + class_name;
  }
  if (class_name.find('.') == std::string::npos && !package_name.empty()) {
    return package_name + "." + class_name;
  }
  return class_name;
}

std::string BuildClassDescriptor(const std::string& class_name) {
  if (class_name.empty()) {
    return "";
  }
  return "L" + ReplaceAll(class_name, '.', '/') + ";";
}

ManagedActivityDispatchAttempt BuildManagedActivityDispatchAttempt(
    const NativeExecuteRequest& request) {
  ManagedActivityDispatchAttempt attempt;
  attempt.component = request.launcher_component;
  attempt.class_name =
      ResolveActivityClassName(request.package_name, request.launcher_component);
  attempt.class_descriptor = BuildClassDescriptor(attempt.class_name);
  if (attempt.component.empty() || attempt.class_name.empty() ||
      attempt.class_descriptor.empty()) {
    attempt.dispatch_state = "blocked";
    attempt.dispatch_reason = "managed_activity_component_unresolved";
    attempt.runtime_binding_state = "component_unresolved";
    return attempt;
  }

  attempt.dispatch_state = "linuxoid_dispatch_attempted";
  attempt.dispatch_reason =
      "jni_registration_completed_and_managed_activity_dispatch_target_selected";
  attempt.runtime_binding_state = "managed_runtime_context_required";
  return attempt;
}

bool IsEntrypointLibraryName(const std::string& file_name) {
  const auto preferred = BuildEntrypointLibraryNames();
  return std::find(preferred.begin(), preferred.end(), file_name) !=
         preferred.end();
}

bool IsPreferredJniLibraryName(const std::string& file_name) {
  return file_name == "libjni_latinime.so" || file_name == "libmain.so" ||
         file_name == "libcalculator.so" || file_name == "libapp.so";
}

int DetermineLibraryOrderRank(const std::string& file_name) {
  if (file_name == "libc++_shared.so") {
    return 0;
  }
  if (file_name == "libmain.so") {
    return 10;
  }
  if (file_name == "libcalculator.so") {
    return 11;
  }
  if (file_name == "libapp.so") {
    return 12;
  }
  if (file_name == "libjni_latinime.so") {
    return 13;
  }
  if (file_name.find("jni") != std::string::npos) {
    return 14;
  }
  return 20;
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
           << "\"status\": \"" << EscapeJson(result.status) << "\", "
           << "\"error_detail\": \"" << EscapeJson(result.error_detail)
           << "\""
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

std::string ResolveExecutableDirectory() {
  char buffer[PATH_MAX] = {};
  const ssize_t size = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (size <= 0) {
    return {};
  }
  buffer[size] = '\0';
  return fs::path(buffer).parent_path().string();
}

std::vector<std::string> BuildAndroidCompatibilityShimPaths() {
  const std::string executable_directory = ResolveExecutableDirectory();
  if (executable_directory.empty()) {
    return {};
  }
  return {
      (fs::path(executable_directory) / "libc.so").string(),
      (fs::path(executable_directory) / "liblog.so").string(),
      (fs::path(executable_directory) / "libm.so").string(),
      (fs::path(executable_directory) / "libdl.so").string(),
  };
}

std::vector<std::string> ReadElfNeededSharedLibraries(
    const std::string& library_path) {
  std::ifstream input(library_path, std::ios::binary);
  if (!input) {
    return {};
  }

  std::vector<char> payload((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
  if (payload.size() < sizeof(Elf64_Ehdr)) {
    return {};
  }

  const auto* header =
      reinterpret_cast<const Elf64_Ehdr*>(payload.data());
  if (!(header->e_ident[EI_MAG0] == ELFMAG0 &&
        header->e_ident[EI_MAG1] == ELFMAG1 &&
        header->e_ident[EI_MAG2] == ELFMAG2 &&
        header->e_ident[EI_MAG3] == ELFMAG3) ||
      header->e_ident[EI_CLASS] != ELFCLASS64 ||
      header->e_ident[EI_DATA] != ELFDATA2LSB ||
      header->e_shoff == 0 || header->e_shentsize < sizeof(Elf64_Shdr) ||
      header->e_shnum == 0) {
    return {};
  }

  const std::size_t section_table_end =
      static_cast<std::size_t>(header->e_shoff) +
      static_cast<std::size_t>(header->e_shentsize) *
          static_cast<std::size_t>(header->e_shnum);
  if (section_table_end > payload.size()) {
    return {};
  }

  const auto* sections = reinterpret_cast<const Elf64_Shdr*>(
      payload.data() + header->e_shoff);
  const Elf64_Shdr* dynamic_section = nullptr;
  const Elf64_Shdr* string_section = nullptr;
  for (int index = 0; index < header->e_shnum; ++index) {
    if (sections[index].sh_type == SHT_DYNAMIC) {
      dynamic_section = &sections[index];
      if (sections[index].sh_link < static_cast<Elf64_Word>(header->e_shnum)) {
        string_section = &sections[sections[index].sh_link];
      }
      break;
    }
  }

  if (dynamic_section == nullptr || string_section == nullptr ||
      dynamic_section->sh_entsize < sizeof(Elf64_Dyn)) {
    return {};
  }

  const std::size_t dynamic_end =
      static_cast<std::size_t>(dynamic_section->sh_offset) +
      static_cast<std::size_t>(dynamic_section->sh_size);
  const std::size_t string_end =
      static_cast<std::size_t>(string_section->sh_offset) +
      static_cast<std::size_t>(string_section->sh_size);
  if (dynamic_end > payload.size() || string_end > payload.size()) {
    return {};
  }

  const auto* dynamic_entries = reinterpret_cast<const Elf64_Dyn*>(
      payload.data() + dynamic_section->sh_offset);
  const std::size_t dynamic_count =
      dynamic_section->sh_size / sizeof(Elf64_Dyn);
  const char* string_table = payload.data() + string_section->sh_offset;

  std::vector<std::string> needed_libraries;
  for (std::size_t index = 0; index < dynamic_count; ++index) {
    if (dynamic_entries[index].d_tag != DT_NEEDED) {
      continue;
    }
    const auto offset =
        static_cast<std::size_t>(dynamic_entries[index].d_un.d_val);
    if (offset >= string_section->sh_size) {
      continue;
    }
    needed_libraries.emplace_back(string_table + offset);
  }
  return needed_libraries;
}

std::vector<std::string> ReadElfDynamicSymbolNames(
    const std::string& library_path) {
  std::ifstream input(library_path, std::ios::binary);
  if (!input) {
    return {};
  }

  std::vector<char> payload((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
  if (payload.size() < sizeof(Elf64_Ehdr)) {
    return {};
  }

  const auto* header = reinterpret_cast<const Elf64_Ehdr*>(payload.data());
  if (!(header->e_ident[EI_MAG0] == ELFMAG0 &&
        header->e_ident[EI_MAG1] == ELFMAG1 &&
        header->e_ident[EI_MAG2] == ELFMAG2 &&
        header->e_ident[EI_MAG3] == ELFMAG3) ||
      header->e_ident[EI_CLASS] != ELFCLASS64 ||
      header->e_ident[EI_DATA] != ELFDATA2LSB ||
      header->e_shoff == 0 || header->e_shentsize < sizeof(Elf64_Shdr) ||
      header->e_shnum == 0) {
    return {};
  }

  const std::size_t section_table_end =
      static_cast<std::size_t>(header->e_shoff) +
      static_cast<std::size_t>(header->e_shentsize) *
          static_cast<std::size_t>(header->e_shnum);
  if (section_table_end > payload.size()) {
    return {};
  }

  const auto* sections = reinterpret_cast<const Elf64_Shdr*>(
      payload.data() + header->e_shoff);
  const Elf64_Shdr* dynsym_section = nullptr;
  const Elf64_Shdr* string_section = nullptr;
  for (int index = 0; index < header->e_shnum; ++index) {
    if (sections[index].sh_type != SHT_DYNSYM) {
      continue;
    }
    dynsym_section = &sections[index];
    if (sections[index].sh_link < static_cast<Elf64_Word>(header->e_shnum)) {
      string_section = &sections[sections[index].sh_link];
    }
    break;
  }

  if (dynsym_section == nullptr || string_section == nullptr ||
      dynsym_section->sh_entsize < sizeof(Elf64_Sym)) {
    return {};
  }

  const std::size_t dynsym_end =
      static_cast<std::size_t>(dynsym_section->sh_offset) +
      static_cast<std::size_t>(dynsym_section->sh_size);
  const std::size_t string_end =
      static_cast<std::size_t>(string_section->sh_offset) +
      static_cast<std::size_t>(string_section->sh_size);
  if (dynsym_end > payload.size() || string_end > payload.size()) {
    return {};
  }

  const auto* symbols = reinterpret_cast<const Elf64_Sym*>(
      payload.data() + dynsym_section->sh_offset);
  const std::size_t symbol_count =
      dynsym_section->sh_size / sizeof(Elf64_Sym);
  const char* string_table = payload.data() + string_section->sh_offset;

  std::vector<std::string> symbols_out;
  for (std::size_t index = 0; index < symbol_count; ++index) {
    const auto& symbol = symbols[index];
    if (symbol.st_name == 0 || symbol.st_shndx == SHN_UNDEF) {
      continue;
    }
    const auto binding = ELF64_ST_BIND(symbol.st_info);
    const auto type = ELF64_ST_TYPE(symbol.st_info);
    if (binding != STB_GLOBAL && binding != STB_WEAK) {
      continue;
    }
    if (type != STT_FUNC && type != STT_OBJECT && type != STT_NOTYPE) {
      continue;
    }
    const auto name_offset = static_cast<std::size_t>(symbol.st_name);
    if (name_offset >= string_section->sh_size) {
      continue;
    }
    std::string name = string_table + name_offset;
    if (!name.empty()) {
      symbols_out.push_back(std::move(name));
    }
  }
  return symbols_out;
}

PostJniDispatchBoundary DiscoverPostJniDispatchBoundary(
    const std::string& library_path) {
  const auto exported_symbols = ReadElfDynamicSymbolNames(library_path);
  for (const auto& symbol_name : exported_symbols) {
    if ((symbol_name.find("register_") != std::string::npos ||
         symbol_name.find("Register") != std::string::npos) &&
        symbol_name.find("registerNativeMethods") == std::string::npos) {
      return {.state = "jni_registration_dispatch_required",
              .symbol_kind = "registration_callback",
              .symbol_name = symbol_name,
              .reason = "jni_registration_callback_symbol_detected"};
    }
  }
  for (const auto& symbol_name : exported_symbols) {
    if (symbol_name == "registerNativeMethods" ||
        symbol_name.find("registerNativeMethods") != std::string::npos) {
      return {.state = "jni_registration_dispatch_required",
              .symbol_kind = "registration_helper",
              .symbol_name = symbol_name,
              .reason = "jni_registration_helper_symbol_detected"};
    }
  }
  for (const auto& symbol_name : exported_symbols) {
    if (symbol_name.rfind("Java_", 0) == 0) {
      return {.state = "jni_direct_method_dispatch_required",
              .symbol_kind = "jni_method_export",
              .symbol_name = symbol_name,
              .reason = "jni_direct_method_export_detected"};
    }
  }
  return {};
}

int RankRegistrationCallbackSymbol(const std::string& symbol_name) {
  if (symbol_name.find("latinime") != std::string::npos) {
    return 0;
  }
  if (symbol_name.find("voiceinput") != std::string::npos) {
    return 1;
  }
  return 2;
}

std::vector<std::string> CollectRegistrationCallbackSymbols(
    const std::string& library_path) {
  auto exported_symbols = ReadElfDynamicSymbolNames(library_path);
  std::vector<std::string> callbacks;
  for (const auto& symbol_name : exported_symbols) {
    if ((symbol_name.find("register_") != std::string::npos ||
         symbol_name.find("Register") != std::string::npos) &&
        symbol_name.find("registerNativeMethods") == std::string::npos) {
      callbacks.push_back(symbol_name);
    }
  }
  std::sort(callbacks.begin(), callbacks.end(),
            [](const std::string& left, const std::string& right) {
              const int left_rank = RankRegistrationCallbackSymbol(left);
              const int right_rank = RankRegistrationCallbackSymbol(right);
              if (left_rank != right_rank) {
                return left_rank < right_rank;
              }
              return left < right;
            });
  return callbacks;
}

RegistrationDispatchOutcome AttemptRegistrationCallbackDispatch(
    const LoadedLibraryHandle& library, const std::string& symbol_name,
    std::ostringstream& output) {
  RegistrationDispatchOutcome outcome;
  outcome.dispatch_state = "symbol_resolved";
  outcome.dispatch_symbol_kind = "registration_callback";
  outcome.dispatch_symbol = symbol_name;

  dlerror();
  auto callback = reinterpret_cast<JniRegistrationCallbackFn>(
      dlsym(library.handle, symbol_name.c_str()));
  const char* symbol_error = dlerror();
  if (callback == nullptr || symbol_error != nullptr) {
    outcome.dispatch_state = "symbol_missing";
    outcome.outcome_state = "registration_callback_symbol_missing";
    outcome.outcome_reason = "registration_callback_symbol_missing";
    outcome.error_detail =
        symbol_error == nullptr ? "unknown_dlsym_error" : std::string(symbol_error);
    return outcome;
  }

  ResetStubJniEnvironmentState();
  output << "[p1] dispatching JNI registration callback: " << symbol_name
         << "\n";
  sigjmp_buf signal_environment;
  const int trapped_signal = BeginSignalTrap(&signal_environment);
  if (trapped_signal != 0) {
    const NativeSignalTrapInfo trap = GetLastSignalTrapInfo();
    EndSignalTrap();
    outcome.dispatch_state = "crashed";
    outcome.outcome_state = "registration_callback_crashed";
    outcome.outcome_reason = "registration_callback_crashed";
    outcome.error_detail = BuildSignalTrapDetail(trap);
    output << "[p1] registration callback crashed: " << symbol_name << " "
           << outcome.error_detail << "\n";
    return outcome;
  }

  callback(MakeStubJniEnv());
  EndSignalTrap();
  outcome.dispatch_state = "called";

  const auto& stub_state = GetStubJniEnvironmentState();
  if (!stub_state.native_registrations.empty()) {
    outcome.outcome_state = "register_natives_completed";
    outcome.outcome_reason = "jni_registration_callback_observed_register_natives";
    outcome.class_name = stub_state.native_registrations.front().class_name;
    for (const auto& registration : stub_state.native_registrations) {
      outcome.method_count += registration.method_count;
    }
    output << "[p1] registration callback completed: " << symbol_name
           << " class=" << outcome.class_name
           << " methods=" << outcome.method_count << "\n";
    return outcome;
  }

  if (stub_state.find_class_calls > 0) {
    outcome.outcome_state = "find_class_without_register_natives";
    outcome.outcome_reason =
        "jni_registration_callback_resolved_class_without_register_natives";
    outcome.class_name =
        stub_state.find_class_requests.empty() ? "" : stub_state.find_class_requests.front();
    output << "[p1] registration callback resolved class without RegisterNatives: "
           << symbol_name << "\n";
    return outcome;
  }

  outcome.outcome_state = "no_jni_registration_observed";
  outcome.outcome_reason = "jni_registration_callback_returned_without_jni_activity";
  output << "[p1] registration callback returned without observed JNI registration: "
         << symbol_name << "\n";
  return outcome;
}

bool RequiresAndroidCompatibilityShims(const std::string& library_path) {
  const auto needed_libraries = ReadElfNeededSharedLibraries(library_path);
  return std::any_of(
      needed_libraries.begin(), needed_libraries.end(),
      [](const std::string& library_name) {
        return library_name == "libc.so" || library_name == "liblog.so" ||
               library_name == "libm.so" || library_name == "libdl.so";
      });
}

int NormalizeElfUndefinedVersionBindings(const std::string& library_path,
                                         std::string* error_detail) {
  std::ifstream input(library_path, std::ios::binary);
  if (!input) {
    if (error_detail != nullptr) {
      *error_detail = "open_failed";
    }
    return -1;
  }

  std::vector<char> payload((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
  if (payload.size() < sizeof(Elf64_Ehdr)) {
    return 0;
  }

  auto* header = reinterpret_cast<Elf64_Ehdr*>(payload.data());
  if (!(header->e_ident[EI_MAG0] == ELFMAG0 &&
        header->e_ident[EI_MAG1] == ELFMAG1 &&
        header->e_ident[EI_MAG2] == ELFMAG2 &&
        header->e_ident[EI_MAG3] == ELFMAG3) ||
      header->e_ident[EI_CLASS] != ELFCLASS64 ||
      header->e_ident[EI_DATA] != ELFDATA2LSB ||
      header->e_shoff == 0 || header->e_shentsize < sizeof(Elf64_Shdr) ||
      header->e_shnum == 0) {
    return 0;
  }

  const std::size_t section_table_end =
      static_cast<std::size_t>(header->e_shoff) +
      static_cast<std::size_t>(header->e_shentsize) *
          static_cast<std::size_t>(header->e_shnum);
  if (section_table_end > payload.size()) {
    if (error_detail != nullptr) {
      *error_detail = "section_table_out_of_bounds";
    }
    return -1;
  }

  auto* sections =
      reinterpret_cast<Elf64_Shdr*>(payload.data() + header->e_shoff);
  const Elf64_Shdr* dynsym_section = nullptr;
  Elf64_Shdr* versym_section = nullptr;
  for (int index = 0; index < header->e_shnum; ++index) {
    if (sections[index].sh_type == SHT_DYNSYM) {
      dynsym_section = &sections[index];
    } else if (sections[index].sh_type == SHT_GNU_versym) {
      versym_section = &sections[index];
    }
  }
  if (dynsym_section == nullptr || versym_section == nullptr ||
      dynsym_section->sh_entsize < sizeof(Elf64_Sym) ||
      versym_section->sh_entsize < sizeof(Elf64_Half)) {
    return 0;
  }

  const std::size_t dynsym_end =
      static_cast<std::size_t>(dynsym_section->sh_offset) +
      static_cast<std::size_t>(dynsym_section->sh_size);
  const std::size_t versym_end =
      static_cast<std::size_t>(versym_section->sh_offset) +
      static_cast<std::size_t>(versym_section->sh_size);
  if (dynsym_end > payload.size() || versym_end > payload.size()) {
    if (error_detail != nullptr) {
      *error_detail = "dynamic_symbol_sections_out_of_bounds";
    }
    return -1;
  }

  const auto* symbols = reinterpret_cast<const Elf64_Sym*>(
      payload.data() + dynsym_section->sh_offset);
  auto* versions = reinterpret_cast<Elf64_Half*>(
      payload.data() + versym_section->sh_offset);
  const std::size_t symbol_count =
      dynsym_section->sh_size / sizeof(Elf64_Sym);
  const std::size_t version_count =
      versym_section->sh_size / sizeof(Elf64_Half);
  const std::size_t count = std::min(symbol_count, version_count);

  int rewritten = 0;
  for (std::size_t index = 0; index < count; ++index) {
    if (symbols[index].st_shndx != SHN_UNDEF) {
      continue;
    }
    const Elf64_Half current = versions[index];
    const Elf64_Half current_index =
        static_cast<Elf64_Half>(current & 0x7fffu);
    if (current_index <= 1u) {
      continue;
    }
    versions[index] = static_cast<Elf64_Half>((current & 0x8000u) | 1u);
    ++rewritten;
  }

  if (rewritten == 0) {
    return 0;
  }

  std::ofstream output(library_path, std::ios::binary | std::ios::trunc);
  if (!output) {
    if (error_detail != nullptr) {
      *error_detail = "rewrite_open_failed";
    }
    return -1;
  }
  output.write(payload.data(),
               static_cast<std::streamsize>(payload.size()));
  if (!output.good()) {
    if (error_detail != nullptr) {
      *error_detail = "rewrite_failed";
    }
    return -1;
  }

  return rewritten;
}

void PreloadAndroidCompatibilityShims(
    NativeExecuteReport& report,
    std::vector<LoadedLibraryHandle>& preloaded_libraries) {
  report.android_compat_state = "not_attempted";
  bool requires_android_compat = false;
  for (const auto& candidate_library : report.candidate_library_paths) {
    if (RequiresAndroidCompatibilityShims(candidate_library)) {
      requires_android_compat = true;
      break;
    }
  }
  if (!requires_android_compat) {
    report.android_compat_state = "not_required";
    return;
  }

  const auto shim_paths = BuildAndroidCompatibilityShimPaths();
  if (shim_paths.empty()) {
    report.android_compat_state = "executable_directory_unavailable";
    report.android_compat_diagnostics.push_back(
        "android_compat_shim_directory_unavailable");
    return;
  }

  for (const auto& candidate_library :
       report.candidate_library_paths) {
    std::string rewrite_error;
    const int rewritten =
        NormalizeElfUndefinedVersionBindings(candidate_library, &rewrite_error);
    if (rewritten < 0) {
      report.android_compat_diagnostics.push_back(
          "elf_version_normalization_failed:" + fs::path(candidate_library).filename().string() +
          ":" + rewrite_error);
      continue;
    }
    report.elf_undefined_versions_normalized += rewritten;
  }

  for (const auto& shim_path : shim_paths) {
    if (!fs::exists(shim_path)) {
      report.android_compat_diagnostics.push_back(
          "android_compat_shim_missing:" + shim_path);
      report.android_compat_state = "shim_missing";
      continue;
    }
    dlerror();
    void* handle = dlopen(shim_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    const char* error = dlerror();
    if (handle == nullptr) {
      report.android_compat_diagnostics.push_back(
          "android_compat_shim_preload_failed:" + shim_path + ":" +
          (error == nullptr ? "unknown_dlopen_error" : std::string(error)));
      report.android_compat_state = "preload_failed";
      continue;
    }
    preloaded_libraries.push_back({shim_path, handle});
    report.android_compat_preloaded_paths.push_back(shim_path);
  }

  if (!report.android_compat_preloaded_paths.empty() &&
      report.android_compat_state != "preload_failed") {
    report.android_compat_state = report.elf_undefined_versions_normalized > 0
                                      ? "preloaded_and_version_normalized"
                                      : "preloaded";
  } else if (report.android_compat_state == "not_attempted") {
    report.android_compat_state = "no_shims_preloaded";
  }
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

std::string BuildSignalTrapDetail(const NativeSignalTrapInfo& trap) {
  if (!trap.trapped) {
    return {};
  }
  std::ostringstream output;
  output << "signal_" << trap.signal_number << "_at_" << trap.address;
  return output.str();
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
  std::vector<LoadedLibraryHandle> preloaded_compatibility_libraries;
  PreloadAndroidCompatibilityShims(report, preloaded_compatibility_libraries);
  output << "[p1] android compat state: " << report.android_compat_state
         << "\n";
  if (report.elf_undefined_versions_normalized > 0) {
    output << "[p1] normalized undefined ELF version bindings: "
           << report.elf_undefined_versions_normalized << "\n";
  }
  for (const auto& diagnostic : report.android_compat_diagnostics) {
    output << "[p1] android compat diagnostic: " << diagnostic << "\n";
  }
  bool primary_library_selected = false;
  for (std::size_t candidate_index = 0;
       candidate_index < report.candidate_library_paths.size();
       ++candidate_index) {
    const auto& candidate = report.candidate_library_paths[candidate_index];
    NativeLibraryLoadAttempt attempt;
    attempt.library_path = candidate;
    attempt.library_name = fs::path(candidate).filename().string();
    attempt.candidate_index = static_cast<int>(candidate_index);
    if (primary_library_selected) {
      attempt.load_state = "skipped_after_primary_selection";
      attempt.failure_reason = "skipped_after_primary_selection";
      output << "[p1] skipping after primary selection: " << candidate << "\n";
      report.library_load_attempts.push_back(attempt);
      continue;
    }
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

    dlerror();
    const bool has_entrypoint =
        dlsym(handle, "ANativeActivity_onCreate") != nullptr &&
        dlerror() == nullptr;
    dlerror();
    const bool has_jni_onload =
        dlsym(handle, "JNI_OnLoad") != nullptr && dlerror() == nullptr;
    if (has_entrypoint || has_jni_onload) {
      primary_library_selected = true;
    }

    report.library_load_attempts.push_back(attempt);
  }

  if (report.libraries_loaded.empty()) {
    report.exit_reason = "libraries_failed_to_load";
    report.exit_code = 0;
    report.output = output.str();
    return report;
  }

  ANativeActivityCreateFn entrypoint = nullptr;
  const LoadedLibraryHandle* selected_jni_library = nullptr;
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

  for (const auto& library : loaded_libraries) {
    dlerror();
    auto* jni_onload =
        reinterpret_cast<JniOnLoadFn>(dlsym(library.handle, "JNI_OnLoad"));
    const char* symbol_error = dlerror();
    if (jni_onload == nullptr || symbol_error != nullptr) {
      continue;
    }
    if (selected_jni_library == nullptr) {
      selected_jni_library = &library;
    } else {
      const std::string candidate_name = fs::path(library.path).filename().string();
      const std::string selected_name =
          fs::path(selected_jni_library->path).filename().string();
      if (IsPreferredJniLibraryName(candidate_name) &&
          !IsPreferredJniLibraryName(selected_name)) {
        selected_jni_library = &library;
      }
    }
  }

  if (entrypoint == nullptr && selected_jni_library != nullptr) {
    report.selected_library_path = selected_jni_library->path;
    report.app_start_bridge_state = "jni_primary_library_selected";
  }

  bool any_jni_onload_success = false;
  bool jni_onload_crashed = false;
  if (selected_jni_library != nullptr) {
    for (const auto& library : loaded_libraries) {
      const std::size_t attempt_index =
          FindLibraryLoadAttemptIndex(report.library_load_attempts,
                                      library.path);
      if (attempt_index >= report.library_load_attempts.size()) {
        continue;
      }

      auto& attempt = report.library_load_attempts[attempt_index];
      if (library.path != selected_jni_library->path) {
        attempt.jni_state = "skipped_non_entrypoint";
        output << "[p1] skipping JNI_OnLoad for non-entrypoint library: "
               << library.path << "\n";
        continue;
      }

      JniOnLoadResult jni_result;
      jni_result.library_path = library.path;
      dlerror();
      auto jni_onload =
          reinterpret_cast<JniOnLoadFn>(dlsym(library.handle, "JNI_OnLoad"));
      const char* symbol_error = dlerror();
      if (jni_onload == nullptr || symbol_error != nullptr) {
        attempt.jni_state = "missing";
        attempt.failure_reason = "jni_onload_missing";
        jni_result.status = "missing";
        output << "[p1] JNI_OnLoad missing: " << library.path << "\n";
        report.jni_onload_results.push_back(jni_result);
        continue;
      }

      jni_result.symbol_present = true;
      output << "[p1] calling JNI_OnLoad: " << library.path << "\n";
      sigjmp_buf signal_environment;
      const int trapped_signal = BeginSignalTrap(&signal_environment);
      if (trapped_signal != 0) {
        const NativeSignalTrapInfo trap = GetLastSignalTrapInfo();
        attempt.jni_state = "crashed";
        attempt.failure_reason = "jni_onload_crashed";
        attempt.error_detail = BuildSignalTrapDetail(trap);
        jni_result.status = "crashed";
        jni_result.error_detail = attempt.error_detail;
        jni_onload_crashed = true;
        output << "[p1] JNI_OnLoad crashed: " << library.path << " "
               << attempt.error_detail << "\n";
        report.jni_onload_results.push_back(jni_result);
        EndSignalTrap();
        continue;
      }
      jni_result.return_code = jni_onload(MakeStubJavaVm(), nullptr);
      EndSignalTrap();
      jni_result.call_succeeded = true;
      jni_result.status = "called";
      any_jni_onload_success = true;
      attempt.jni_state = "called";
      attempt.jni_return_code = jni_result.return_code;
      output << "[p1] JNI_OnLoad OK: " << library.path
             << " returned " << jni_result.return_code << "\n";
      report.jni_onload_results.push_back(jni_result);
      break;
    }
  }

  report.execution_engine_ready =
      !report.libraries_loaded.empty() && any_jni_onload_success;
  if (jni_onload_crashed) {
    report.exit_reason = "jni_onload_missing_or_failed";
  } else if (!any_jni_onload_success && selected_jni_library != nullptr) {
    report.exit_reason = "jni_onload_missing_or_failed";
  } else if (!any_jni_onload_success) {
    report.exit_reason = "native_entry_library_not_selected";
  } else {
    report.exit_reason = "jni_onload_attempts_completed";
  }

  if (entrypoint == nullptr) {
    if (jni_onload_crashed) {
      output << "[p1] entrypoint not evaluated further because JNI_OnLoad crashed\n";
      report.post_jni_startup_state = "jni_onload_crashed";
      report.app_start_bridge_reason = "jni_onload_crashed";
    } else if (selected_jni_library != nullptr && any_jni_onload_success) {
      const auto registration_callbacks =
          CollectRegistrationCallbackSymbols(selected_jni_library->path);
      RegistrationDispatchOutcome registration_outcome;
      bool registration_callback_completed = false;
      bool registration_callback_crashed = false;
      for (const auto& callback_symbol : registration_callbacks) {
        registration_outcome = AttemptRegistrationCallbackDispatch(
            *selected_jni_library, callback_symbol, output);
        if (registration_outcome.dispatch_state == "crashed") {
          registration_callback_crashed = true;
          break;
        }
        if (registration_outcome.outcome_state == "register_natives_completed") {
          registration_callback_completed = true;
          break;
        }
      }

      if (!registration_outcome.dispatch_symbol.empty()) {
        report.registration_dispatch_state = registration_outcome.dispatch_state;
        report.registration_dispatch_symbol_kind =
            registration_outcome.dispatch_symbol_kind;
        report.registration_dispatch_symbol =
            registration_outcome.dispatch_symbol;
        report.registration_outcome_state = registration_outcome.outcome_state;
        report.registration_outcome_reason =
            registration_outcome.outcome_reason;
        report.registration_class_name = registration_outcome.class_name;
        report.registration_method_count = registration_outcome.method_count;
        report.post_jni_dispatch_symbol_kind =
            registration_outcome.dispatch_symbol_kind;
        report.post_jni_dispatch_symbol =
            registration_outcome.dispatch_symbol;
        report.post_jni_dispatch_reason =
            registration_outcome.outcome_reason;
      } else {
        report.registration_dispatch_state = "not_attempted";
        report.registration_outcome_state = "not_applicable";
        report.registration_outcome_reason = "no_registration_callback_symbols_detected";
      }

      if (registration_callback_crashed) {
        report.app_start_bridge_state =
            "linuxoid_managed_app_start_bridge_selected";
        report.app_start_bridge_reason =
            "jni_registration_callback_crashed_after_jni_onload";
        report.post_jni_startup_state = "jni_registration_callback_crashed";
        report.exit_reason = "jni_registration_callback_crashed";
      } else if (registration_callback_completed) {
        const ManagedActivityDispatchAttempt dispatch_attempt =
            BuildManagedActivityDispatchAttempt(request);
        report.app_start_bridge_state =
            "linuxoid_managed_app_start_bridge_selected";
        report.app_start_bridge_reason =
            dispatch_attempt.dispatch_state == "linuxoid_dispatch_attempted"
                ? "jni_registration_completed_and_managed_activity_dispatch_target_selected"
                : "jni_registration_completed_without_managed_activity_target";
        report.managed_activity_dispatch_state =
            dispatch_attempt.dispatch_state;
        report.managed_activity_dispatch_reason =
            dispatch_attempt.dispatch_reason;
        report.managed_activity_dispatch_component =
            dispatch_attempt.component;
        report.managed_activity_dispatch_class_name =
            dispatch_attempt.class_name;
        report.managed_activity_dispatch_class_descriptor =
            dispatch_attempt.class_descriptor;
        report.managed_activity_dispatch_method_name =
            dispatch_attempt.method_name;
        report.managed_activity_dispatch_method_signature =
            dispatch_attempt.method_signature;
        report.managed_activity_runtime_binding_state =
            dispatch_attempt.runtime_binding_state;
        if (dispatch_attempt.dispatch_state == "linuxoid_dispatch_attempted") {
          report.post_jni_startup_state = "managed_runtime_context_required";
          report.post_jni_dispatch_symbol_kind =
              "managed_activity_lifecycle_method";
          report.post_jni_dispatch_symbol =
              dispatch_attempt.class_name + "->" + dispatch_attempt.method_name +
              dispatch_attempt.method_signature;
          report.post_jni_dispatch_reason =
              "linuxoid_managed_activity_dispatch_attempted_without_runtime_context";
          report.exit_reason = "managed_runtime_context_required";
        } else {
          report.post_jni_startup_state = "managed_activity_dispatch_required";
          report.post_jni_dispatch_reason =
              "managed_activity_component_unresolved_after_jni_registration";
          report.exit_reason = "managed_activity_dispatch_required";
        }
      } else {
        const PostJniDispatchBoundary boundary =
            DiscoverPostJniDispatchBoundary(selected_jni_library->path);
        if (report.post_jni_dispatch_symbol.empty()) {
          report.post_jni_dispatch_symbol_kind = boundary.symbol_kind;
          report.post_jni_dispatch_symbol = boundary.symbol_name;
          report.post_jni_dispatch_reason = boundary.reason;
        }
        if (boundary.state == "jni_registration_dispatch_required" ||
            boundary.state == "jni_direct_method_dispatch_required") {
          report.app_start_bridge_state =
              "linuxoid_managed_app_start_bridge_selected";
          report.app_start_bridge_reason =
              "jni_onload_succeeded_without_native_activity_entrypoint";
          report.post_jni_startup_state = boundary.state;
          report.exit_reason = boundary.state;
        } else {
          report.app_start_bridge_state =
              "linuxoid_managed_app_start_bridge_required";
          report.app_start_bridge_reason =
              "jni_onload_succeeded_without_native_activity_entrypoint";
          report.post_jni_startup_state = "managed_activity_dispatch_required";
          report.exit_reason = "linuxoid_managed_app_start_bridge_required";
        }
      }
      const std::size_t attempt_index = FindLibraryLoadAttemptIndex(
          report.library_load_attempts, selected_jni_library->path);
      if (attempt_index < report.library_load_attempts.size()) {
        report.library_load_attempts[attempt_index].failure_reason =
            report.exit_reason;
        if (report.library_load_attempts[attempt_index].error_detail.empty()) {
          if (!registration_outcome.error_detail.empty()) {
            report.library_load_attempts[attempt_index].error_detail =
                registration_outcome.error_detail;
          } else {
            report.library_load_attempts[attempt_index].error_detail =
                !report.post_jni_dispatch_symbol.empty()
                    ? (report.post_jni_dispatch_symbol_kind + ":" +
                       report.post_jni_dispatch_symbol)
                    : "jni_onload_succeeded_without_native_activity_entrypoint";
          }
        }
      }
      output << "[p1] JNI-shaped primary library selected for Linuxoid-managed "
                "app-start bridge: "
             << selected_jni_library->path << "\n";
      if (!report.post_jni_dispatch_symbol.empty()) {
        output << "[p1] post-JNI dispatch symbol: "
               << report.post_jni_dispatch_symbol_kind << " "
               << report.post_jni_dispatch_symbol << "\n";
      }
      if (!report.managed_activity_dispatch_component.empty()) {
        output << "[p1] managed activity dispatch target: "
               << report.managed_activity_dispatch_component << " -> "
               << report.managed_activity_dispatch_method_name
               << report.managed_activity_dispatch_method_signature << "\n";
      }
      output << "[p1] next seam: " << report.post_jni_startup_state
             << " after JNI_OnLoad\n";
    } else {
      output << "[p1] entrypoint not found in loaded libraries\n";
      report.exit_reason = "native_activity_entrypoint_missing";
      report.post_jni_startup_state = "native_activity_entrypoint_missing";
    }
    report.exit_code = 0;
    report.output = output.str();
    return report;
  }

  report.app_start_bridge_state = "native_activity_entrypoint_selected";
  report.app_start_bridge_reason = "native_activity_entrypoint_found";
  report.post_jni_startup_state = "native_activity_dispatch_ready";

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
  if (!jni_onload_crashed) {
    CloseLoadedLibraries(loaded_libraries);
  }
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
         << "  \"android_compat_state\": \""
         << EscapeJson(report.android_compat_state) << "\",\n"
         << "  \"elf_undefined_versions_normalized\": "
         << report.elf_undefined_versions_normalized << ",\n"
         << "  \"android_compat_preloaded_paths\": "
         << BuildJsonStringArray(report.android_compat_preloaded_paths)
         << ",\n"
         << "  \"android_compat_diagnostics\": "
         << BuildJsonStringArray(report.android_compat_diagnostics) << ",\n"
         << "  \"jni_onload_results\": "
         << BuildJniOnLoadResultsJson(report.jni_onload_results) << ",\n"
         << "  \"app_start_bridge_state\": \""
         << EscapeJson(report.app_start_bridge_state) << "\",\n"
         << "  \"app_start_bridge_reason\": \""
         << EscapeJson(report.app_start_bridge_reason) << "\",\n"
         << "  \"registration_dispatch_state\": \""
         << EscapeJson(report.registration_dispatch_state) << "\",\n"
         << "  \"registration_dispatch_symbol_kind\": \""
         << EscapeJson(report.registration_dispatch_symbol_kind) << "\",\n"
         << "  \"registration_dispatch_symbol\": \""
         << EscapeJson(report.registration_dispatch_symbol) << "\",\n"
         << "  \"registration_outcome_state\": \""
         << EscapeJson(report.registration_outcome_state) << "\",\n"
         << "  \"registration_outcome_reason\": \""
         << EscapeJson(report.registration_outcome_reason) << "\",\n"
         << "  \"registration_class_name\": \""
         << EscapeJson(report.registration_class_name) << "\",\n"
         << "  \"registration_method_count\": "
         << report.registration_method_count << ",\n"
         << "  \"post_jni_startup_state\": \""
         << EscapeJson(report.post_jni_startup_state) << "\",\n"
         << "  \"post_jni_dispatch_symbol_kind\": \""
         << EscapeJson(report.post_jni_dispatch_symbol_kind) << "\",\n"
         << "  \"post_jni_dispatch_symbol\": \""
         << EscapeJson(report.post_jni_dispatch_symbol) << "\",\n"
         << "  \"post_jni_dispatch_reason\": \""
         << EscapeJson(report.post_jni_dispatch_reason) << "\",\n"
         << "  \"managed_activity_dispatch_state\": \""
         << EscapeJson(report.managed_activity_dispatch_state) << "\",\n"
         << "  \"managed_activity_dispatch_reason\": \""
         << EscapeJson(report.managed_activity_dispatch_reason) << "\",\n"
         << "  \"managed_activity_dispatch_component\": \""
         << EscapeJson(report.managed_activity_dispatch_component) << "\",\n"
         << "  \"managed_activity_dispatch_class_name\": \""
         << EscapeJson(report.managed_activity_dispatch_class_name) << "\",\n"
         << "  \"managed_activity_dispatch_class_descriptor\": \""
         << EscapeJson(report.managed_activity_dispatch_class_descriptor)
         << "\",\n"
         << "  \"managed_activity_dispatch_method_name\": \""
         << EscapeJson(report.managed_activity_dispatch_method_name)
         << "\",\n"
         << "  \"managed_activity_dispatch_method_signature\": \""
         << EscapeJson(report.managed_activity_dispatch_method_signature)
         << "\",\n"
         << "  \"managed_activity_runtime_binding_state\": \""
         << EscapeJson(report.managed_activity_runtime_binding_state)
         << "\",\n"
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
