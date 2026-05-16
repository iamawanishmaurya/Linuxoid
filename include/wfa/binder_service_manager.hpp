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
};

struct BinderServiceRegistration {
  std::string service_name;
  std::string interface_name;
  std::string transport_kind;
  std::string status;
  int handle_id = 0;
  int registration_order = 0;
  std::string notes;
};

struct BinderServiceLookup {
  std::string service_name;
  std::string caller_identity;
  std::string result;
  int handle_id = 0;
};

struct BinderTransactionMetadata {
  std::string service_name;
  std::string interface_name;
  std::string transaction_name;
  int transaction_code = 0;
  std::string caller_identity;
  std::string request_summary;
  std::string response_summary;
  std::string status;
};

struct BinderServiceManagerFixtureReport {
  bool manager_ready = false;
  std::string package_name;
  std::string launcher_component;
  std::string artifact_root;
  std::string transport_kind;
  std::string transport_log_path;
  std::string metadata_path;
  std::string registry_path;
  std::string lookup_log_path;
  std::string transaction_log_path;
  std::vector<BinderServiceRegistration> services;
  std::vector<BinderServiceLookup> lookups;
  std::vector<BinderTransactionMetadata> transactions;
  int transport_round_trips = 0;
  std::string exit_reason;
};

BinderServiceManagerFixtureReport RunBinderServiceManagerFixture(
    const BinderServiceManagerSpec& spec);
std::string RenderBinderServiceManagerFixtureJson(
    const BinderServiceManagerFixtureReport& report);

}  // namespace wfa

#endif  // WFA_BINDER_SERVICE_MANAGER_HPP
