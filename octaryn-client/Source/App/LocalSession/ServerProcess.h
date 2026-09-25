#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace octaryn::client::app::local_session {
class ServerProcess {
public:
  ServerProcess();
  ~ServerProcess();
  bool start(const std::filesystem::path& executable, const std::filesystem::path& log,
             const std::vector<std::pair<std::string, std::string>>& environment);
  bool running() const;
  void terminate();
private:
  struct Handles;
  std::unique_ptr<Handles> handles_;
};
}
