#include "wfa/binder_service_manager.hpp"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace wfa {

namespace fs = std::filesystem;

namespace {

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

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

std::string ResolveSessionId(const BinderServiceManagerSpec& spec) {
  if (!spec.session_id.empty()) {
    return spec.session_id;
  }
  if (!spec.package_name.empty()) {
    return spec.package_name + ":bootstrap";
  }
  return "linuxoid-local-bootstrap";
}

std::string ResolveOwnerProcessIdentity(
    const BinderServiceManagerSpec& spec,
    const std::string& session_id) {
  if (!spec.owner_process_identity.empty()) {
    return spec.owner_process_identity;
  }
  return "linuxoid-native-session:" + session_id;
}

std::string RenderStringJsonArray(const std::vector<std::string>& values) {
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

std::string RenderServiceRegistrationJson(
    const BinderServiceRegistration& registration) {
  std::ostringstream output;
  output << "{"
         << "\"service_name\": \"" << EscapeJson(registration.service_name)
         << "\", "
         << "\"interface_name\": \""
         << EscapeJson(registration.interface_name) << "\", "
         << "\"descriptor_name\": \""
         << EscapeJson(registration.descriptor_name) << "\", "
         << "\"transport_kind\": \""
         << EscapeJson(registration.transport_kind) << "\", "
         << "\"status\": \"" << EscapeJson(registration.status) << "\", "
         << "\"handle_id\": " << registration.handle_id << ", "
         << "\"registration_order\": " << registration.registration_order
         << ", "
         << "\"owner_session_id\": \""
         << EscapeJson(registration.owner_session_id) << "\", "
         << "\"owner_process_identity\": \""
         << EscapeJson(registration.owner_process_identity) << "\", "
         << "\"app_local_placeholder\": "
         << (registration.app_local_placeholder ? "true" : "false") << ", "
         << "\"notes\": \"" << EscapeJson(registration.notes) << "\""
         << "}";
  return output.str();
}

std::string RenderServiceLookupJson(const BinderServiceLookup& lookup) {
  std::ostringstream output;
  output << "{"
         << "\"service_name\": \"" << EscapeJson(lookup.service_name)
         << "\", "
         << "\"interface_name\": \""
         << EscapeJson(lookup.interface_name) << "\", "
         << "\"descriptor_name\": \""
         << EscapeJson(lookup.descriptor_name) << "\", "
         << "\"caller_identity\": \"" << EscapeJson(lookup.caller_identity)
         << "\", "
         << "\"owner_session_id\": \""
         << EscapeJson(lookup.owner_session_id) << "\", "
         << "\"owner_process_identity\": \""
         << EscapeJson(lookup.owner_process_identity) << "\", "
         << "\"result\": \"" << EscapeJson(lookup.result) << "\", "
         << "\"lookup_status\": \"" << EscapeJson(lookup.result) << "\", "
         << "\"handle_id\": " << lookup.handle_id << ", "
         << "\"request_status\": \"" << EscapeJson(lookup.request_status)
         << "\", "
         << "\"response_status\": \"" << EscapeJson(lookup.response_status)
         << "\""
         << "}";
  return output.str();
}

std::string RenderTransactionJson(const BinderTransactionMetadata& transaction) {
  std::ostringstream output;
  output << "{"
         << "\"service_name\": \"" << EscapeJson(transaction.service_name)
         << "\", "
         << "\"interface_name\": \""
         << EscapeJson(transaction.interface_name) << "\", "
         << "\"descriptor_name\": \""
         << EscapeJson(transaction.descriptor_name) << "\", "
         << "\"handle_id\": " << transaction.handle_id << ", "
         << "\"transaction_name\": \""
         << EscapeJson(transaction.transaction_name) << "\", "
         << "\"transaction_code\": " << transaction.transaction_code << ", "
         << "\"caller_identity\": \""
         << EscapeJson(transaction.caller_identity) << "\", "
         << "\"owner_session_id\": \""
         << EscapeJson(transaction.owner_session_id) << "\", "
         << "\"owner_process_identity\": \""
         << EscapeJson(transaction.owner_process_identity) << "\", "
         << "\"request_summary\": \""
         << EscapeJson(transaction.request_summary) << "\", "
         << "\"response_summary\": \""
         << EscapeJson(transaction.response_summary) << "\", "
         << "\"request_status\": \""
         << EscapeJson(transaction.request_status) << "\", "
         << "\"response_status\": \""
         << EscapeJson(transaction.response_status) << "\", "
         << "\"status\": \"" << EscapeJson(transaction.status) << "\""
         << "}";
  return output.str();
}

void WriteJsonlFile(const fs::path& path,
                    const std::vector<std::string>& rendered_lines) {
  std::ostringstream output;
  for (const auto& line : rendered_lines) {
    output << line << "\n";
  }
  WriteTextFile(path, output.str());
}

struct BinderTransportMessage {
  std::string message_kind;
  std::string service_name;
  std::string interface_name;
  std::string descriptor_name;
  std::string caller_identity;
  std::string owner_session_id;
  std::string owner_process_identity;
  int handle_id = 0;
  std::string delivery_status;
  std::string payload_summary;
};

std::vector<BinderServiceRegistration> BuildDefaultRegistrations(
    const BinderServiceManagerSpec& spec,
    const std::string& session_id,
    const std::string& owner_process_identity) {
  const std::string package_name =
      spec.package_name.empty() ? "linuxoid.local" : spec.package_name;
  const std::string app_local_descriptor =
      package_name + ".ILinuxoidSessionService";
  return {
      {"service_manager", "android.os.IServiceManager",
       "android.os.IServiceManager", "in_process_binder_shape", "ready", 1, 1,
       session_id, owner_process_identity, false,
       "Owns local Binder-shaped service registration and lookup during Linuxoid bootstrap."},
      {"package_manager", "android.content.pm.IPackageManager",
       "android.content.pm.IPackageManager", "in_process_binder_shape", "ready",
       2, 2, session_id, owner_process_identity, false,
       "Returns staged package identity, APK path, and launcher metadata from Linuxoid-owned bundle roots."},
      {"activity_manager", "android.app.IActivityManager",
       "android.app.IActivityManager", "in_process_binder_shape", "ready", 3,
       3, session_id, owner_process_identity, false,
       "Queues native activity launch metadata and lifecycle state for Linuxoid-owned process bootstrap."},
      {"app_local_service", app_local_descriptor, app_local_descriptor,
       "in_process_binder_shape", "ready", 4, 4, session_id,
       owner_process_identity, true,
       "Provides a Linuxoid-owned app-local placeholder service so future Self-Healing Android Device work can bind deterministic session-local capabilities without claiming real Android Binder or system_server support."},
  };
}

std::vector<BinderServiceLookup> BuildDefaultLookups(
    const BinderServiceManagerSpec& spec,
    const std::string& session_id,
    const std::string& owner_process_identity) {
  const std::string package_name =
      spec.package_name.empty() ? "linuxoid.local" : spec.package_name;
  const std::string app_local_descriptor =
      package_name + ".ILinuxoidSessionService";
  return {
      {"package_manager", "android.content.pm.IPackageManager",
       "android.content.pm.IPackageManager", "linuxoid-native-runner",
       session_id, owner_process_identity, "found", 2, "completed", "found"},
      {"activity_manager", "android.app.IActivityManager",
       "android.app.IActivityManager", "linuxoid-native-runner", session_id,
       owner_process_identity, "found", 3, "completed", "found"},
      {"app_local_service", app_local_descriptor, app_local_descriptor,
       "linuxoid-native-runner", session_id, owner_process_identity, "found", 4,
       "completed", "found"},
      {"window_manager", "android.view.IWindowManager",
       "android.view.IWindowManager", "linuxoid-native-runner", "", "",
       "missing", 0, "completed", "missing"},
  };
}

std::vector<BinderTransactionMetadata> BuildDefaultTransactions(
    const BinderServiceManagerSpec& spec,
    const std::string& session_id,
    const std::string& owner_process_identity) {
  const std::string package_name =
      spec.package_name.empty() ? "linuxoid.local" : spec.package_name;
  const std::string app_local_descriptor =
      package_name + ".ILinuxoidSessionService";
  return {
      {"package_manager", "android.content.pm.IPackageManager",
       "android.content.pm.IPackageManager", 2, "getPackageInfo", 1001,
       "linuxoid-native-runner", session_id, owner_process_identity,
       "package=" + spec.package_name + "; apk_path=" + spec.apk_path,
       "launcher_component=" + spec.launcher_component + "; status=ready",
       "accepted", "ready", "ok"},
      {"activity_manager", "android.app.IActivityManager",
       "android.app.IActivityManager", 3, "scheduleLaunchActivity", 2001,
       "linuxoid-native-runner", session_id, owner_process_identity,
       "component=" + spec.launcher_component + "; package=" + spec.package_name,
       "activity_state=NOT_CREATED; process_state=BOOTSTRAPPED", "accepted",
       "queued", "queued"},
      {"app_local_service", app_local_descriptor, app_local_descriptor, 4,
       "getSessionSummary", 3001, "linuxoid-native-runner", session_id,
       owner_process_identity, "session_id=" + session_id,
       "owner_process=" + owner_process_identity + "; status=ready",
       "accepted", "ready", "ok"},
  };
}

std::string RenderServiceRegistryJson(
    const std::vector<BinderServiceRegistration>& services) {
  std::ostringstream output;
  output << "[\n";
  for (std::size_t index = 0; index < services.size(); ++index) {
    if (index != 0) {
      output << ",\n";
    }
    output << "  " << RenderServiceRegistrationJson(services[index]);
  }
  output << "\n]\n";
  return output.str();
}

std::string RenderLookupSummaryJson(
    const std::vector<BinderServiceLookup>& lookups) {
  std::ostringstream output;
  output << "[\n";
  for (std::size_t index = 0; index < lookups.size(); ++index) {
    if (index != 0) {
      output << ",\n";
    }
    output << "  " << RenderServiceLookupJson(lookups[index]);
  }
  output << "\n]\n";
  return output.str();
}

std::string RenderRegistrationArrayJson(
    const std::vector<BinderServiceRegistration>& services) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < services.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    output << RenderServiceRegistrationJson(services[index]);
  }
  output << "]";
  return output.str();
}

