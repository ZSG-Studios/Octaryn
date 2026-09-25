#include "BlockStore.h"
#include "PlayerSimulation.h"

#include <cmath>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstdio>
#include <string_view>
#include <unordered_set>

namespace {

constexpr uint32_t JumpFlag = 1u << 0u;
constexpr uint32_t SprintFlag = 1u << 1u;
constexpr uint32_t FlyModeFlag = 1u << 2u;
constexpr uint32_t SolidBlockFlag = 1u << 16u;
constexpr uint16_t WhiteBlock = 1u;

using octaryn::server::world::blocks::BlockEdit;
using octaryn::server::world::blocks::BlockPosition;
using octaryn::server::world::blocks::BlockStore;

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
                                  .jump_held = 0u};
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

uint32_t query_block(void *context, int32_t x, int32_t y, int32_t z) {
  const auto *world = static_cast<const ProbeWorld *>(context);
  if (!world || !world->solids.contains(BlockKey{.x = x, .y = y, .z = z})) {
    return 0u;
  }

  return static_cast<uint32_t>(WhiteBlock) | SolidBlockFlag;
}

uint32_t is_solid_block(void *, uint16_t block) {
  return block == WhiteBlock ? 1u : 0u;
}

} // namespace

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
  ok &= expect_close("walk forward z", state.z, -0.25f, 0.02f);
  ok &= expect_close("walk velocity z", state.velocity_z, -5.0f, 0.25f);
  ok &= expect_true("walk mode", state.control_mode == 0u);
  ok &= expect_true("walk stays grounded", state.is_on_ground == 1u);

  state = default_state();
  state.y = octaryn_server_player_spawn_eye_height();
  state.is_on_ground = 1u;
  const auto jump = input(JumpFlag, 0.0f, 0.0f, 0.0f);
  const int jump_result =
      octaryn_server_player_move(&jump, 0.05, query_block, &world, &state);
  ok &= expect_true("jump result", jump_result == 0);
  ok &= expect_true("jump leaves ground", state.is_on_ground == 0u);
  ok &= expect_true("jump rises",
                    state.y > octaryn_server_player_spawn_eye_height());
  ok &= expect_close("jump velocity y", state.velocity_y, 6.8f);
  const float open_jump_y = state.y;
  const float open_jump_velocity_y = state.velocity_y;

  ProbeWorld wall_world = world;
  for (int32_t y = 1; y <= 3; y++) {
    wall_world.solids.insert(BlockKey{.x = 1, .y = y, .z = 0});
  }
  state = default_state();
  state.x = 0.69f;
  state.y = octaryn_server_player_spawn_eye_height();
  state.is_on_ground = 1u;
  const int wall_jump_result =
      octaryn_server_player_move(&jump, 0.05, query_block, &wall_world, &state);
  ok &= expect_true("wall-adjacent jump result", wall_jump_result == 0);
  ok &= expect_close("wall-adjacent jump height", state.y, open_jump_y, 0.001f);
  ok &= expect_close("wall-adjacent jump velocity y", state.velocity_y,
                     open_jump_velocity_y, 0.001f);

  const float first_jump_velocity_y = state.velocity_y;
  const int repeated_jump_result =
      octaryn_server_player_move(&jump, 0.05, query_block, &wall_world, &state);
  ok &= expect_true("repeated jump result", repeated_jump_result == 0);
  ok &= expect_true("repeated jump does not reset velocity",
                    state.velocity_y < first_jump_velocity_y);
  ok &= expect_true("repeated jump remains airborne", state.is_on_ground == 0u);

  world.solids.insert(BlockKey{.x = 0, .y = 1, .z = 0});
  state = default_state();
  state.y = octaryn_server_player_spawn_eye_height();
  state.is_on_ground = 1u;
  const int placed_jump_result =
      octaryn_server_player_move(&jump, 0.05, query_block, &world, &state);
  ok &= expect_true("placed support jump result", placed_jump_result == 0);
  ok &= expect_true("placed support does not launch",
                    state.velocity_y <= 6.81f);
  return ok;
}

