#include "BlockStore.h"
#include "PlayerSimulation.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
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
    value =
        (value * 16777619u) ^ static_cast<size_t>(static_cast<uint32_t>(key.y));
    value =
        (value * 16777619u) ^ static_cast<size_t>(static_cast<uint32_t>(key.z));
    return value;
  }
};

struct ProbeWorld {
  std::unordered_set<BlockKey, BlockKeyHash> solids;
};

using octaryn::server::world::blocks::BlockEdit;
using octaryn::server::world::blocks::BlockPosition;
using octaryn::server::world::blocks::BlockStore;

uint32_t query_block(void *context, int32_t x, int32_t y, int32_t z) {
  const auto *world = static_cast<const ProbeWorld *>(context);
  if (!world || !world->solids.contains(BlockKey{.x = x, .y = y, .z = z})) {
    return 0u;
  }

  return static_cast<uint32_t>(WhiteBlock) | SolidBlockFlag;
}

uint16_t generated_block(void *context, int32_t x, int32_t y, int32_t z) {
  const auto *world = static_cast<const ProbeWorld *>(context);
  if (!world || !world->solids.contains(BlockKey{.x = x, .y = y, .z = z})) {
    return 0u;
  }

  return WhiteBlock;
}

uint32_t is_solid_block(void *, uint16_t block) {
  return block == WhiteBlock ? 1u : 0u;
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
                               float move_z, float pitch = -0.35f,
                               float yaw = 0.0f) {
  return OctarynServerPlayerInput{.flags = flags,
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
  const int result = octaryn_server_player_align_spawn(&state, 0u, query_block,
                                                       &world, &alignment);

  bool ok = true;
  ok &= expect_true("spawn align result", result == 0);
  ok &= expect_true("spawn aligned", alignment.aligned == 1u);
  ok &= expect_true("spawn adjusted", alignment.adjusted == 1u);
  ok &=
      expect_true("spawn surface block", alignment.surface_block == WhiteBlock);
  ok &= expect_true("spawn surface y", alignment.surface_y == 10);
  ok &= expect_close("spawn eye y", state.y,
                     10.0f + octaryn_server_player_spawn_eye_height());
  ok &= expect_close("spawn pitch", state.pitch, -0.35f);

  state = default_state();
  state.y = 12.0f + octaryn_server_player_spawn_eye_height();
  alignment = {};
  const int saved_result = octaryn_server_player_align_spawn(
      &state, 1u, query_block, &world, &alignment);
  ok &= expect_true("saved spawn align result", saved_result == 0);
  ok &= expect_true("saved spawn aligned", alignment.aligned == 1u);
  ok &= expect_true("saved spawn not adjusted", alignment.adjusted == 0u);
  ok &= expect_close("saved spawn keeps y", state.y,
                     12.0f + octaryn_server_player_spawn_eye_height());
  return ok;
}

bool validate_block_store_spawn_alignment() {
  ProbeWorld world;
  world.solids.insert(BlockKey{.x = 0, .y = 10, .z = 0});
  BlockStore store;
  auto state = default_state();
  OctarynServerPlayerSpawnAlignment alignment{};
  const int result = octaryn_server_player_align_spawn_with_block_store(
      &state, 0u, &store, generated_block, is_solid_block, &world, &alignment);

  bool ok = true;
  ok &= expect_true("block store spawn align result", result == 0);
  ok &= expect_true("block store spawn aligned", alignment.aligned == 1u);
  ok &= expect_true("block store spawn adjusted", alignment.adjusted == 1u);
  ok &= expect_true("block store spawn surface block",
                    alignment.surface_block == WhiteBlock);
  ok &= expect_true("block store spawn surface y", alignment.surface_y == 10);
  ok &= expect_close("block store spawn eye y", state.y,
                     10.0f + octaryn_server_player_spawn_eye_height());
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

bool validate_saved_state_load() {
  OctarynServerPlayerState state{};
  const int result = octaryn_server_player_state_from_save(
      1.0f, 2000.0f, -3.0f, 2.0f, 4.0f * 3.14159265358979323846f, 9u,
      &state);
  const int invalid_result = octaryn_server_player_state_from_save(
      std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f, 0.0f, 9u,
      &state);

  bool ok = true;
  ok &= expect_true("saved state result", result == 0);
  ok &= expect_close("saved state x", state.x, 1.0f);
  ok &= expect_close("saved state y clamps", state.y, 1000.0f);
  ok &= expect_close("saved state z", state.z, -3.0f);
  ok &= expect_true("saved state pitch clamps", state.pitch < 1.571f);
  ok &= expect_close("saved state yaw normalizes", state.yaw, 0.0f);
  ok &= expect_true("saved state velocity clears",
                    state.velocity_x == 0.0f && state.velocity_y == 0.0f &&
                        state.velocity_z == 0.0f);
  ok &= expect_true("saved state walk mode", state.control_mode == 0u);
  ok &= expect_true("saved state selected block", state.selected_block == 9u);
  ok &= expect_true("invalid saved state rejects", invalid_result != 0);
  return ok;
}

bool validate_save_state_change_threshold() {
  constexpr OctarynServerPlayerSaveState baseline{
      .x = 1.0f,
      .y = 2.0f,
      .z = 3.0f,
      .pitch = 0.25f,
      .yaw = 0.5f,
      .selected_block = 9u,
      .reserved = 0u};
  OctarynServerPlayerSaveState current = baseline;

  bool ok = true;
  ok &= expect_true(
      "same save state unchanged",
      octaryn_server_player_save_state_changed(&baseline, &current) == 0u);

  current = baseline;
  current.x += 0.01f;
  ok &= expect_true(
      "position threshold inclusive",
      octaryn_server_player_save_state_changed(&baseline, &current) == 0u);
  current.x += 0.001f;
  ok &= expect_true(
      "position threshold exceeded",
      octaryn_server_player_save_state_changed(&baseline, &current) == 1u);

  current = baseline;
  current.yaw += 0.001f;
  ok &= expect_true(
      "angle threshold inclusive",
      octaryn_server_player_save_state_changed(&baseline, &current) == 0u);
  current.yaw += 0.0001f;
  ok &= expect_true(
      "angle threshold exceeded",
      octaryn_server_player_save_state_changed(&baseline, &current) == 1u);

  current = baseline;
  current.selected_block = 10u;
  ok &= expect_true(
      "selected block change persists",
      octaryn_server_player_save_state_changed(&baseline, &current) == 1u);
  ok &= expect_true("null previous persists",
                    octaryn_server_player_save_state_changed(nullptr,
                                                             &current) == 1u);
  ok &= expect_true("null current persists",
                    octaryn_server_player_save_state_changed(&baseline,
                                                             nullptr) == 1u);
  ok &= expect_true("save cadence holds changed state",
                    octaryn_server_player_should_save_state(
                        &baseline, &current, 0.5, 0u) == 0u);
  ok &= expect_true("save cadence releases changed state",
                    octaryn_server_player_should_save_state(
                        &baseline, &current, 1.0, 0u) == 1u);
  ok &= expect_true("save cadence force releases changed state",
                    octaryn_server_player_should_save_state(
                        &baseline, &current, 0.0, 1u) == 1u);
  ok &= expect_true("save cadence ignores unchanged force",
                    octaryn_server_player_should_save_state(
                        &baseline, &baseline, 1.0, 1u) == 0u);
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
  ok &= expect_true("walk applies gravity without contact",
                    state.is_on_ground == 0u);

  state = default_state();
  state.y = octaryn_server_player_spawn_eye_height();
  state.is_on_ground = 1u;
  const auto jump = input(JumpFlag, 0.0f, 0.0f, 0.0f);
  ProbeWorld empty_world;
  const int jump_result = octaryn_server_player_move(&jump, 0.05, query_block,
                                                     &empty_world, &state);
  ok &= expect_true("jump result", jump_result == 0);
  ok &= expect_true("jump leaves ground", state.is_on_ground == 0u);
  ok &= expect_true("jump rises",
                    state.y > octaryn_server_player_spawn_eye_height());
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

bool validate_block_store_wall_collision() {
  BlockStore store;
  for (int32_t y = 0; y <= 3; y++) {
    for (int32_t z = -1; z <= 1; z++) {
      store.set_block(
          BlockEdit{.position = BlockPosition{.x = 1, .y = y, .z = z},
                    .block = WhiteBlock});
    }
  }

  OctarynServerPlayerState state = default_state();
  state.x = 0.6f;
  state.y = octaryn_server_player_spawn_eye_height();
  state.is_on_ground = 1u;
  const auto right = input(0u, 1.0f, 0.0f, 0.0f);
  const int result = octaryn_server_player_move_with_block_store(
      &right, 0.1, &store, nullptr, is_solid_block, nullptr, &state);

  bool ok = true;
  ok &= expect_true("block store wall move result", result == 0);
  ok &= expect_true("block store wall clamps x", state.x < 0.701f);
  ok &=
      expect_close("block store wall stops x velocity", state.velocity_x, 0.0f);
  return ok;
}

bool validate_fly_move() {
  ProbeWorld world;
  auto state = default_state();
  const auto fly =
      input(FlyModeFlag | SprintFlag, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f);
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

bool validate_input_intent() {
  OctarynServerPlayerInput none{};
  OctarynServerPlayerInput movement = input(0u, 0.0f, 0.0f, 1.0f);
  OctarynServerPlayerInput mouse = input(0u, 0.0f, 0.0f, 0.0f);
  mouse.controller = 0u;
  mouse.relative_mouse = 1;

  bool ok = true;
  ok &= expect_true("empty input has no intent",
                    octaryn_server_player_has_input_intent(&none) == 0u);
  ok &= expect_true("null input has no intent",
                    octaryn_server_player_has_input_intent(nullptr) == 0u);
  ok &= expect_true("movement has intent",
                    octaryn_server_player_has_input_intent(&movement) == 1u);
  ok &= expect_true("relative mouse has intent",
                    octaryn_server_player_has_input_intent(&mouse) == 1u);
  return ok;
}

bool validate_input_intent_file() {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() /
      "octaryn_server_player_input_intent_probe.json";
  std::error_code error;
  std::filesystem::remove(output_path, error);

  std::ofstream output{output_path, std::ios::binary | std::ios::trunc};
  output << "{\"version\":1,\"frameIndex\":12,\"deltaSeconds\":0.05,"
            "\"flags\":1,\"controller\":1,\"moveX\":0.25,\"moveY\":0.5,"
            "\"moveZ\":0.75,\"cameraX\":1.0,\"cameraY\":2.0,"
            "\"cameraZ\":3.0,\"cameraPitch\":0.1,\"cameraYaw\":0.2,"
            "\"relativeMouse\":1}\n";
  output.close();

  const std::string output_path_text = output_path.string();
  OctarynServerPlayerInputIntent intent{};
  bool ok = true;
  ok &= expect_true("input intent file read",
                    octaryn_server_player_read_input_intent_file(
                        output_path_text.c_str(), &intent) == 0);
  ok &= expect_true("input intent file frame", intent.frame_index == 12u);
  ok &= expect_close("input intent file delta",
                     static_cast<float>(intent.delta_seconds), 0.05f);
  ok &= expect_true("input intent file flags", intent.input.flags == 1u);
  ok &= expect_close("input intent file move z", intent.input.move_z, 0.75f);
  ok &= expect_true("input intent file relative mouse",
                    intent.input.relative_mouse == 1);

  output.open(output_path, std::ios::binary | std::ios::trunc);
  output << "{\"version\":1,\"frameIndex\":0,\"deltaSeconds\":0.05}\n";
  output.close();
  ok &= expect_true("input intent file rejects unsupported",
                    octaryn_server_player_read_input_intent_file(
                        output_path_text.c_str(), &intent) == -4);

  std::filesystem::remove(output_path, error);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_default_state();
  ok &= validate_saved_state_load();
  ok &= validate_save_state_change_threshold();
  ok &= validate_spawn_alignment();
  ok &= validate_block_store_spawn_alignment();
  ok &= validate_walk_ground_and_jump();
  ok &= validate_wall_collision();
  ok &= validate_block_store_wall_collision();
  ok &= validate_fly_move();
  ok &= validate_idle_update();
  ok &= validate_input_intent();
  ok &= validate_input_intent_file();
  if (!ok) {
    return 1;
  }

  std::puts("server player simulation native probe passed");
  return 0;
}