std::string RenderLookupArrayJson(
    const std::vector<BinderServiceLookup>& lookups) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < lookups.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    output << RenderServiceLookupJson(lookups[index]);
  }
  output << "]";
  return output.str();
}

std::string RenderTransactionArrayJson(
    const std::vector<BinderTransactionMetadata>& transactions) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < transactions.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    output << RenderTransactionJson(transactions[index]);
  }
  output << "]";
  return output.str();
}

std::string RenderTransportMessageJson(const BinderTransportMessage& message) {
  std::ostringstream output;
  output << "{"
         << "\"message_kind\": \"" << EscapeJson(message.message_kind)
         << "\", "
         << "\"service_name\": \"" << EscapeJson(message.service_name)
         << "\", "
         << "\"interface_name\": \"" << EscapeJson(message.interface_name)
         << "\", "
         << "\"descriptor_name\": \"" << EscapeJson(message.descriptor_name)
         << "\", "
         << "\"caller_identity\": \"" << EscapeJson(message.caller_identity)
         << "\", "
         << "\"owner_session_id\": \"" << EscapeJson(message.owner_session_id)
         << "\", "
         << "\"owner_process_identity\": \""
         << EscapeJson(message.owner_process_identity) << "\", "
         << "\"handle_id\": " << message.handle_id << ", "
         << "\"delivery_status\": \""
         << EscapeJson(message.delivery_status) << "\", "
         << "\"payload_summary\": \"" << EscapeJson(message.payload_summary)
         << "\""
         << "}";
  return output.str();
}

