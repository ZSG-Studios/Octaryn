#include "BlockStore.h"
#include "PlayerSimulation.h"

#include <cmath>
#include <cstdio>
#include <string_view>

namespace {

constexpr uint16_t WhiteBlock = 1u;

using octaryn::server::world::blocks::BlockStore;

bool expect_true(std::string_view label, bool value) {
  if (value) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected true\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

bool expect_close(std::string_view label, float actual, float expected) {
  if (std::fabs(actual - expected) <= 0.001f) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch actual=%f expected=%f\n",
               static_cast<int>(label.size()), label.data(), actual, expected);
  return false;
}

OctarynServerPlayerState default_state() {
  return OctarynServerPlayerState{.x = 0.0f,
                                  .y = 80.0f,
                                  .z = 0.0f,
                                  .pitch = -0.35f,
                                  .yaw = 0.0f,
                                  .velocity_x = 0.0f,
                                  .velocity_y = 0.0f,
                                  .velocity_z = 0.0f,
                                  .is_on_ground = 0u,
                                  .control_mode = 0u,
                                  .selected_block = 25u,
                                  .reserved = 0u};
}

OctarynServerPlayerInput input(uint32_t flags, float move_x, float move_y,
                               float move_z) {
  return OctarynServerPlayerInput{.flags = flags,
                                  .controller = 1u,
                                  .move_x = move_x,
                                  .move_y = move_y,
                                  .move_z = move_z,
                                  .camera_x = 0.0f,
                                  .camera_y = 0.0f,
                                  .camera_z = 0.0f,
                                  .camera_pitch = -0.35f,
                                  .camera_yaw = 0.0f,
                                  .relative_mouse = 0};
}

uint16_t generated_block(void *, int32_t, int32_t, int32_t) { return 0u; }

uint32_t is_solid_block(void *, uint16_t block) {
  return block == WhiteBlock ? 1u : 0u;
}

} // namespace

bool validate_block_store_step_update() {
  BlockStore store;
  auto state = default_state();
  state.velocity_x = 3.0f;
  OctarynServerPlayerInput none{};
  OctarynServerPlayerTickResult result{};
  bool ok = true;
  ok &= expect_true("block store idle step result",
                    octaryn_server_player_step_with_block_store(
                        &none, 0.05, &store, generated_block, is_solid_block,
                        nullptr, &state, &result) == 0);
  ok &= expect_true("block store idle step input", result.tick_input == 0u);
  ok &= expect_close("block store idle step velocity x", state.velocity_x,
                     0.0f);
  ok &= expect_close("block store idle step delta x", result.delta_x, 0.0f);
  ok &= expect_close("block store idle step delta y", result.delta_y, 0.0f);
  ok &= expect_close("block store idle step delta z", result.delta_z, 0.0f);

  state = default_state();
  state.y = octaryn_server_player_spawn_eye_height();
  state.is_on_ground = 1u;
  result = {};
  const auto forward = input(0u, 0.0f, 0.0f, 1.0f);
  ok &= expect_true("block store move step result",
                    octaryn_server_player_step_with_block_store(
                        &forward, 0.05, &store, generated_block,
                        is_solid_block, nullptr, &state, &result) == 0);
  ok &= expect_true("block store move step input", result.tick_input == 1u);
  ok &= expect_close("block store move step z", state.z, -0.25f);
  ok &= expect_close("block store move step delta x", result.delta_x, 0.0f);
  ok &= expect_close("block store move step delta y", result.delta_y, -0.06f);
  ok &= expect_close("block store move step delta z", result.delta_z, -0.25f);
  return ok;
}
