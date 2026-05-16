#include "wfa/binder_service_manager.hpp"

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

std::string RenderServiceRegistrationJson(
    const BinderServiceRegistration& registration) {
  std::ostringstream output;
  output << "{"
         << "\"service_name\": \"" << EscapeJson(registration.service_name)
         << "\", "
         << "\"interface_name\": \""
         << EscapeJson(registration.interface_name) << "\", "
         << "\"transport_kind\": \""
         << EscapeJson(registration.transport_kind) << "\", "
         << "\"status\": \"" << EscapeJson(registration.status) << "\", "
         << "\"handle_id\": " << registration.handle_id << ", "
         << "\"registration_order\": " << registration.registration_order
         << ", "
         << "\"notes\": \"" << EscapeJson(registration.notes) << "\""
         << "}";
  return output.str();
}

std::string RenderServiceLookupJson(const BinderServiceLookup& lookup) {
  std::ostringstream output;
  output << "{"
         << "\"service_name\": \"" << EscapeJson(lookup.service_name)
         << "\", "
         << "\"caller_identity\": \"" << EscapeJson(lookup.caller_identity)
         << "\", "
         << "\"result\": \"" << EscapeJson(lookup.result) << "\", "
         << "\"handle_id\": " << lookup.handle_id
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
         << "\"transaction_name\": \""
         << EscapeJson(transaction.transaction_name) << "\", "
         << "\"transaction_code\": " << transaction.transaction_code << ", "
         << "\"caller_identity\": \""
         << EscapeJson(transaction.caller_identity) << "\", "
         << "\"request_summary\": \""
         << EscapeJson(transaction.request_summary) << "\", "
         << "\"response_summary\": \""
         << EscapeJson(transaction.response_summary) << "\", "
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

std::vector<BinderServiceRegistration> BuildDefaultRegistrations() {
  return {
      {"service_manager", "android.os.IServiceManager",
       "in_process_binder_shape", "ready", 1, 1,
       "Owns local Binder-shaped service registration and lookup during Linuxoid bootstrap."},
      {"package_manager", "android.content.pm.IPackageManager",
       "in_process_binder_shape", "ready", 2, 2,
       "Returns staged package identity, APK path, and launcher metadata from Linuxoid-owned bundle roots."},
      {"activity_manager", "android.app.IActivityManager",
       "in_process_binder_shape", "ready", 3, 3,
       "Queues native activity launch metadata and lifecycle state for Linuxoid-owned process bootstrap."},
  };
}

std::vector<BinderServiceLookup> BuildDefaultLookups() {
  return {
      {"package_manager", "linuxoid-native-runner", "found", 2},
      {"activity_manager", "linuxoid-native-runner", "found", 3},
  };
}

std::vector<BinderTransactionMetadata> BuildDefaultTransactions(
    const BinderServiceManagerSpec& spec) {
  return {
      {"package_manager", "android.content.pm.IPackageManager",
       "getPackageInfo", 1001, "linuxoid-native-runner",
       "package=" + spec.package_name + "; apk_path=" + spec.apk_path,
       "launcher_component=" + spec.launcher_component + "; status=ready",
       "ok"},
      {"activity_manager", "android.app.IActivityManager",
       "scheduleLaunchActivity", 2001, "linuxoid-native-runner",
       "component=" + spec.launcher_component + "; package=" + spec.package_name,
       "activity_state=NOT_CREATED; process_state=BOOTSTRAPPED", "queued"},
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

}  // namespace

BinderServiceManagerFixtureReport RunBinderServiceManagerFixture(
    const BinderServiceManagerSpec& spec) {
  BinderServiceManagerFixtureReport report;
  report.package_name = spec.package_name;
  report.launcher_component = spec.launcher_component;
  report.artifact_root = spec.artifact_root;
  report.transport_kind = "in_process_binder_shape";
  report.metadata_path =
      (fs::path(spec.artifact_root) / "binder" / "service-manager.json").string();
  report.registry_path =
      (fs::path(spec.artifact_root) / "binder" / "registered-services.json")
          .string();
  report.lookup_log_path =
      (fs::path(spec.artifact_root) / "binder" / "service-lookups.jsonl")
          .string();
  report.transaction_log_path =
      (fs::path(spec.artifact_root) / "binder" / "service-transactions.jsonl")
          .string();

  if (spec.package_name.empty() || spec.launcher_component.empty() ||
      spec.apk_path.empty() || spec.artifact_root.empty()) {
    report.exit_reason = "invalid_service_manager_spec";
    fs::create_directories(fs::path(report.metadata_path).parent_path());
    WriteTextFile(report.metadata_path,
                  RenderBinderServiceManagerFixtureJson(report));
    WriteTextFile(report.registry_path, "[]\n");
    WriteTextFile(report.lookup_log_path, "");
    WriteTextFile(report.transaction_log_path, "");
    return report;
  }

  fs::create_directories(fs::path(report.metadata_path).parent_path());

  report.services = BuildDefaultRegistrations();
  report.lookups = BuildDefaultLookups();
  report.transactions = BuildDefaultTransactions(spec);
  report.manager_ready = true;
  report.exit_reason = "binder_service_manager_ready";

  WriteTextFile(report.registry_path, RenderServiceRegistryJson(report.services));

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
         << "  \"transport_kind\": \"" << EscapeJson(report.transport_kind)
         << "\",\n"
         << "  \"artifact_root\": \"" << EscapeJson(report.artifact_root)
         << "\",\n"
         << "  \"metadata_path\": \"" << EscapeJson(report.metadata_path)
         << "\",\n"
         << "  \"registry_path\": \"" << EscapeJson(report.registry_path)
         << "\",\n"
         << "  \"lookup_log_path\": \"" << EscapeJson(report.lookup_log_path)
         << "\",\n"
         << "  \"transaction_log_path\": \""
         << EscapeJson(report.transaction_log_path) << "\",\n"
         << "  \"registered_services\": " << report.services.size() << ",\n"
         << "  \"lookups_recorded\": " << report.lookups.size() << ",\n"
         << "  \"transactions_recorded\": " << report.transactions.size()
         << ",\n"
         << "  \"exit_reason\": \"" << EscapeJson(report.exit_reason)
         << "\"\n"
         << "}\n";
  return output.str();
}

}  // namespace wfa
