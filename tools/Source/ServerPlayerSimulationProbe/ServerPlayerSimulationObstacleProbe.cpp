#include "BlockStore.h"
#include "PlayerSimulation.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
using octaryn::server::world::blocks::BlockEdit;
using octaryn::server::world::blocks::BlockStore;
uint32_t solid(void*, uint16_t block) { return block != 0; }

bool obstacle_case(const char* name, float z, float strafe, float forward, bool sprint,
 double dt, int wall_height = 1) {
 BlockStore store;
 for (int x = -8; x <= 16; ++x)
 for (int depth = -12; depth <= 12; ++depth)
 store.set_block(BlockEdit{.position = {x, 0, depth}, .block = 1});
 for (int depth = -2; depth <= 2; ++depth)
 for (int y = 1; y <= wall_height; ++y)
 store.set_block(BlockEdit{.position = {1, y, depth}, .block = 1});

 OctarynServerPlayerState state{};
 octaryn_server_player_default_state(&state);
 state.x = 0;
 state.z = z;
 state.y = octaryn_server_player_spawn_eye_height();
 state.is_on_ground = 1;
 OctarynServerPlayerInput input{};
 input.controller = 1;
 input.move_x = 1;
 input.flags = sprint ? 2 : 0;
 auto step = [&] {
 return octaryn_server_player_move_with_block_store(
 &input, dt, &store, nullptr, solid, nullptr, &state) == 0;
 };
 bool ok = true;
 for (int tick = 0; tick < int(std::lround(1.0 / dt)); ++tick) ok &= step();
 const float pressed_x = state.x;
 const float ground_y = state.y;
 const bool pressed_wall = pressed_x > 0.60f && pressed_x < 0.71f;
 input.move_x = strafe;
 input.move_z = forward;
 float apex = ground_y;
 float first_clear = -1;
 float max_backslide = 0;
 unsigned stalled_ascent = 0;
 bool embedded = false;
 bool false_support = false;
 for (int tick = 0; tick < int(std::lround(1.5 / dt)); ++tick) {
 input.flags = (sprint ? 2u : 0u) | (tick * dt < 0.15 ? 1u : 0u);
 const auto before = state;
 ok &= step();
 apex = std::max(apex, state.y);
 max_backslide = std::max(max_backslide, before.x - state.x);
 if (before.velocity_y > 1.0f && state.y <= before.y + 0.00001f) ++stalled_ascent;
 if (state.x > 2.31f && first_clear < 0) first_clear = float((tick + 1) * dt);
 const float feet = state.y - 1.62f;
 if (wall_height > 1 && state.is_on_ground && state.y > ground_y + 0.18f)
 false_support = true;
 embedded |= state.x + 0.3f > 1.002f && state.x - 0.3f < 1.998f &&
 state.z + 0.3f > -1.998f && state.z - 0.3f < 2.998f && feet < float(wall_height) + 0.998f;
 std::printf("obstacle_sample case=%s dt=%.6f tick=%d x=%.5f y=%.5f z=%.5f vy=%.5f grounded=%u\n",
 name, dt, tick, state.x, state.y, state.z, state.velocity_y, state.is_on_ground);
 }
 const bool expected_progress = wall_height == 1 ? first_clear > 0 : state.x < 0.71f;
 ok &= pressed_wall && expected_progress && !embedded && !false_support &&
 stalled_ascent == 0 && apex - ground_y > 1.1f;
 std::printf("obstacle_case=%s dt=%.6f result=%s pressed_x=%.5f apex_delta=%.5f clear_seconds=%.5f "
 "final_x=%.5f final_y=%.5f final_z=%.5f embedded=%d false_support=%d stalled_ascent=%u max_backslide=%.5f\n",
 name, dt, ok ? "passed" : "failed", pressed_x, apex - ground_y, first_clear,
 state.x, state.y, state.z, embedded, false_support, stalled_ascent, max_backslide);
 return ok;
}
}

bool validate_obstacle_jumps() {
 bool ok = true;
 for (double dt : {1.0 / 60.0, 1.0 / 30.0, 1.0 / 120.0}) {
 ok &= obstacle_case("front", 0.5f, 1, 0, false, dt);
 ok &= obstacle_case("seam", 1.0f, 1, 0, false, dt);
 ok &= obstacle_case("oblique", 1.5f, 0.8f, 0.6f, false, dt);
 ok &= obstacle_case("edge", 2.8f, 1, 0, false, dt);
 ok &= obstacle_case("sprint", 0.5f, 1, 0, true, dt);
 ok &= obstacle_case("solid-tall-wall", 1.0f, 1, 0, false, dt, 2);
 }
 std::printf("obstacle_jumps=%s\n", ok ? "passed" : "failed");
 return ok;
}
