#include "TerrainGeneration.h"

#include <cmath>
#include <cstdio>
#include <string_view>

namespace {

constexpr uint16_t Air = 0u;
constexpr uint16_t White = 1u;
constexpr uint16_t Sand = 4u;
constexpr uint16_t Water = 7u;

struct PlanContext {
  int call_count = 0;
  int local_x = -1;
  int local_z = -1;
  float height_noise = 0.0f;
};

bool expect_true(std::string_view label, bool value) {
  if (value) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected true\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

bool expect_equal(std::string_view label, auto actual, auto expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

int32_t fixed_lowland_plan(void *context,
                           const OctarynServerTerrainColumnSample *sample,
                           OctarynServerTerrainColumnPlan *plan) {
  if (sample == nullptr || plan == nullptr) {
    return -1;
  }

  auto *capture = static_cast<PlanContext *>(context);
  if (capture != nullptr) {
    capture->call_count += 1;
    capture->local_x = sample->local_x;
    capture->local_z = sample->local_z;
    capture->height_noise = sample->height_noise;
  }

  *plan = OctarynServerTerrainColumnPlan{
      .world_x = sample->world_x,
      .world_z = sample->world_z,
      .local_x = sample->local_x,
      .local_z = sample->local_z,
      .local_width = sample->local_width,
      .local_depth = sample->local_depth,
      .terrain_height = 18,
      .decoration_y = 30,
      .surface_block = Sand,
      .fill_block = Sand,
      .is_lowland = 1u,
      .has_grass_surface = 0u,
  };
  return 0;
}

bool validate_generated_blocks() {
  PlanContext context;
  uint16_t block = 99u;
  bool ok = true;
  ok &= expect_equal("fill block result",
                     octaryn_server_terrain_generated_block(
                         -1, 17, -33, 30, Water, fixed_lowland_plan, &context,
                         &block),
                     0);
  ok &= expect_equal("fill block", block, Sand);
  ok &= expect_equal("floor local x", context.local_x, 31);
  ok &= expect_equal("floor local z", context.local_z, 31);
  ok &= expect_true("height noise finite", std::isfinite(context.height_noise));

  ok &= expect_equal("surface block result",
                     octaryn_server_terrain_generated_block(
                         0, 18, 0, 30, Water, fixed_lowland_plan, &context,
                         &block),
                     0);
  ok &= expect_equal("surface block", block, Sand);

  ok &= expect_equal("water block result",
                     octaryn_server_terrain_generated_block(
                         0, 29, 0, 30, Water, fixed_lowland_plan, &context,
                         &block),
                     0);
  ok &= expect_equal("water block", block, Water);

  ok &= expect_equal("air block result",
                     octaryn_server_terrain_generated_block(
                         0, 30, 0, 30, Water, fixed_lowland_plan, &context,
                         &block),
                     0);
  ok &= expect_equal("air block", block, Air);
  return ok;
}

bool validate_empty_world() {
  bool ok = true;
  ok &= expect_equal("empty world solid",
                     octaryn_server_empty_world_generated_block(0, -1, 0),
                     White);
  ok &= expect_equal("empty world air",
                     octaryn_server_empty_world_generated_block(0, 0, 0), Air);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_generated_blocks();
  ok &= validate_empty_world();

  if (!ok) {
    return 1;
  }

  std::puts("server terrain generation native probe passed");
  return 0;
}
