#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace octaryn::client::app {
enum class InventoryCategory { All, Terrain, Nature, Lighting, Fluids };
struct InventoryBlock {
  std::uint16_t id{};
  std::string key, name;
  unsigned atlas_tile{};
  InventoryCategory category{InventoryCategory::Terrain};
};
struct InventoryEquipment {
  std::string key, name, equip_slot;
  bool available{};
};
struct InventoryStack {std::uint16_t block{};std::uint32_t count{};};
// Client building shortcuts; block edits remain subject to server authority.
class Inventory {
public:
  static constexpr unsigned SlotCount=50, HotbarCount=10;
  static constexpr unsigned StackLimit=999;
  bool load_catalog(const std::filesystem::path& blocks, const std::filesystem::path& hand);
  bool load(const std::filesystem::path& path);
  bool save_if_changed(const std::filesystem::path& path);
  const std::vector<InventoryBlock>& blocks() const { return blocks_; }
  const InventoryBlock* find(std::uint16_t id) const;
  std::vector<std::uint16_t> search(std::string_view query, InventoryCategory category=InventoryCategory::All) const;
  const std::array<std::uint16_t,SlotCount>& slots() const { return slots_; }
  const std::array<std::uint32_t,SlotCount>& counts() const { return counts_; }
  InventoryStack cursor() const { return cursor_; }
  bool take_creative(std::uint16_t block,bool single=false);
  bool right_click_slot(unsigned index);
  InventoryStack drop_cursor(bool single=false);
  std::uint32_t receive(std::uint16_t block,std::uint32_t count);
  bool credit_grant(std::uint64_t grant,std::uint16_t block,std::uint32_t count);
  std::uint32_t reserved_drop() const {return reserved_drop_;}
  bool reserve_drop(std::uint32_t count);
  void resolve_drop(bool accepted,std::uint64_t receipt=0);
  std::uint64_t drop_watermark() const {return drop_watermark_;}
  std::uint64_t grant_watermark() const {return grant_watermark_;}
  unsigned selected_slot() const { return selected_; }
  std::uint16_t selected_block() const { return slots_[selected_]; }
  bool select_hotbar(unsigned index);
  bool cycle_hotbar(int delta);
  bool assign(std::uint16_t block);
  bool pick(std::uint16_t block);
  bool click_slot(unsigned index);
  bool clear_slot(unsigned index);
  bool sort_backpack();
  void cancel_move();
  int moving_slot() const { return moving_; }
  std::uint64_t revision() const { return revision_; }
  bool dirty() const { return dirty_; }
  const InventoryEquipment& hand() const { return hand_; }
private:
  void defaults();
  void changed();
  std::vector<InventoryBlock> blocks_;
  std::array<std::uint16_t,SlotCount> slots_{};
  std::array<std::uint32_t,SlotCount> counts_{};
  InventoryStack cursor_{};
  std::uint64_t grant_watermark_{};
  std::uint32_t reserved_drop_{};
  std::uint64_t drop_watermark_{};
  InventoryEquipment hand_;
  unsigned selected_{};
  int moving_{-1};
  std::uint64_t revision_{};
  bool dirty_{};
};
}
