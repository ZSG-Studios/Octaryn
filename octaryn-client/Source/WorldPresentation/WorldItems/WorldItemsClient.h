#pragma once
#include "WorldItemWire.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace octaryn::client::world_presentation {
struct WorldItemSnapshot {
  double source_seconds{};
  std::vector<octaryn::world_items::Item> items;
};
struct DropReceipt {
  std::uint64_t command_id{};
  std::uint32_t block_id{},count{};
  octaryn::world_items::DropResult result{};
  bool accepted() const {return result==octaryn::world_items::DropResult::Accepted;}
};
using PickupGrant=octaryn::world_items::Grant;
class WorldItemsClient {
public:
  explicit WorldItemsClient(const std::filesystem::path& world_root);
  ~WorldItemsClient();
  WorldItemsClient(const WorldItemsClient&)=delete;
  WorldItemsClient& operator=(const WorldItemsClient&)=delete;
  bool submit_drop(std::uint16_t block,std::uint32_t count);
  bool drop_receipt(DropReceipt&) const;
  void acknowledge_drop(std::uint64_t command);
  bool next_pickup(PickupGrant&) const;
  void acknowledge_pickup(std::uint64_t grant);
  std::uint64_t acknowledged_pickup() const;
  std::shared_ptr<const WorldItemSnapshot> snapshot() const;
  std::string status() const;
private:
  struct State;std::unique_ptr<State> state_;
};
}
