#pragma once
#include <cstddef>
#include <cstdint>

namespace octaryn::client::app {
struct WorldItemPose {
  uint64_t entity_id{}, generation{}, source_tick{};
  float x{}, y{}, z{}, velocity_x{}, velocity_y{}, velocity_z{};
  uint32_t item_id{}, count{}, flags{}, reserved{};
  float previous_x{}, previous_y{}, previous_z{};
  uint32_t previous_reserved{};
  uint64_t previous_tick{};
};
static_assert(sizeof(WorldItemPose)==88);
static_assert(offsetof(WorldItemPose,previous_tick)==80);
}