void WriteTransportMessageLog(
    const fs::path& path,
    const std::vector<BinderTransportMessage>& messages) {
  std::vector<std::string> rendered_lines;
  rendered_lines.reserve(messages.size());
  for (const auto& message : messages) {
    rendered_lines.push_back(RenderTransportMessageJson(message));
  }
  WriteJsonlFile(path, rendered_lines);
}

[[noreturn]] void ThrowTransportError(const std::string& prefix,
                                      const std::string& detail) {
  if (detail.empty()) {
    throw std::runtime_error(prefix);
  }
  throw std::runtime_error(prefix + ":" + detail);
}

void WriteTransportBytes(int fd, const char* data, std::size_t size,
                         const std::string& prefix) {
  std::size_t total_written = 0;
  while (total_written < size) {
    const ssize_t written = ::write(fd, data + total_written, size - total_written);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      ThrowTransportError(prefix, std::strerror(errno));
    }
    if (written == 0) {
      ThrowTransportError(prefix, "short_write_zero");
    }
    total_written += static_cast<std::size_t>(written);
  }
}

void ReadTransportBytes(int fd, char* data, std::size_t size,
                        const std::string& prefix) {
  std::size_t total_read = 0;
  while (total_read < size) {
    const ssize_t received = ::read(fd, data + total_read, size - total_read);
    if (received < 0) {
      if (errno == EINTR) {
        continue;
      }
      ThrowTransportError(prefix, std::strerror(errno));
    }
    if (received == 0) {
      ThrowTransportError(prefix, "peer_closed");
    }
    total_read += static_cast<std::size_t>(received);
  }
}

