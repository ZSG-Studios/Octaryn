#pragma once
#include "LocalSession.h"
#include "SessionChannels.h"
#include <optional>

namespace octaryn::client::app::local_session {
// Disk operations belong exclusively to this worker after session startup.
// Mailboxes are bounded and their lock is never held during filesystem I/O.
class SessionIo {
public:
 struct Update {
 uint64_t acknowledged_input_frame{};
    std::optional<LocalPlayerPose> pose;
    std::string status;
  };
  SessionIo(std::filesystem::path pose, std::filesystem::path input,
            std::filesystem::path window, bool read_pose_file = true, SessionChannels channels = {});
  ~SessionIo();
  SessionIo(const SessionIo&) = delete;
  SessionIo& operator=(const SessionIo&) = delete;
  void stop();
  Update poll();
  void publish_input(std::string text);
  void publish_window(std::string text);
  void publish_time(std::string text);
  bool publish_ui_action(std::string action);
  bool poll_module_event(uint64_t& id, uint64_t& kind, uint64_t& p1, uint64_t& p2);
  bool publish_scene_physics(std::string request);
  bool scene_physics_snapshot(std::string& snapshot) const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