bool validate_walk_leaves_ground_without_support() {
  ProbeWorld world;
  for (int32_t x = -4; x <= 0; x++) {
    for (int32_t z = -4; z <= 4; z++) {
      world.solids.insert(BlockKey{.x = x, .y = 0, .z = z});
    }
  }

  OctarynServerPlayerState state = default_state();
  state.x = 1.31f;
  state.y = octaryn_server_player_spawn_eye_height();
  state.is_on_ground = 1u;
  const auto right = input(0u, 1.0f, 0.0f, 0.0f);
  const int result =
      octaryn_server_player_move(&right, 0.1, query_block, &world, &state);

  bool ok = true;
  ok &= expect_true("edge walk result", result == 0);
  ok &= expect_true("edge walk leaves support", state.x > 1.5f);
  ok &= expect_true("edge walk leaves ground", state.is_on_ground == 0u);
  ok &= expect_true("edge walk starts falling", state.velocity_y < 0.0f);
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
  ok &= expect_close("wall stops x velocity", state.velocity_x, 0.0f, 0.05f);
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
  ok &= expect_close("block store wall stops x velocity", state.velocity_x,
                     0.0f, 0.05f);
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

bool validate_movement_timing() {
  using Clock = std::chrono::steady_clock;
  constexpr double dt = 1.0 / 60.0;
  constexpr int warmup = 120, measured = 600;
  const auto floor = [](void* context, int32_t, int32_t y, int32_t) -> uint32_t {
    ++*static_cast<uint64_t*>(context);
    return y <= 0 ? WhiteBlock | SolidBlockFlag : 0u;
  };
  bool ok = true;
  for (int scenario = 0; scenario < 3; ++scenario) {
    const char* name = scenario == 0 ? "idle" : scenario == 1 ? "walk" : "sprint";
    auto state = default_state();
    state.y = octaryn_server_player_spawn_eye_height();
    state.is_on_ground = 1u;
    const auto movement = input(scenario == 2 ? SprintFlag : 0u,
        0.0f, 0.0f, scenario == 0 ? 0.0f : 1.0f, 0.0f, 0.0f);
    uint64_t queries = 0;
    std::vector<double> timings;
    timings.reserve(measured);
    float start_z{}, start_y{};
    for (int step = -warmup; step < measured; ++step) {
      if (step == 0) { start_z = state.z; start_y = state.y; queries = 0; }
      const auto begin = Clock::now();
      const auto result = octaryn_server_player_move(&movement, dt, floor, &queries, &state);
      const auto end = Clock::now();
      if (step >= 0)
        timings.push_back(std::chrono::duration<double, std::milli>(end - begin).count());
      if (result != 0 || !std::isfinite(state.x) || !std::isfinite(state.y) ||
          !std::isfinite(state.z) || (step >= 0 && state.is_on_ground == 0u)) {
        std::fprintf(stderr, "native movement timing %s invalid result/contact at step %d\n", name, step);
        return false;
      }
    }
    const float speed = scenario == 0 ? 0.0f : scenario == 1 ? 5.0f : 9.0f;
    ok &= expect_close("timed movement exact ground speed", start_z-state.z,
        speed * static_cast<float>(measured * dt), 0.03f);
    ok &= expect_close("timed movement stable floor height", state.y, start_y, 0.002f);
    ok &= expect_close("timed movement no lateral drift", state.x, 0.0f, 0.002f);
    ok &= expect_close("timed movement grounded vertical velocity", state.velocity_y, 0.0f, 0.002f);
    std::sort(timings.begin(), timings.end());
    double sum = 0;
    for (const auto value : timings) sum += value;
    std::printf("native_player_timing scenario=%s warmup=%d steps=%d dt=%.9f mean_ms=%.6f p50_ms=%.6f p95_ms=%.6f p99_ms=%.6f max_ms=%.6f queries_per_step=%.2f distance=%.4f grounded=%u\n",
        name, warmup, measured, dt, sum/measured, timings[measured/2],
        timings[measured*95/100], timings[measured*99/100], timings.back(),
        static_cast<double>(queries)/measured, start_z-state.z, state.is_on_ground);
  }
  return ok;
}

bool validate_voxel_seam_movement() {
  const auto floor = [](void*, int32_t, int32_t y, int32_t) -> uint32_t {
    return y <= 0 ? WhiteBlock | SolidBlockFlag : 0u;
  };
  constexpr float directions[][2]{{1,0},{-1,0},{0,1},{0,-1},
                                  {1,1},{1,-1},{-1,1},{-1,-1}};
  constexpr double variable_dt[]{1.0/120, 1.0/30, .017, .009};
  bool ok = true;
  unsigned cases = 0;
  for (const auto& direction : directions) for (int cadence = 0; cadence < 4; ++cadence)
    for (const bool sprint : {false, true}) {
      auto state = default_state();
      state.x = -.375f; state.z = .625f;
      state.y = octaryn_server_player_spawn_eye_height(); state.is_on_ground = 1;
      const auto movement = input(sprint ? SprintFlag : 0u,
          direction[0], 0, direction[1], 0, 0);
      float start_x{}, start_y{}, start_z{}, maximum_height_error{};
      double elapsed = 0;
      for (int step = 0; step < 420; ++step) {
        const double dt = cadence == 3 ? variable_dt[step % 4]
            : 1.0 / (cadence == 0 ? 30 : cadence == 1 ? 60 : 120);
        if (step == 120) { start_x=state.x; start_y=state.y; start_z=state.z; }
        const int result = octaryn_server_player_move(&movement, dt, floor, nullptr, &state);
        if (result != 0 || !state.is_on_ground || !std::isfinite(state.x) ||
            !std::isfinite(state.y) || !std::isfinite(state.z)) {
          std::fprintf(stderr, "voxel seam movement lost contact cadence=%d step=%d\n", cadence, step);
          return false;
        }
        if (step >= 120) {
          elapsed += dt;
          maximum_height_error = std::max(maximum_height_error, std::abs(state.y-start_y));
        }
      }
      const float scale = (sprint ? 9.0f : 5.0f) * static_cast<float>(elapsed) /
          std::hypot(direction[0], direction[1]);
      const bool matches = std::abs(state.x-start_x-direction[0]*scale) <= .03f &&
          std::abs(state.z-start_z+direction[1]*scale) <= .03f && maximum_height_error <= .002f;
      if (!matches) std::fprintf(stderr,
          "voxel seam movement direction=(%.0f,%.0f) cadence=%d sprint=%d actual=(%.6f,%.6f) expected=(%.6f,%.6f) height_error=%.6f\n",
          direction[0], direction[1], cadence, sprint, state.x-start_x, state.z-start_z,
          direction[0]*scale, -direction[1]*scale, maximum_height_error);
      ok &= matches;
      ++cases;
    }
  std::printf("native_player_voxel_seams cases=%u directions=8 cadences=30,60,120,variable max_height_tolerance=.002 exact_speed_tolerance=.03 passed=%u\n",
      cases, ok ? 1u : 0u);
  const auto room = [](void*, int32_t x, int32_t y, int32_t z) -> uint32_t {
    return y <= 0 || y >= 4 || x >= 3 || z <= -4 ? WhiteBlock | SolidBlockFlag : 0u;
  };
  for (const int hz : {30,60,120}) {
    auto state = default_state();
    state.y = octaryn_server_player_spawn_eye_height(); state.is_on_ground = 1;
    const auto diagonal = input(SprintFlag,1,0,1,0,0);
    for (int step=0; step<hz*2; ++step) {
      ok &= octaryn_server_player_move(&diagonal,1.0/hz,room,nullptr,&state)==0;
      ok &= state.x<=2.701f && state.z>=-2.701f && state.y<=3.821f;
    }
    ok &= expect_close("compound room corner x",state.x,2.69f,.025f);
    ok &= expect_close("compound room corner z",state.z,-2.69f,.025f);
    const auto jump = input(JumpFlag,0,0,0,0,0);
    float max_y=state.y;
    for (int step=0; step<hz*2; ++step) {
      ok &= octaryn_server_player_move(&jump,1.0/hz,room,nullptr,&state)==0;
      max_y=std::max(max_y,state.y);
      ok &= expect_true("compound room ceiling prevents penetration",state.y<=3.821f);
    }
    ok &= expect_true("compound room jump reaches ceiling",max_y>3.7f);
    ok &= expect_true("compound room held jump lands once",state.is_on_ground!=0);
  }
  return ok;
}
