#pragma once
#include <array>
#include <compare>
#include <cstdint>
#include <list>
#include <map>
#include <span>

namespace octaryn::client::rendering {

// Integer cell coordinates, never camera-relative positions. Level 0/1/2 = 4/16/64 blocks.
struct FarFieldKey {
  std::int32_t x{},y{},z{};
  unsigned level{};
  auto operator<=>(const FarFieldKey&) const = default;
};
unsigned far_field_width(unsigned level);
FarFieldKey far_field_key(std::int32_t x,std::int32_t y,std::int32_t z,unsigned level);
enum class FarFieldState { Unknown, Empty, Occupied };
struct FarFieldNode {
  // Child order: x + 4*(y + 4*z). Occupied means a material witness, not a full opaque hit.
  std::uint64_t known{},occupied{},uniform{};
  std::array<std::uint16_t,64> material{};
  std::uint32_t material_features{};
  std::uint64_t authority_revision{},geometry_epoch{};
  bool complete() const {return known==~std::uint64_t{};}
  FarFieldState state() const {
    return occupied?FarFieldState::Occupied:complete()?FarFieldState::Empty:FarFieldState::Unknown;
  }
};
FarFieldNode far_field_aggregate(std::span<const FarFieldNode,64> children);

// Presentation/coordinator owned. Tickets reject work started before any edit/reset.
// FIFO eviction affects availability only; a retained parent remains a valid summary.
class FarFieldCache {
  struct Entry {FarFieldNode node;std::list<FarFieldKey>::iterator order;};
  std::map<FarFieldKey,Entry> nodes_;
  std::list<FarFieldKey> order_;
  std::size_t capacity_;
  std::uint64_t ticket_{1},world_epoch_{};
public:
  explicit FarFieldCache(std::size_t capacity=4096):capacity_(capacity) {}
  FarFieldCache(const FarFieldCache&)=delete;
  FarFieldCache& operator=(const FarFieldCache&)=delete;
  std::uint64_t ticket() const {return ticket_;}
  std::size_t size() const {return nodes_.size();}
  const FarFieldNode* find(FarFieldKey key) const;
  bool publish(FarFieldKey key,FarFieldNode node,std::uint64_t ticket);
  void invalidate(std::int32_t x,std::int32_t y,std::int32_t z);
  void invalidate_column(std::int32_t column_x,std::int32_t column_z);
  void reset(std::uint64_t world_epoch);
};
} // namespace octaryn::client::rendering
