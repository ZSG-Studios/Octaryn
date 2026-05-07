#include "PlayerSimulation.h"
#include "BlockStore.h"
#include "PlayerMovement.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {

using octaryn::server::world::blocks::BlockPosition;
using octaryn::server::world::blocks::BlockStore;

constexpr uint32_t FlyModeFlag = 1u << 2u;
constexpr uint32_t WalkMode = 0u;
constexpr uint32_t FlyMode = 1u;

constexpr int32_t WorldMinY = -256;
constexpr int32_t WorldMaxYExclusive = 256;
constexpr uint16_t AirBlock = 0;
constexpr uint32_t SolidBlockFlag = 1u << 16u;

constexpr float DefaultSpawnY = 80.0f;
constexpr float DefaultSpawnPitch = -0.35f;
constexpr uint16_t DefaultSelectedBlock = 25u;
constexpr float SpawnEyeHeight = 2.72f;
constexpr float MaxDeltaSeconds = 0.05f;
constexpr float Pi = 3.14159265358979323846f;
constexpr float TwoPi = Pi * 2.0f;

float finite_or(float value, float fallback) {
  return std::isfinite(value) ? value : fallback;
}

float clamp_pitch(float pitch) {
  return std::clamp(pitch, -Pi * 0.5f + FLT_EPSILON, Pi * 0.5f - FLT_EPSILON);
}

float normalize_yaw(float yaw) {
  yaw = std::fmod(yaw + Pi, TwoPi);
  if (yaw < 0.0f) {
    yaw += TwoPi;
  }
  return yaw - Pi;
}

float clamp_delta_seconds(double value) {
  if (!std::isfinite(value) || value <= 0.0) {
    return 0.0f;
  }
  return static_cast<float>(
      std::min(value, static_cast<double>(MaxDeltaSeconds)));
}

int32_t floor_to_int(float value) {
  return static_cast<int32_t>(std::floor(value));
}

uint16_t block_id(uint32_t block_info) {
  return static_cast<uint16_t>(block_info & 0xffffu);
}

bool is_solid_block_info(uint32_t block_info) {
  return (block_info & SolidBlockFlag) != 0u;
}

uint32_t query_block(octaryn_server_player_block_query_fn block_query,
                     void *context, int32_t x, int32_t y, int32_t z) {
  if (!block_query) {
    return 0u;
  }
  return block_query(context, x, y, z);
}

struct BlockStoreQueryContext {
  BlockStore *store;
  octaryn_server_player_generated_block_fn generated_block;
  octaryn_server_player_block_solid_fn is_solid_block;
  void *callback_context;
};

uint32_t query_block_store(void *context, int32_t x, int32_t y, int32_t z) {
  auto *query_context = static_cast<BlockStoreQueryContext *>(context);
  if (!query_context || !query_context->store ||
      !query_context->is_solid_block) {
    return 0u;
  }

  const BlockPosition position{.x = x, .y = y, .z = z};
  uint16_t block = AirBlock;
  if (!query_context->store->try_get_block(position, block) &&
      query_context->generated_block) {
    block = query_context->generated_block(query_context->callback_context, x,
                                           y, z);
  }

  uint32_t info = static_cast<uint32_t>(block);
  if (query_context->is_solid_block(query_context->callback_context, block) !=
      0u) {
    info |= SolidBlockFlag;
  }
  return info;
}

} // namespace