std::array<char, 4> EncodeTransportPayloadSize(std::size_t size) {
  if (size > 0xffffffffu) {
    throw std::runtime_error("binder transport payload too large");
  }
  const std::uint32_t value = static_cast<std::uint32_t>(size);
  return {
      static_cast<char>(value & 0xffu),
      static_cast<char>((value >> 8u) & 0xffu),
      static_cast<char>((value >> 16u) & 0xffu),
      static_cast<char>((value >> 24u) & 0xffu),
  };
}

std::uint32_t DecodeTransportPayloadSize(const std::array<char, 4>& bytes) {
  return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[0])) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[1]))
          << 8u) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[2]))
          << 16u) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3]))
          << 24u);
}

void SendBinderTransportPayload(int fd, const std::string& payload) {
  const auto header = EncodeTransportPayloadSize(payload.size());
  WriteTransportBytes(fd, header.data(), header.size(),
                      "unable to write binder transport payload");
  if (!payload.empty()) {
    WriteTransportBytes(fd, payload.data(), payload.size(),
                        "unable to write binder transport payload");
  }
}

std::string ReceiveBinderTransportPayload(int fd) {
  std::array<char, 4> header{};
  ReadTransportBytes(fd, header.data(), header.size(),
                     "unable to read binder transport payload");
  const std::uint32_t payload_size = DecodeTransportPayloadSize(header);
  if (payload_size > 1024u * 1024u) {
    throw std::runtime_error("binder transport payload too large");
  }
  std::string payload(payload_size, '\0');
  if (payload_size != 0) {
    ReadTransportBytes(fd, payload.data(), payload.size(),
                       "unable to read binder transport payload");
  }
  return payload;
}

void SimulateTransportRoundTrip(const BinderTransportMessage& request,
                                const BinderTransportMessage& response,
                                std::vector<BinderTransportMessage>& messages) {
  int sockets[2] = {-1, -1};
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
    throw std::runtime_error("unable to create unix socketpair transport");
  }

  const std::string request_json = RenderTransportMessageJson(request);
  const std::string response_json = RenderTransportMessageJson(response);

  try {
    SendBinderTransportPayload(sockets[0], request_json);
    const std::string server_seen = ReceiveBinderTransportPayload(sockets[1]);
    messages.push_back(request);

    SendBinderTransportPayload(sockets[1], response_json);
    const std::string client_seen = ReceiveBinderTransportPayload(sockets[0]);
    messages.push_back(response);

    if (server_seen != request_json || client_seen != response_json) {
      throw std::runtime_error("binder transport payload mismatch");
    }
  } catch (...) {
    close(sockets[0]);
    close(sockets[1]);
    throw;
  }

  close(sockets[0]);
  close(sockets[1]);
}

}  // namespace

