#pragma once
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>

namespace octaryn::client::app::local_session {
// Command memory is bounded; only the session worker accesses disk.
class ScenePhysicsIo {
public:
  bool submit(std::string request);
  bool snapshot(std::string& text) const;
  void exchange(const std::filesystem::path& directory);
private:
  mutable std::mutex mutex_;
  std::deque<std::pair<uint64_t,std::string>> pending_;
  uint64_t epoch_{}, sequence_{};
  std::string snapshot_, published_;
};
}