extern "C" {

float octaryn_server_player_spawn_eye_height() { return SpawnEyeHeight; }

const char *octaryn_server_player_control_mode_name(uint32_t mode) {
  return mode == FlyMode ? "fly" : "walk";
}

uint32_t octaryn_server_player_control_mode_is_fly(uint32_t mode) {
  return mode == FlyMode ? 1u : 0u;
}

int octaryn_server_player_default_state(OctarynServerPlayerState *state) {
  if (!state) {
    return -1;
  }

  state->x = 0.0f;
  state->y = DefaultSpawnY;
  state->z = 0.0f;
  state->pitch = DefaultSpawnPitch;
  state->yaw = 0.0f;
  state->velocity_x = 0.0f;
  state->velocity_y = 0.0f;
  state->velocity_z = 0.0f;
  state->is_on_ground = 0u;
  state->control_mode = WalkMode;
  state->selected_block = DefaultSelectedBlock;
  state->reserved = 0u;
  return 0;
}

int octaryn_server_player_align_spawn(
    OctarynServerPlayerState *state, uint32_t loaded_from_save,
    octaryn_server_player_block_query_fn block_query, void *context,
    OctarynServerPlayerSpawnAlignment *alignment) {
  if (!state || !block_query || !alignment) {
    return -1;
  }

  alignment->aligned = 0u;
  alignment->adjusted = 0u;
  alignment->surface_y = 0;
  alignment->surface_block = AirBlock;
  const int32_t column_x = floor_to_int(state->x);
  const int32_t column_z = floor_to_int(state->z);
  for (int32_t y = WorldMaxYExclusive - 1; y >= WorldMinY; y--) {
    const uint32_t block_info =
        query_block(block_query, context, column_x, y, column_z);
    if (!is_solid_block_info(block_info)) {
      continue;
    }

    const float desired_eye_y = static_cast<float>(y) + SpawnEyeHeight;
    const bool should_adjust = loaded_from_save == 0u ||
                               state->y < desired_eye_y ||
                               state->y > desired_eye_y + 24.0f;
    if (should_adjust) {
      state->y = desired_eye_y;
      state->velocity_x = 0.0f;
      state->velocity_y = 0.0f;
      state->velocity_z = 0.0f;
      state->is_on_ground = 0u;
      alignment->adjusted = 1u;
    }
    if (loaded_from_save == 0u) {
      state->pitch = DefaultSpawnPitch;
    }

    alignment->aligned = 1u;
    alignment->surface_y = y;
    alignment->surface_block = block_id(block_info);
    return 0;
  }

  return 0;
}

int octaryn_server_player_align_spawn_with_block_store(
    OctarynServerPlayerState *state, uint32_t loaded_from_save,
    void *block_store, octaryn_server_player_generated_block_fn generated_block,
    octaryn_server_player_block_solid_fn is_solid_block, void *context,
    OctarynServerPlayerSpawnAlignment *alignment) {
  if (!block_store || !is_solid_block) {
    return -1;
  }

  BlockStoreQueryContext query_context{
      .store = static_cast<BlockStore *>(block_store),
      .generated_block = generated_block,
      .is_solid_block = is_solid_block,
      .callback_context = context};
  return octaryn_server_player_align_spawn(
      state, loaded_from_save, query_block_store, &query_context, alignment);
}

int octaryn_server_player_move(const OctarynServerPlayerInput *input,
                               double delta_seconds,
                               octaryn_server_player_block_query_fn block_query,
                               void *context, OctarynServerPlayerState *state) {
  if (!input || !state || !block_query) {
    return -1;
  }

  const float pitch = clamp_pitch(finite_or(input->camera_pitch, state->pitch));
  const float yaw = normalize_yaw(finite_or(input->camera_yaw, state->yaw));
  const float dt = clamp_delta_seconds(delta_seconds);
  if ((input->flags & FlyModeFlag) != 0u) {
    octaryn::server::simulation::players::move_fly(*input, dt, *state, pitch,
                                                   yaw);
  } else {
    octaryn::server::simulation::players::move_walk(*input, dt, *state, pitch,
                                                    yaw, block_query, context);
  }
  return 0;
}

int octaryn_server_player_move_with_block_store(
    const OctarynServerPlayerInput *input, double delta_seconds,
    void *block_store, octaryn_server_player_generated_block_fn generated_block,
    octaryn_server_player_block_solid_fn is_solid_block, void *context,
    OctarynServerPlayerState *state) {
  if (!block_store || !is_solid_block) {
    return -1;
  }

  BlockStoreQueryContext query_context{
      .store = static_cast<BlockStore *>(block_store),
      .generated_block = generated_block,
      .is_solid_block = is_solid_block,
      .callback_context = context};
  return octaryn_server_player_move(input, delta_seconds, query_block_store,
                                    &query_context, state);
}

int octaryn_server_player_step(const OctarynServerPlayerInput *input,
                               double delta_seconds,
                               octaryn_server_player_block_query_fn block_query,
                               void *context, OctarynServerPlayerState *state,
                               OctarynServerPlayerTickResult *result) {
  if (!input || !state || !result) {
    return -1;
  }

  const float previous_x = state->x;
  const float previous_y = state->y;
  const float previous_z = state->z;
  result->tick_input = octaryn_server_player_has_input_intent(input);
  result->reserved = 0u;
  result->delta_x = 0.0f;
  result->delta_y = 0.0f;
  result->delta_z = 0.0f;
  if (result->tick_input == 0u) {
    return octaryn_server_player_idle(state);
  }

  const int move_result =
      octaryn_server_player_move(input, delta_seconds, block_query, context,
                                 state);
  if (move_result == 0) {
    result->delta_x = state->x - previous_x;
    result->delta_y = state->y - previous_y;
    result->delta_z = state->z - previous_z;
  }
  return move_result;
}

int octaryn_server_player_step_with_block_store(
    const OctarynServerPlayerInput *input, double delta_seconds,
    void *block_store, octaryn_server_player_generated_block_fn generated_block,
    octaryn_server_player_block_solid_fn is_solid_block, void *context,
    OctarynServerPlayerState *state, OctarynServerPlayerTickResult *result) {
  if (!block_store || !is_solid_block) {
    return -1;
  }

  BlockStoreQueryContext query_context{
      .store = static_cast<BlockStore *>(block_store),
      .generated_block = generated_block,
      .is_solid_block = is_solid_block,
      .callback_context = context};
  return octaryn_server_player_step(input, delta_seconds, query_block_store,
                                    &query_context, state, result);
}

int octaryn_server_player_idle(OctarynServerPlayerState *state) {
  if (!state) {
    return -1;
  }

  state->velocity_x = 0.0f;
  state->velocity_y = 0.0f;
  state->velocity_z = 0.0f;
  return 0;
}
}