BinderServiceManagerFixtureReport RunBinderServiceManagerFixture(
    const BinderServiceManagerSpec& spec) {
  BinderServiceManagerFixtureReport report;
  report.package_name = spec.package_name;
  report.launcher_component = spec.launcher_component;
  report.session_id = ResolveSessionId(spec);
  report.owner_process_identity =
      ResolveOwnerProcessIdentity(spec, report.session_id);
  report.artifact_root = spec.artifact_root;
  report.transport_kind = "unix_socketpair_binder_shape";
  report.transport_log_path =
      (fs::path(spec.artifact_root) / "binder" / "transport-messages.jsonl")
          .string();
  report.metadata_path =
      (fs::path(spec.artifact_root) / "binder" / "service-manager.json").string();
  report.registry_path =
      (fs::path(spec.artifact_root) / "binder" / "registered-services.json")
          .string();
  report.lookup_summary_path =
      (fs::path(spec.artifact_root) / "binder" / "service-lookups.json")
          .string();
  report.lookup_log_path =
      (fs::path(spec.artifact_root) / "binder" / "service-lookups.jsonl")
          .string();
  report.transaction_log_path =
      (fs::path(spec.artifact_root) / "binder" / "service-transactions.jsonl")
          .string();
  report.limitation_flags = {
      "linuxoid_local_foundation_only",
      "no_kernel_binder_driver",
      "no_real_system_server",
      "parcel_semantics_metadata_only",
  };

  if (spec.package_name.empty() || spec.launcher_component.empty() ||
      spec.apk_path.empty() || spec.artifact_root.empty()) {
    report.exit_reason = "invalid_service_manager_spec";
    fs::create_directories(fs::path(report.metadata_path).parent_path());
    WriteTextFile(report.metadata_path,
                  RenderBinderServiceManagerFixtureJson(report));
    WriteTextFile(report.registry_path, "[]\n");
    WriteTextFile(report.lookup_summary_path, "[]\n");
    WriteTextFile(report.lookup_log_path, "");
    WriteTextFile(report.transaction_log_path, "");
    WriteTextFile(report.transport_log_path, "");
    return report;
  }

  fs::create_directories(fs::path(report.metadata_path).parent_path());

  report.services = BuildDefaultRegistrations(
      spec, report.session_id, report.owner_process_identity);
  report.lookups = BuildDefaultLookups(
      spec, report.session_id, report.owner_process_identity);
  report.transactions = BuildDefaultTransactions(
      spec, report.session_id, report.owner_process_identity);
  report.manager_ready = true;
  report.exit_reason = "binder_service_manager_ready";

  WriteTextFile(report.registry_path, RenderServiceRegistryJson(report.services));
  WriteTextFile(report.lookup_summary_path,
                RenderLookupSummaryJson(report.lookups));

  std::vector<std::string> lookup_lines;
  lookup_lines.reserve(report.lookups.size());
  for (const auto& lookup : report.lookups) {
    lookup_lines.push_back(RenderServiceLookupJson(lookup));
  }
  WriteJsonlFile(report.lookup_log_path, lookup_lines);

  std::vector<std::string> transaction_lines;
  transaction_lines.reserve(report.transactions.size());
  for (const auto& transaction : report.transactions) {
    transaction_lines.push_back(RenderTransactionJson(transaction));
  }
  WriteJsonlFile(report.transaction_log_path, transaction_lines);

  std::vector<BinderTransportMessage> transport_messages;
  transport_messages.reserve((report.lookups.size() + report.transactions.size()) *
                             2);

  for (const auto& lookup : report.lookups) {
    SimulateTransportRoundTrip(
        {.message_kind = "lookup_request",
         .service_name = lookup.service_name,
         .interface_name = "android.os.IServiceManager",
         .descriptor_name = "android.os.IServiceManager",
         .caller_identity = lookup.caller_identity,
         .owner_session_id = lookup.owner_session_id,
         .owner_process_identity = lookup.owner_process_identity,
         .handle_id = 1,
         .delivery_status = "sent",
         .payload_summary = "service=" + lookup.service_name},
        {.message_kind = "lookup_response",
         .service_name = lookup.service_name,
         .interface_name = lookup.interface_name,
         .descriptor_name = lookup.descriptor_name,
         .caller_identity = lookup.caller_identity,
         .owner_session_id = lookup.owner_session_id,
         .owner_process_identity = lookup.owner_process_identity,
         .handle_id = lookup.handle_id,
         .delivery_status = lookup.response_status,
         .payload_summary = "result=" + lookup.result},
        transport_messages);
  }

  for (const auto& transaction : report.transactions) {
    SimulateTransportRoundTrip(
        {.message_kind = "transaction_request",
         .service_name = transaction.service_name,
         .interface_name = transaction.interface_name,
         .descriptor_name = transaction.descriptor_name,
         .caller_identity = transaction.caller_identity,
         .owner_session_id = transaction.owner_session_id,
         .owner_process_identity = transaction.owner_process_identity,
         .handle_id = transaction.handle_id,
         .delivery_status = transaction.request_status,
         .payload_summary = transaction.request_summary},
        {.message_kind = "transaction_response",
         .service_name = transaction.service_name,
         .interface_name = transaction.interface_name,
         .descriptor_name = transaction.descriptor_name,
         .caller_identity = transaction.caller_identity,
         .owner_session_id = transaction.owner_session_id,
         .owner_process_identity = transaction.owner_process_identity,
         .handle_id = transaction.handle_id,
         .delivery_status = transaction.response_status,
         .payload_summary = transaction.response_summary},
        transport_messages);
  }
  report.transport_round_trips =
      static_cast<int>(report.lookups.size() + report.transactions.size());
  WriteTransportMessageLog(report.transport_log_path, transport_messages);

  WriteTextFile(report.metadata_path,
                RenderBinderServiceManagerFixtureJson(report));
  return report;
}

