#pragma once
#include "LocalSession.h"
#include <optional>

namespace octaryn::client::app::local_session {
// Disk operations belong exclusively to this worker after session startup.
// Mailboxes are bounded and their lock is never held during filesystem I/O.
class SessionIo {
public:
 struct Update {
 uint64_t acknowledged_input_frame{};
    std::optional<LocalPlayerPose> pose;
 std::optional<BlockReceipts> receipts;
    std::string status, interaction_status;
  };
  SessionIo(std::filesystem::path pose, std::filesystem::path input,
            std::filesystem::path window, std::filesystem::path interaction);
  ~SessionIo();
  SessionIo(const SessionIo&) = delete;
  SessionIo& operator=(const SessionIo&) = delete;
  void stop();
  Update poll();
  void publish_input(std::string text);
  void publish_window(std::string text);
  void publish_time(std::string text);
 void publish_receipt_ack(std::string text);
  bool submit_edit(std::string text);
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
