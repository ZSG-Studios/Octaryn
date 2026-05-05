#include "PlayerSimulation.h"

#include <cmath>
#include <cstdio>
#include <string_view>
#include <unordered_set>

namespace {

constexpr uint32_t JumpFlag = 1u << 0u;
constexpr uint32_t SprintFlag = 1u << 1u;
constexpr uint32_t FlyModeFlag = 1u << 2u;
constexpr uint32_t SolidBlockFlag = 1u << 16u;
constexpr uint16_t WhiteBlock = 1u;

struct BlockKey {
  int32_t x;
  int32_t y;
  int32_t z;

  friend bool operator==(const BlockKey &left, const BlockKey &right) = default;
};

struct BlockKeyHash {
  size_t operator()(const BlockKey &key) const {
    size_t value = static_cast<size_t>(static_cast<uint32_t>(key.x));
    value = (value * 16777619u) ^
            static_cast<size_t>(static_cast<uint32_t>(key.y));
    value = (value * 16777619u) ^
            static_cast<size_t>(static_cast<uint32_t>(key.z));
    return value;
  }
};

struct ProbeWorld {
  std::unordered_set<BlockKey, BlockKeyHash> solids;
};

uint32_t query_block(void *context, int32_t x, int32_t y, int32_t z) {
  const auto *world = static_cast<const ProbeWorld *>(context);
  if (!world || !world->solids.contains(BlockKey{.x = x, .y = y, .z = z})) {
    return 0u;
  }

  return static_cast<uint32_t>(WhiteBlock) | SolidBlockFlag;
}

bool expect_true(std::string_view label, bool value) {
  if (value) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected true\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

bool expect_close(std::string_view label, float actual, float expected,
                  float epsilon = 0.001f) {
  if (std::fabs(actual - expected) <= epsilon) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch actual=%f expected=%f\n",
               static_cast<int>(label.size()), label.data(), actual, expected);
  return false;
}

OctarynServerPlayerState default_state() {
  return OctarynServerPlayerState{
      .x = 0.0f,
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
                               float move_z, float pitch = -0.35f,
                               float yaw = 0.0f) {
  return OctarynServerPlayerInput{
      .flags = flags,
      .controller = 1u,
      .move_x = move_x,
      .move_y = move_y,
      .move_z = move_z,
      .camera_x = 0.0f,
      .camera_y = 0.0f,
      .camera_z = 0.0f,
      .camera_pitch = pitch,
      .camera_yaw = yaw,
      .relative_mouse = 0};
}

bool validate_spawn_alignment() {
  ProbeWorld world;
  world.solids.insert(BlockKey{.x = 0, .y = 10, .z = 0});
  auto state = default_state();
  OctarynServerPlayerSpawnAlignment alignment{};
  const int result =
      octaryn_server_player_align_spawn(&state, 0u, query_block, &world, &alignment);

  bool ok = true;
  ok &= expect_true("spawn align result", result == 0);
  ok &= expect_true("spawn aligned", alignment.aligned == 1u);
  ok &= expect_true("spawn surface block", alignment.surface_block == WhiteBlock);
  ok &= expect_true("spawn surface y", alignment.surface_y == 10);
  ok &= expect_close("spawn eye y", state.y,
                     10.0f + octaryn_server_player_spawn_eye_height());
  ok &= expect_close("spawn pitch", state.pitch, -0.35f);
  return ok;
}

bool validate_default_state() {
  OctarynServerPlayerState state{};
  const int result = octaryn_server_player_default_state(&state);

  bool ok = true;
  ok &= expect_true("default state result", result == 0);
  ok &= expect_close("default x", state.x, 0.0f);
  ok &= expect_close("default y", state.y, 80.0f);
  ok &= expect_close("default z", state.z, 0.0f);
  ok &= expect_close("default pitch", state.pitch, -0.35f);
  ok &= expect_close("default yaw", state.yaw, 0.0f);
  ok &= expect_close("default velocity x", state.velocity_x, 0.0f);
  ok &= expect_close("default velocity y", state.velocity_y, 0.0f);
  ok &= expect_close("default velocity z", state.velocity_z, 0.0f);
  ok &= expect_true("default walk mode", state.control_mode == 0u);
  ok &= expect_true("default selected block", state.selected_block == 25u);
  return ok;
}

bool validate_walk_ground_and_jump() {
  ProbeWorld world;
  for (int32_t x = -4; x <= 4; x++) {
    for (int32_t z = -4; z <= 4; z++) {
      world.solids.insert(BlockKey{.x = x, .y = 0, .z = z});
    }
  }

  OctarynServerPlayerState state = default_state();
  state.y = octaryn_server_player_spawn_eye_height();
  state.is_on_ground = 1u;
  const auto forward = input(0u, 0.0f, 0.0f, 1.0f);
  const int walk_result =
      octaryn_server_player_move(&forward, 0.05, query_block, &world, &state);

  bool ok = true;
  ok &= expect_true("walk result", walk_result == 0);
  ok &= expect_close("walk forward z", state.z, -0.25f);
  ok &= expect_close("walk velocity z", state.velocity_z, -5.0f);
  ok &= expect_true("walk mode", state.control_mode == 0u);
  ok &= expect_true("walk applies gravity without contact", state.is_on_ground == 0u);

  state = default_state();
  state.y = octaryn_server_player_spawn_eye_height();
  state.is_on_ground = 1u;
  const auto jump = input(JumpFlag, 0.0f, 0.0f, 0.0f);
  ProbeWorld empty_world;
  const int jump_result =
      octaryn_server_player_move(&jump, 0.05, query_block, &empty_world, &state);
  ok &= expect_true("jump result", jump_result == 0);
  ok &= expect_true("jump leaves ground", state.is_on_ground == 0u);
  ok &= expect_true("jump rises", state.y > octaryn_server_player_spawn_eye_height());
  ok &= expect_close("jump velocity y", state.velocity_y, 7.3f);
  return ok;
}

bool validate_wall_collision() {
  ProbeWorld world;
  for (int32_t y = 0; y <= 3; y++) {
    for (int32_t z = -1; z <= 1; z++) {
      world.solids.insert(BlockKey{.x = 1, .y = y, .z = z});
    }
  }

  OctarynServerPlayerState state = default_state();
  state.x = 0.6f;
  state.y = octaryn_server_player_spawn_eye_height();
  state.is_on_ground = 1u;
  const auto right = input(0u, 1.0f, 0.0f, 0.0f);
  const int result =
      octaryn_server_player_move(&right, 0.1, query_block, &world, &state);

  bool ok = true;
  ok &= expect_true("wall move result", result == 0);
  ok &= expect_true("wall clamps x", state.x < 0.701f);
  ok &= expect_close("wall stops x velocity", state.velocity_x, 0.0f);
  return ok;
}

bool validate_fly_move() {
  ProbeWorld world;
  auto state = default_state();
  const auto fly = input(FlyModeFlag | SprintFlag, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f);
  const int result =
      octaryn_server_player_move(&fly, 0.05, query_block, &world, &state);

  bool ok = true;
  ok &= expect_true("fly result", result == 0);
  ok &= expect_close("fly y", state.y, 85.0f);
  ok &= expect_close("fly z", state.z, -5.0f);
  ok &= expect_close("fly velocity y", state.velocity_y, 100.0f);
  ok &= expect_true("fly mode", state.control_mode == 1u);
  ok &= expect_true("fly leaves ground", state.is_on_ground == 0u);
  return ok;
}

bool validate_idle_update() {
  auto state = default_state();
  state.velocity_x = 3.0f;
  state.velocity_y = -4.0f;
  state.velocity_z = 5.0f;
  state.is_on_ground = 1u;
  const int result = octaryn_server_player_idle(&state);

  bool ok = true;
  ok &= expect_true("idle result", result == 0);
  ok &= expect_close("idle velocity x", state.velocity_x, 0.0f);
  ok &= expect_close("idle velocity y", state.velocity_y, 0.0f);
  ok &= expect_close("idle velocity z", state.velocity_z, 0.0f);
  ok &= expect_true("idle preserves ground", state.is_on_ground == 1u);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_default_state();
  ok &= validate_spawn_alignment();
  ok &= validate_walk_ground_and_jump();
  ok &= validate_wall_collision();
  ok &= validate_fly_move();
  ok &= validate_idle_update();
  if (!ok) {
    return 1;
  }

  std::puts("server player simulation native probe passed");
  return 0;
}
