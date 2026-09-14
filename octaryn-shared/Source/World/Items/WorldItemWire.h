#pragma once
#include <cstdint>
#include <type_traits>

namespace octaryn::world_items {
constexpr std::uint32_t max_items=256, max_grants=64, stack_limit=64;
constexpr std::uint32_t drop_limit=999; // One atomic command may split into multiple world stacks.
struct Item {
  std::uint64_t id{};
  std::uint32_t block{},count{};
  float x{},y{},z{},vx{},vy{},vz{};
  double age{},pickup_delay{};
};
struct Grant { std::uint64_t id{};std::uint32_t block{},count{}; };
enum class DropResult : std::uint32_t { None,Accepted,Invalid,Full,RateLimited };
struct State {
  std::uint32_t version{1},size{sizeof(State)};
  std::uint64_t next_item{1},next_grant{1},last_command{},acknowledged_grant{};
  double seconds{},next_drop{};
  std::uint32_t receipt_block{},receipt_count{};
  DropResult receipt_result{};
  std::uint32_t item_count{},grant_count{},reserved{};
  Item items[max_items]{};
  Grant grants[max_grants]{};
};
struct Intent {
  std::uint32_t version{1},size{sizeof(Intent)};
  std::uint64_t command{},acknowledge{};
  std::uint32_t block{},count{};
};
static_assert(sizeof(Item)==56 && sizeof(Grant)==16 && sizeof(Intent)==32);
static_assert(sizeof(State)==15440 && std::is_trivially_copyable_v<State>);
}
