#include "PlayerSimulation.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

namespace {

constexpr uint32_t JumpFlag = 1u << 0u;
constexpr uint32_t SprintFlag = 1u << 1u;
constexpr uint32_t FlyModeFlag = 1u << 2u;
constexpr uint32_t WalkMode = 0u;
constexpr uint32_t FlyMode = 1u;

constexpr int32_t WorldMinY = -256;
constexpr int32_t WorldMaxYExclusive = 256;
constexpr uint16_t AirBlock = 0;
constexpr uint32_t SolidBlockFlag = 1u << 16u;

constexpr float DefaultSpawnPitch = -0.35f;
constexpr float SpawnEyeHeight = 2.72f;
constexpr float NormalFlySpeedBlocksPerSecond = 10.0f;
constexpr float SprintFlySpeedBlocksPerSecond = 100.0f;
constexpr float WalkSpeedBlocksPerSecond = 5.0f;
constexpr float SprintWalkSpeedBlocksPerSecond = 9.0f;
constexpr float MaxDeltaSeconds = 0.05f;
constexpr float CollisionStep = 0.1f;
constexpr float CollisionRadius = 0.3f;
constexpr float CollisionHeight = 1.8f;
constexpr float EyeOffset = 1.62f;
constexpr float GroundContactProbe = 0.025f;
constexpr float PhysicsEpsilon = 0.001f;
constexpr float AirAcceleration = 6.0f;
constexpr float Gravity = 24.0f;
constexpr float JumpSpeed = 8.5f;
constexpr float Pi = 3.14159265358979323846f;
constexpr float TwoPi = Pi * 2.0f;

struct Vec3 {
  float x;
  float y;
  float z;
};

float finite_or(float value, float fallback) {
  return std::isfinite(value) ? value : fallback;
}

float clamp_pitch(float pitch) {
  return std::clamp(pitch, -Pi * 0.5f + FLT_EPSILON,
                    Pi * 0.5f - FLT_EPSILON);
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
  return static_cast<float>(std::min(value, static_cast<double>(MaxDeltaSeconds)));
}

int32_t floor_to_int(float value) {
  return static_cast<int32_t>(std::floor(value));
}

uint16_t block_id(uint32_t block_info) {
  return static_cast<uint16_t>(block_info & 0xffffu);
}

bool is_solid_block(uint32_t block_info) {
  return (block_info & SolidBlockFlag) != 0u;
}

uint32_t query_block(octaryn_server_player_block_query_fn block_query,
                     void *context, int32_t x, int32_t y, int32_t z) {
  if (!block_query) {
    return 0u;
  }
  return block_query(context, x, y, z);
}

bool is_colliding(const Vec3 &position,
                  octaryn_server_player_block_query_fn block_query,
                  void *context) {
  const int32_t min_x = floor_to_int(position.x - CollisionRadius + PhysicsEpsilon);
  const int32_t min_y = floor_to_int(position.y - EyeOffset + PhysicsEpsilon);
  const int32_t min_z = floor_to_int(position.z - CollisionRadius + PhysicsEpsilon);
  const int32_t max_x = floor_to_int(position.x + CollisionRadius - PhysicsEpsilon);
  const int32_t max_y =
      floor_to_int(position.y + CollisionHeight - EyeOffset - PhysicsEpsilon);
  const int32_t max_z = floor_to_int(position.z + CollisionRadius - PhysicsEpsilon);

  for (int32_t z = min_z; z <= max_z; z++) {
    for (int32_t y = min_y; y <= max_y; y++) {
      for (int32_t x = min_x; x <= max_x; x++) {
        if (is_solid_block(query_block(block_query, context, x, y, z))) {
          return true;
        }
      }
    }
  }
  return false;
}

void bisect(Vec3 &position, int axis, float step,
            octaryn_server_player_block_query_fn block_query, void *context) {
  const Vec3 start = position;
  float lower = 0.0f;
  float upper = 1.0f;
  for (int index = 0; index < 8; index++) {
    const float t = (lower + upper) * 0.5f;
    Vec3 candidate = start;
    if (axis == 0) {
      candidate.x += step * t;
    } else if (axis == 1) {
      candidate.y += step * t;
    } else {
      candidate.z += step * t;
    }

    if (is_colliding(candidate, block_query, context)) {
      upper = t;
    } else {
      lower = t;
    }
  }

  if (axis == 0) {
    position.x = start.x + step * lower;
  } else if (axis == 1) {
    position.y = start.y + step * lower;
  } else {
    position.z = start.z + step * lower;
  }
}

bool move_axis(Vec3 &position, int axis, float delta,
               octaryn_server_player_block_query_fn block_query, void *context) {
  if (std::fabs(delta) <= FLT_EPSILON) {
    return false;
  }

  const int steps =
      std::max(1, static_cast<int>(std::ceil(std::fabs(delta) / CollisionStep)));
  const float step = delta / static_cast<float>(steps);
  for (int index = 0; index < steps; index++) {
    Vec3 candidate = position;
    if (axis == 0) {
      candidate.x += step;
    } else if (axis == 1) {
      candidate.y += step;
    } else {
      candidate.z += step;
    }

    if (is_colliding(candidate, block_query, context)) {
      bisect(position, axis, step, block_query, context);
      return true;
    }

    position = candidate;
  }
  return false;
}

bool has_ground_contact(const Vec3 &position,
                        octaryn_server_player_block_query_fn block_query,
                        void *context) {
  return is_colliding(Vec3{position.x, position.y - GroundContactProbe, position.z},
                      block_query, context);
}

Vec3 move_yaw_relative(float x, float z, float yaw) {
  const float yaw_sine = std::sin(yaw);
  const float yaw_cosine = std::cos(yaw);
  return Vec3{yaw_cosine * x + yaw_sine * z, 0.0f,
              -(yaw_cosine * z) + yaw_sine * x};
}

Vec3 move_camera_relative(float x, float y, float z, float pitch, float yaw) {
  const float yaw_sine = std::sin(yaw);
  const float yaw_cosine = std::cos(yaw);
  const float pitch_sine = std::sin(pitch);
  const float pitch_cosine = std::cos(pitch);
  return Vec3{pitch_cosine * (yaw_sine * z) + yaw_cosine * x,
              y + z * pitch_sine,
              -(pitch_cosine * (yaw_cosine * z) - yaw_sine * x)};
}

void move_fly(const OctarynServerPlayerInput &input, float dt,
              OctarynServerPlayerState &state, float pitch, float yaw) {
  const float speed =
      (input.flags & SprintFlag) != 0u ? SprintFlySpeedBlocksPerSecond
                                       : NormalFlySpeedBlocksPerSecond;
  const float distance = speed * dt;
  const Vec3 move = move_camera_relative(input.move_x * distance,
                                         input.move_y * distance,
                                         input.move_z * distance, pitch, yaw);
  state.x += move.x;
  state.y = std::clamp(state.y + move.y, -1000.0f, 1000.0f);
  state.z += move.z;
  state.pitch = clamp_pitch(pitch);
  state.yaw = yaw;
  state.velocity_x = dt > 0.0f ? move.x / dt : 0.0f;
  state.velocity_y = dt > 0.0f ? move.y / dt : 0.0f;
  state.velocity_z = dt > 0.0f ? move.z / dt : 0.0f;
  state.is_on_ground = 0u;
  state.control_mode = FlyMode;
}

void move_walk(const OctarynServerPlayerInput &input, float dt,
               OctarynServerPlayerState &state, float pitch, float yaw,
               octaryn_server_player_block_query_fn block_query, void *context) {
  const float speed = (input.flags & SprintFlag) != 0u
                          ? SprintWalkSpeedBlocksPerSecond
                          : WalkSpeedBlocksPerSecond;
  const Vec3 target = move_yaw_relative(input.move_x * speed, input.move_z * speed, yaw);
  float velocity_x = state.velocity_x;
  float velocity_y = state.velocity_y;
  float velocity_z = state.velocity_z;
  bool is_on_ground = state.is_on_ground != 0u;
  if (is_on_ground) {
    velocity_x = target.x;
    velocity_z = target.z;
  } else {
    const float blend = std::min(1.0f, AirAcceleration * dt);
    velocity_x += (target.x - velocity_x) * blend;
    velocity_z += (target.z - velocity_z) * blend;
  }

  const bool jump_requested = (input.flags & JumpFlag) != 0u && is_on_ground;
  if (jump_requested) {
    velocity_y = JumpSpeed;
    is_on_ground = false;
  } else if (is_on_ground && velocity_y < 0.0f) {
    velocity_y = 0.0f;
  }

  Vec3 position{state.x, state.y, state.z};
  const bool hit_x = move_axis(position, 0, velocity_x * dt, block_query, context);
  const bool hit_z = move_axis(position, 2, velocity_z * dt, block_query, context);
  if (hit_x) {
    velocity_x = 0.0f;
  }
  if (hit_z) {
    velocity_z = 0.0f;
  }

  if (!jump_requested && is_on_ground &&
      has_ground_contact(position, block_query, context)) {
    velocity_y = 0.0f;
  } else {
    is_on_ground = false;
    velocity_y -= Gravity * dt;
    const bool hit_y =
        move_axis(position, 1, velocity_y * dt, block_query, context);
    if (hit_y) {
      is_on_ground = velocity_y < 0.0f;
      velocity_y = 0.0f;
    }
  }

  state.x = position.x;
  state.y = position.y;
  state.z = position.z;
  state.pitch = pitch;
  state.yaw = yaw;
  state.velocity_x = velocity_x;
  state.velocity_y = velocity_y;
  state.velocity_z = velocity_z;
  state.is_on_ground = is_on_ground ? 1u : 0u;
  state.control_mode = WalkMode;
}

} // namespace

