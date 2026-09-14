#include "BlockInteraction.h"
#include "WorldStream.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>

namespace octaryn::client::world_presentation {
struct CatalogFile { std::string schema; std::vector<InteractionBlock> blocks; };
namespace {
constexpr double TargetReach = 10.0;
constexpr double AuthorityReachSquared = 36.0;
bool valid_eye(const InteractionEye& eye) {
  return std::isfinite(eye.x) && std::isfinite(eye.y) && std::isfinite(eye.z) &&
      std::abs(eye.x)<=32000000 && std::abs(eye.z)<=32000000 && std::abs(eye.y)<=10000;
}
}
bool BlockInteraction::load_catalog(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const auto size = file.tellg();
  if (!file || size <= 0 || size > 1024 * 1024) return false;
  std::string text(static_cast<std::size_t>(size), '\0');
  file.seekg(0);
  if (!file.read(text.data(), size)) return false;
  CatalogFile parsed;
  constexpr glz::opts options{.error_on_unknown_keys = false};
  if (glz::read<options>(parsed, text) || parsed.schema != "octaryn.basegame.blocks.v1" ||
      parsed.blocks.empty() || parsed.blocks.size() > 65536 ||
      parsed.blocks.front().id != "octaryn.basegame.block.air") return false;
  catalog_ = std::move(parsed.blocks);
  if (selected_ >= catalog_.size() || !catalog_[selected_].placeable) cycle(1);
  return true;
}
std::string BlockInteraction::selected_name() const {
  return selected_ < catalog_.size() ? catalog_[selected_].displayName : "Unknown";
}
bool BlockInteraction::cycle(int delta) {
  if (catalog_.size() < 2 || delta == 0) return false;
  const auto count = static_cast<std::int64_t>(catalog_.size() - 1);
  auto candidate = static_cast<std::int64_t>(selected_);
  for (std::size_t attempt = 1; attempt < catalog_.size(); ++attempt) {
    candidate = ((candidate - 1 + delta) % count + count) % count + 1;
    if (catalog_[static_cast<std::size_t>(candidate)].placeable) { selected_ = static_cast<std::uint16_t>(candidate); return true; }
  }
  return false;
}
bool BlockInteraction::select(std::uint16_t block) {
  if(block>=catalog_.size() || (block!=0 && !catalog_[block].placeable)) return false;
  selected_=block;return true;
}
bool BlockInteraction::pick() {
  if (target_.hit && target_.block_id < catalog_.size() && catalog_[target_.block_id].targetable) {
    selected_ = target_.block_id;
    return true;
  }
  return false;
}
void BlockInteraction::update(const WorldStream& world, float x, float y, float z, float yaw, float pitch) {
  update(world,{{x,y,z},yaw,pitch},{x,y,z});
}
void BlockInteraction::update(const WorldStream& world,const InteractionView& view,const InteractionEye& authority_eye) {
  target_ = {};
  if (!valid_eye(view.origin) || !valid_eye(authority_eye) ||
      !std::isfinite(view.yaw) || !std::isfinite(view.pitch)) return;
  x_=authority_eye.x;y_=authority_eye.y;z_=authority_eye.z;
  const auto yaw=view.yaw,pitch=view.pitch;
  const std::array<double, 3> origin{view.origin.x,view.origin.y,view.origin.z};
  const std::array<double, 3> direction{std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
  std::array<int, 3> cell{}, step{};
  std::array<double, 3> next{}, interval{};
  for (std::size_t axis = 0; axis < cell.size(); ++axis) {
    cell[axis] = static_cast<int>(std::floor(origin[axis]));
    step[axis] = direction[axis] < 0 ? -1 : 1;
    interval[axis] = direction[axis] == 0 ? std::numeric_limits<double>::infinity() : std::abs(1.0 / direction[axis]);
    next[axis] = direction[axis] == 0 ? interval[axis] :
        (cell[axis] + (step[axis] > 0 ? 1 : 0) - origin[axis]) / direction[axis];
  }
  BlockPosition previous{cell[0], cell[1], cell[2]};
  bool previous_air = false;
  double distance = 0;
  for (int visit = 0; visit < 64 && distance <= TargetReach; ++visit) {
    std::uint16_t block{};
    if (!world.try_block(cell[0], cell[1], cell[2], block)) return;
    if (block < catalog_.size() && catalog_[block].targetable) {
      const double dx = x_ - (cell[0] + .5), dy = y_ - (cell[1] + .5), dz = z_ - (cell[2] + .5);
      target_ = {true, dx*dx + dy*dy + dz*dz <= AuthorityReachSquared, previous_air,
          {cell[0], cell[1], cell[2]}, previous, block, static_cast<float>(distance)};
      return;
    }
    previous = {cell[0], cell[1], cell[2]};
    previous_air = block == 0;
    const std::size_t axis = next[0] <= next[1] && next[0] <= next[2] ? 0u : next[1] <= next[2] ? 1u : 2u;
    distance = next[axis];
    cell[axis] += step[axis];
    next[axis] += interval[axis];
  }
}
bool BlockInteraction::make_edit(bool place, BlockEditIntent& edit) const {
  if (!target_.hit || !target_.actionable) return false;
  if (place && (!target_.can_place || selected_ >= catalog_.size() || !catalog_[selected_].placeable)) return false;
  edit = {place ? target_.adjacent : target_.block, target_.block,
      static_cast<std::uint16_t>(place ? selected_ : 0), x_, y_, z_};
  return true;
}
}
