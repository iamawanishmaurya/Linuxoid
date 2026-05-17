#ifndef WFA_APK_LIFECYCLE_BRIDGE_HPP
#define WFA_APK_LIFECYCLE_BRIDGE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wfa {

struct NativeApkLifecycleBridgeContext {
  std::string session_id;
  std::string package_name;
  std::string apk_path;
  std::string staged_dir;
  std::string selected_abi;
  std::string selected_library_path;
  std::string launch_status;
  std::string asset_health;
  std::string resource_health;
  std::string surface_health;
  std::string surface_state;
  std::string artifact_root;
  int width = 320;
  int height = 240;
  int format = 1;
};

struct NativeApkInputEvent {
  std::string action;
  int x = 0;
  int y = 0;
  int key_code = 0;
  std::uint64_t sequence_id = 0;
  bool handled = false;
};

struct NativeApkInputEnqueueResult {
  bool accepted = false;
  bool rejected = false;
  NativeApkInputEvent event;
  std::vector<std::string> errors;
};

struct NativeApkLifecycleProof {
  bool ready = false;
  std::string session_id;
  std::string metadata_path;
  std::string event_log_path;
  std::vector<std::string> states_visited;
  std::vector<std::uint64_t> state_sequence_ids;
  std::string current_state;
  std::size_t events_dispatched = 0;
  std::vector<std::string> errors;
};

struct NativeApkLooperProof {
  bool ready = false;
  std::string metadata_path;
  std::string event_log_path;
  std::size_t posted_events = 0;
  std::size_t dispatched_events = 0;
  bool shutdown_clean = false;
  std::vector<std::string> errors;
};

struct NativeApkInputQueueProof {
  bool ready = false;
  std::string metadata_path;
  std::string event_log_path;
  std::size_t queued_events = 0;
  std::size_t dispatched_events = 0;
  std::size_t handled_events = 0;
  std::size_t rejected_events = 0;
  std::vector<std::string> handled_actions;
  std::vector<std::string> errors;
};

struct NativeApkLifecycleBridgeReport {
  bool ready = false;
  std::string exit_reason;
  NativeApkLifecycleProof lifecycle;
  NativeApkLooperProof looper;
  NativeApkInputQueueProof input_queue;
  std::vector<std::string> errors;
};

class NativeApkLifecycleBridgeSession {
 public:
  explicit NativeApkLifecycleBridgeSession(
      NativeApkLifecycleBridgeContext context);

  const NativeApkLifecycleBridgeContext& context() const;
  NativeApkInputEnqueueResult EnqueueInputEvent(
      const NativeApkInputEvent& event);
 NativeApkLifecycleBridgeReport RunDeterministicProof();

 private:
  NativeApkLifecycleBridgeContext context_;
  std::uint64_t next_sequence_id_ = 1;
  std::vector<NativeApkInputEvent> queued_input_events_;
  std::size_t rejected_input_events_ = 0;
  std::vector<std::string> session_errors_;
};

}  // namespace wfa

#endif  // WFA_APK_LIFECYCLE_BRIDGE_HPP