extern "C" {

float octaryn_server_player_spawn_eye_height() { return SpawnEyeHeight; }

int octaryn_server_player_align_spawn(
    OctarynServerPlayerState *state, uint32_t loaded_from_save,
    octaryn_server_player_block_query_fn block_query, void *context,
    OctarynServerPlayerSpawnAlignment *alignment) {
  if (!state || !block_query || !alignment) {
    return -1;
  }

  alignment->aligned = 0u;
  alignment->surface_y = 0;
  alignment->surface_block = AirBlock;
  const int32_t column_x = floor_to_int(state->x);
  const int32_t column_z = floor_to_int(state->z);
  for (int32_t y = WorldMaxYExclusive - 1; y >= WorldMinY; y--) {
    const uint32_t block_info = query_block(block_query, context, column_x, y, column_z);
    if (!is_solid_block(block_info)) {
      continue;
    }

    const float desired_eye_y = static_cast<float>(y) + SpawnEyeHeight;
    const bool should_adjust = loaded_from_save == 0u || state->y < desired_eye_y ||
                               state->y > desired_eye_y + 24.0f;
    if (should_adjust) {
      state->y = desired_eye_y;
      state->velocity_x = 0.0f;
      state->velocity_y = 0.0f;
      state->velocity_z = 0.0f;
      state->is_on_ground = 0u;
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

int octaryn_server_player_move(const OctarynServerPlayerInput *input,
                               double delta_seconds,
                               octaryn_server_player_block_query_fn block_query,
                               void *context,
                               OctarynServerPlayerState *state) {
  if (!input || !state || !block_query) {
    return -1;
  }

  const float pitch = clamp_pitch(finite_or(input->camera_pitch, state->pitch));
  const float yaw = normalize_yaw(finite_or(input->camera_yaw, state->yaw));
  const float dt = clamp_delta_seconds(delta_seconds);
  if ((input->flags & FlyModeFlag) != 0u) {
    move_fly(*input, dt, *state, pitch, yaw);
  } else {
    move_walk(*input, dt, *state, pitch, yaw, block_query, context);
  }
  return 0;
}
}

