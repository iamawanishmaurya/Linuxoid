#ifndef WFA_APK_SELF_HEALING_WATCHDOG_HPP
#define WFA_APK_SELF_HEALING_WATCHDOG_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace wfa {

struct NativeApkLaunchReport;

struct SelfHealingAndroidDeviceRecoveryAction {
  std::size_t sequence_id = 0;
  std::string subsystem;
  std::string reason;
  std::string action;
  std::string result;
  bool recoverable = false;
  std::string initial_health;
  std::string final_health;
};

struct SelfHealingAndroidDeviceReport {
  bool ready = false;
  std::string phase_name =
      "P13 Real ART Runtime Path / Java VM Bootstrap Contract";
  std::string session_id;
  std::string artifact_root;
  std::string report_json_path;
  std::string journal_path;
  std::string initial_health = "failed";
  std::string final_health = "failed";
  std::string storage_health = "not_requested";
  std::string sandbox_health = "not_requested";
  std::string permission_health = "not_requested";
  std::string app_ops_health = "not_requested";
  std::string activity_manager_health = "not_requested";
  std::string process_health = "not_requested";
  std::string window_health = "not_requested";
  std::string runtime_health = "not_requested";
  bool recoverable = false;
  int actions_attempted = 0;
  int actions_succeeded = 0;
  int actions_failed = 0;
  std::string recommended_next_action = "none";
  std::vector<SelfHealingAndroidDeviceRecoveryAction> actions;
  std::vector<std::string> limitation_flags;
  std::vector<std::string> errors;
};

class SelfHealingAndroidDeviceWatchdog {
 public:
  explicit SelfHealingAndroidDeviceWatchdog(
      const NativeApkLaunchReport& report);

  SelfHealingAndroidDeviceReport Run() const;

 private:
  const NativeApkLaunchReport& report_;
};

}  // namespace wfa

#endif  // WFA_APK_SELF_HEALING_WATCHDOG_HPP