std::string RenderBinderServiceManagerFixtureJson(
    const BinderServiceManagerFixtureReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"manager_ready\": "
         << (report.manager_ready ? "true" : "false") << ",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"launcher_component\": \""
         << EscapeJson(report.launcher_component) << "\",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id)
         << "\",\n"
         << "  \"owner_process_identity\": \""
         << EscapeJson(report.owner_process_identity) << "\",\n"
         << "  \"transport_kind\": \"" << EscapeJson(report.transport_kind)
         << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"transport_log_path\": \""
         << EscapeJson(report.transport_log_path) << "\",\n"
         << "  \"metadata_path\": \"" << EscapeJson(report.metadata_path)
         << "\",\n"
         << "  \"registry_path\": \"" << EscapeJson(report.registry_path)
         << "\",\n"
         << "  \"lookup_summary_path\": \""
         << EscapeJson(report.lookup_summary_path) << "\",\n"
         << "  \"lookup_log_path\": \"" << EscapeJson(report.lookup_log_path)
         << "\",\n"
         << "  \"transaction_log_path\": \""
         << EscapeJson(report.transaction_log_path) << "\",\n"
         << "  \"registered_services\": " << report.services.size() << ",\n"
         << "  \"lookups_recorded\": " << report.lookups.size() << ",\n"
         << "  \"transactions_recorded\": " << report.transactions.size()
         << ",\n"
         << "  \"transport_round_trips\": " << report.transport_round_trips
         << ",\n"
         << "  \"local_foundation_only\": "
         << (report.local_foundation_only ? "true" : "false") << ",\n"
         << "  \"real_android_binder\": "
         << (report.real_android_binder ? "true" : "false") << ",\n"
         << "  \"system_server_present\": "
         << (report.system_server_present ? "true" : "false") << ",\n"
         << "  \"parcel_support_level\": \""
         << EscapeJson(report.parcel_support_level) << "\",\n"
         << "  \"limitation_flags\": "
         << RenderStringJsonArray(report.limitation_flags) << ",\n"
         << "  \"services\": " << RenderRegistrationArrayJson(report.services)
         << ",\n"
         << "  \"lookups\": " << RenderLookupArrayJson(report.lookups)
         << ",\n"
         << "  \"transactions\": "
         << RenderTransactionArrayJson(report.transactions) << ",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\"\n"
         << "}\n";
  return output.str();
}

}  // namespace wfa
