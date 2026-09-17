#include "PlayerJoltMovement.h"
#include "PlayerJoltWorld.h"

// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
// clang-format on

#include <algorithm>
#include <cmath>

JPH_SUPPRESS_WARNINGS

namespace {

constexpr uint32_t JumpFlag = 1u << 0u;
constexpr uint32_t SprintFlag = 1u << 1u;
constexpr uint32_t WalkMode = 0u;

constexpr float WalkSpeedBlocksPerSecond = 5.0f;
constexpr float SprintWalkSpeedBlocksPerSecond = 9.0f;
constexpr float JumpSpeed = 8.0f;
constexpr float AirAcceleration = 6.0f;

float horizontal_length(float x, float z) {
  return std::sqrt(x * x + z * z);
}

octaryn::server::simulation::players::Vec3 move_yaw_relative(float x, float z,
                                                              float yaw) {
  const float yaw_sine = std::sin(yaw);
  const float yaw_cosine = std::cos(yaw);
  return octaryn::server::simulation::players::Vec3{
      yaw_cosine * x + yaw_sine * z, 0.0f,
      -(yaw_cosine * z) + yaw_sine * x};
}

} // namespace

namespace octaryn::server::simulation::players {

bool move_walk_with_jolt(const OctarynServerPlayerInput &input, float dt,
                         OctarynServerPlayerState &state, float pitch,
                         float yaw,
                         octaryn_server_player_block_query_fn block_query,
                         void *context) {
  if (!block_query || dt <= 0.0f) {
    state.pitch = pitch;
    state.yaw = yaw;
    state.velocity_x = 0.0f;
    state.velocity_y = 0.0f;
    state.velocity_z = 0.0f;
    state.control_mode = WalkMode;
    return true;
  }

  initialize_jolt();

  const Vec3 position{state.x, state.y, state.z};
  const float speed = (input.flags & SprintFlag) != 0u
                          ? SprintWalkSpeedBlocksPerSecond
                          : WalkSpeedBlocksPerSecond;
  const float input_length = horizontal_length(input.move_x, input.move_z);
  const float input_scale = input_length > 1.0f ? 1.0f / input_length : 1.0f;
  const Vec3 horizontal_target =
      move_yaw_relative(input.move_x * input_scale * speed,
                        input.move_z * input_scale * speed, yaw);
  const Vec3 scan_target{
      state.x + horizontal_target.x * dt,
      state.y + (state.velocity_y - Gravity * dt) * dt,
      state.z + horizontal_target.z * dt};

  BroadPhaseLayerMap broad_phase_layers;
  ObjectVsBroadPhaseFilter broad_phase_filter;
  ObjectLayerPairFilter layer_pair_filter;
  JPH::PhysicsSystem system;
  system.Init(2048, 0, 2048, 2048, broad_phase_layers, broad_phase_filter,
              layer_pair_filter);

  add_collision_blocks(system, block_query, context, position, scan_target, dt);
  system.OptimizeBroadPhase();

  JPH::TempAllocatorImpl allocator(1024 * 1024);
  JPH::Ref<JPH::CharacterVirtualSettings> settings =
      new JPH::CharacterVirtualSettings();
  // Sharp character box: a convex bevel rests deeper in floor seams than the
  // body checks tolerate (sustained freeze) and sneaks sub-threshold ceiling
  // pokes. Wall-slide jumps are handled by the axis-separated penetration
  // resolve instead. Terrain boxes stay sharp (see PlayerJoltWorld).
  settings->mShape = new JPH::BoxShape(
      JPH::Vec3(CollisionRadius, CollisionHalfHeight, CollisionRadius), 0.0f);
  settings->mShapeOffset = JPH::Vec3(0.0f, CollisionHalfHeight, 0.0f);
  settings->mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
  settings->mSupportingVolume =
      JPH::Plane(JPH::Vec3::sAxisY(), -CollisionRadius);
  settings->mEnhancedInternalEdgeRemoval = true;
  settings->mPredictiveContactDistance = 0.03f;
  settings->mCharacterPadding = 0.01f;
  settings->mPenetrationRecoverySpeed = 1.0f;

  const JPH::RVec3 base_position(state.x, state.y - EyeOffset, state.z);
  JPH::CharacterVirtual character(settings, base_position,
                                  JPH::Quat::sIdentity(), &system);
  character.RefreshContacts(
      system.GetDefaultBroadPhaseLayerFilter(Layers::Character),
      system.GetDefaultLayerFilter(Layers::Character), {}, {}, allocator);
  character.UpdateGroundVelocity();

  const bool was_grounded = state.velocity_y <= 0.1f &&
      (is_grounded(character) || state.is_on_ground != 0u);
  float velocity_x =
      was_grounded
          ? horizontal_target.x
          : state.velocity_x + (horizontal_target.x - state.velocity_x) *
                                   std::min(1.0f, AirAcceleration * dt);
  float velocity_z =
      was_grounded
          ? horizontal_target.z
          : state.velocity_z + (horizontal_target.z - state.velocity_z) *
                                   std::min(1.0f, AirAcceleration * dt);
  float velocity_y = state.velocity_y;
  const bool jump_held = (input.flags & JumpFlag) != 0u;
  const bool jump_requested =
      jump_held && state.jump_held == 0u && was_grounded;
  if (jump_requested) {
    velocity_y = JumpSpeed;
  }
  velocity_y -= Gravity * dt;
  const Vec3 target{state.x + velocity_x * dt, state.y + velocity_y * dt,
                    state.z + velocity_z * dt};
  character.SetLinearVelocity(JPH::Vec3(velocity_x, velocity_y, velocity_z));

  JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
  update_settings.mStickToFloorStepDown = JPH::Vec3(0.0f, -0.25f, 0.0f);
  update_settings.mWalkStairsStepUp = JPH::Vec3::sZero();
  update_settings.mWalkStairsStepDownExtra = JPH::Vec3::sZero();
  character.ExtendedUpdate(
      dt, JPH::Vec3(0.0f, -Gravity, 0.0f), update_settings,
      system.GetDefaultBroadPhaseLayerFilter(Layers::Character),
      system.GetDefaultLayerFilter(Layers::Character), {}, {}, allocator);

  const JPH::RVec3 next_base_position = resolve_body_penetration(
      block_query, context, position, character.GetPosition());
  const JPH::RVec3 next_position(next_base_position.GetX(),
                                 next_base_position.GetY() + EyeOffset,
                                 next_base_position.GetZ());
  const float foot_y = static_cast<float>(base_position.GetY());
  const bool block_x =
      has_blocking_wall_contact(character, 0, velocity_x, foot_y);
  const bool block_z =
      has_blocking_wall_contact(character, 2, velocity_z, foot_y);
  log_physics_diagnostics(input, dt, position, target, next_position,
                          velocity_x, velocity_y, velocity_z, block_x,
                          block_z, was_grounded, character, block_query,
                          context);
  state.x = static_cast<float>(next_position.GetX());
  state.y = static_cast<float>(next_position.GetY());
  state.z = static_cast<float>(next_position.GetZ());
  state.pitch = pitch;
  state.yaw = yaw;
  // A wall-top contact can report support while the character is still rising.
  // Landing must not cancel upward jump momentum at that edge.
  const bool floor_support = has_floor_support(block_query, context, state.x,
                                               state.y - EyeOffset, state.z);
  const bool next_grounded = velocity_y <= 0.0f && floor_support && is_grounded(character);
  const bool keeps_ground =
      was_grounded && !jump_requested && velocity_y <= 0.0f &&
      floor_support;
  const float delta_y = state.y - position.y;
  state.velocity_x = (state.x - position.x) / dt;
  if (next_grounded || keeps_ground) {
    state.velocity_y = 0.0f;
  } else if (!jump_requested && velocity_y <= 0.0f && delta_y > 0.0f) {
    state.velocity_y = std::min(velocity_y, 0.0f);
  } else {
    state.velocity_y = velocity_y;
  }
  state.velocity_z = (state.z - position.z) / dt;
  state.is_on_ground = (next_grounded || keeps_ground) ? 1u : 0u;
  state.control_mode = WalkMode;
  state.jump_held = jump_held ? 1u : 0u;
  return true;
}

} // namespace octaryn::server::simulation::players
