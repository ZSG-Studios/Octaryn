#pragma once
#include <cstdint>
#include <unordered_set>

namespace octaryn::client::app {
class GameUi;
class ModuleActionValidation {
public:
  explicit ModuleActionValidation(bool enabled):enabled_(enabled) {}
  void start(GameUi& ui);
  void event(std::uint64_t id,std::uint64_t kind,std::uint64_t item,std::uint64_t count);
  bool finish() const;
private:
  bool enabled_{},started_{},failed_{};
  unsigned drops_{};
  std::uint64_t item_{};
  std::unordered_set<std::uint64_t> receipts_;
};
}
