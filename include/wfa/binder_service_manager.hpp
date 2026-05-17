#ifndef WFA_BINDER_SERVICE_MANAGER_HPP
#define WFA_BINDER_SERVICE_MANAGER_HPP

#include <string>
#include <vector>

namespace wfa {

struct BinderServiceManagerSpec {
  std::string package_name;
  std::string launcher_component;
  std::string apk_path;
  std::string artifact_root;
  std::string session_id;
  std::string owner_process_identity;
};

struct BinderServiceRegistration {
  std::string service_name;
  std::string interface_name;
  std::string descriptor_name;
  std::string transport_kind;
  std::string status;
  int handle_id = 0;
  int registration_order = 0;
  std::string owner_session_id;
  std::string owner_process_identity;
  bool app_local_placeholder = false;
  std::string notes;
};

struct BinderServiceLookup {
  std::string service_name;
  std::string interface_name;
  std::string descriptor_name;
  std::string caller_identity;
  std::string owner_session_id;
  std::string owner_process_identity;
  std::string result;
  int handle_id = 0;
  std::string request_status;
  std::string response_status;
};

struct BinderTransactionMetadata {
  std::string service_name;
  std::string interface_name;
  std::string descriptor_name;
  int handle_id = 0;
  std::string transaction_name;
  int transaction_code = 0;
  std::string caller_identity;
  std::string owner_session_id;
  std::string owner_process_identity;
  std::string request_summary;
  std::string response_summary;
  std::string request_status;
  std::string response_status;
  std::string status;
};

struct BinderServiceManagerFixtureReport {
  bool manager_ready = false;
  std::string package_name;
  std::string launcher_component;
  std::string session_id;
  std::string owner_process_identity;
  std::string artifact_root;
  std::string transport_kind;
  std::string transport_log_path;
  std::string metadata_path;
  std::string registry_path;
  std::string lookup_summary_path;
  std::string lookup_log_path;
  std::string transaction_log_path;
  std::vector<BinderServiceRegistration> services;
  std::vector<BinderServiceLookup> lookups;
  std::vector<BinderTransactionMetadata> transactions;
  int transport_round_trips = 0;
  bool local_foundation_only = true;
  bool real_android_binder = false;
  bool system_server_present = false;
  std::string parcel_support_level = "metadata_only";
  std::vector<std::string> limitation_flags;
  std::string exit_reason;
};

BinderServiceManagerFixtureReport RunBinderServiceManagerFixture(
    const BinderServiceManagerSpec& spec);
std::string RenderBinderServiceManagerFixtureJson(
    const BinderServiceManagerFixtureReport& report);

}  // namespace wfa

#endif  // WFA_BINDER_SERVICE_MANAGER_HPP
