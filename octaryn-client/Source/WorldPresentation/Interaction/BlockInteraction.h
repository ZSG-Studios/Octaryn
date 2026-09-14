#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace octaryn::client::world_presentation {
class WorldStream;
struct BlockPosition { std::int32_t x{}, y{}, z{}; };
struct BlockTarget {
  bool hit{}, actionable{}, can_place{};
  BlockPosition block{}, adjacent{};
  std::uint16_t block_id{};
  float distance{};
};
struct BlockEditIntent {
  BlockPosition edit{}, hit{};
  std::uint16_t block{};
  float camera_x{}, camera_y{}, camera_z{};
};
struct InteractionBlock {
  std::string id, displayName;
  bool placeable{}, targetable{}, solid{};
};
struct InteractionEye { float x{}, y{}, z{}; };
struct InteractionView { InteractionEye origin; float yaw{}, pitch{}; };
class BlockInteraction {
public:
  bool load_catalog(const std::filesystem::path& path);
  void update(const WorldStream& world, float x, float y, float z, float yaw, float pitch);
  // Pick along the rendered crosshair ray; authority and edit intents retain the player eye.
  void update(const WorldStream& world, const InteractionView& view, const InteractionEye& authority_eye);
  const BlockTarget& target() const { return target_; }
  std::uint16_t selected() const { return selected_; }
  std::string selected_name() const;
  bool blocks_camera(std::uint16_t block) const { return block >= catalog_.size() || catalog_[block].solid; }
  bool cycle(int delta);
  bool select(std::uint16_t block);
  bool pick();
  bool make_edit(bool place, BlockEditIntent& edit) const;
private:
  std::vector<InteractionBlock> catalog_;
  std::uint16_t selected_{25};
  BlockTarget target_;
  float x_{}, y_{}, z_{};
};
}
