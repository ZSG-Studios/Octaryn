#include "PlayerJoltMesh.h"

#include "PlayerJoltWorld.h"
#include "PlayerMovement.h"

// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/PhysicsSystem.h>
// clang-format on

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <utility>

JPH_SUPPRESS_WARNINGS

namespace {

constexpr uint32_t JumpFlag = 1u << 0u;
constexpr uint32_t SprintFlag = 1u << 1u;
constexpr uint32_t FlyFlag = 1u << 2u;
constexpr uint32_t WalkMode = 0u;

constexpr float WalkSpeedBlocksPerSecond = 5.0f;
constexpr float SprintWalkSpeedBlocksPerSecond = 9.0f;
constexpr float JumpSpeed = 8.0f;
constexpr float AirAcceleration = 6.0f;

float horizontal_length(float x, float z) {
  return std::sqrt(x * x + z * z);
}

octaryn::character_motion::Vec3 move_yaw_relative(float x, float z,
                                                  float yaw) {
  const float yaw_sine = std::sin(yaw);
  const float yaw_cosine = std::cos(yaw);
  return octaryn::character_motion::Vec3{
      yaw_cosine * x + yaw_sine * z, 0.0f,
      -(yaw_cosine * z) + yaw_sine * x};
}

struct MeshShapeCacheKey {
  const float *positions;
  const uint32_t *indices;
  bool operator==(const MeshShapeCacheKey &) const = default;
};

struct MeshShapeCacheKeyHash {
  std::size_t operator()(const MeshShapeCacheKey &key) const {
    const auto mix = [](const void *pointer) {
      const auto value = reinterpret_cast<std::uintptr_t>(pointer);
      return std::hash<std::uintptr_t>{}(value >> 4u);
    };
    return mix(key.positions) ^ (mix(key.indices) * 0x9e3779b97f4a7c15ull);
  }
};

JPH::RefConst<JPH::Shape> mesh_collision_shape(
    const octaryn::character_motion::MeshCollision &mesh) {
  // The server steps players from one thread, so the cache is not synchronized.
  // Keys are the soup pointers handed out by the loaded map world; the soup
  // outlives every step against it.
  static std::unordered_map<MeshShapeCacheKey, JPH::RefConst<JPH::Shape>,
                            MeshShapeCacheKeyHash>
      cache;
  const auto [entry, inserted] = cache.try_emplace(
      MeshShapeCacheKey{mesh.positions, mesh.indices});
  if (!inserted) {
    return entry->second;
  }

  const std::size_t vertex_count = mesh.position_count / 3u;
  const std::size_t triangle_count = mesh.index_count / 3u;
  const auto started = std::chrono::steady_clock::now();

  JPH::MeshShapeSettings settings;
  settings.mTriangleVertices.reserve(static_cast<int>(vertex_count));
  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    const float *position = mesh.positions + vertex * 3u;
    settings.mTriangleVertices.push_back(
        JPH::Float3(position[0], position[1], position[2]));
  }
  settings.mIndexedTriangles.reserve(static_cast<int>(triangle_count));
  for (std::size_t triangle = 0; triangle < triangle_count; ++triangle) {
    const uint32_t *indices = mesh.indices + triangle * 3u;
    if (indices[0] >= vertex_count || indices[1] >= vertex_count ||
        indices[2] >= vertex_count) {
      continue;
    }
    settings.mIndexedTriangles.push_back(
        JPH::IndexedTriangle(indices[0], indices[1], indices[2], 0));
  }
  // Real exports contain degenerate triangles; Jolt refuses to build the
  // mesh shape without sanitizing them out first.
  settings.Sanitize();

  const auto shape_result = settings.Create();
  if (shape_result.HasError()) {
    entry->second = nullptr;
    std::fprintf(stderr,
                 "server_map_mesh_shape_build failed triangles=%zu "
                 "error=%s\n",
                 triangle_count, shape_result.GetError().c_str());
    return nullptr;
  }

  entry->second = shape_result.Get();
  const double build_ms =
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - started)
          .count();
  std::fprintf(stderr,
               "server_map_mesh_shape_build vertices=%zu triangles=%zu "
               "ms=%.1f\n",
               vertex_count, triangle_count, build_ms);
  return entry->second;
}

} // namespace

namespace octaryn::character_motion {

void step_on_mesh(const Input &input, float deltaSeconds, State &state,
                  const MeshCollision &mesh) {
  if (!mesh.positions || !mesh.indices || mesh.index_count < 3u) {
    return;
  }
  constexpr float Pi = 3.14159265358979323846f;
  constexpr float TwoPi = Pi * 2.0f;
  const float pitch = std::clamp(
      std::isfinite(input.camera_pitch) ? input.camera_pitch : state.pitch,
      -Pi * 0.5f + FLT_EPSILON, Pi * 0.5f - FLT_EPSILON);
  float yaw = std::isfinite(input.camera_yaw) ? input.camera_yaw : state.yaw;
  yaw = std::fmod(yaw + Pi, TwoPi);
  if (yaw < 0.0f) yaw += TwoPi;
  yaw -= Pi;
  float remaining = std::isfinite(deltaSeconds) && deltaSeconds > 0.0f
      ? std::min(deltaSeconds, 0.25f) : 0.0f;
  if (remaining <= 0.0f) {
    state.pitch = pitch;
    state.yaw = yaw;
    state.velocity_x = state.velocity_y = state.velocity_z = 0.0f;
    return;
  }
  while (remaining > 0.0f) {
    const float dt = std::min(remaining, 0.05f);
    remaining -= dt;
    if ((input.flags & FlyFlag) != 0u) {
      move_fly(input, dt, state, pitch, yaw);
    } else {
      move_walk_on_mesh(input, dt, state, pitch, yaw, mesh);
    }
  }
}

bool move_walk_on_mesh(const Input &input, float dt, State &state, float pitch,
                       float yaw, const MeshCollision &mesh) {
  if (dt <= 0.0f) {
    state.pitch = pitch;
    state.yaw = yaw;
    state.velocity_x = 0.0f;
    state.velocity_y = 0.0f;
    state.velocity_z = 0.0f;
    state.control_mode = WalkMode;
    return true;
  }

  initialize_jolt();
  const JPH::RefConst<JPH::Shape> mesh_shape = mesh_collision_shape(mesh);
  if (!mesh_shape) {
    state.pitch = pitch;
    state.yaw = yaw;
    state.velocity_x = 0.0f;
    state.velocity_y = 0.0f;
    state.velocity_z = 0.0f;
    state.control_mode = WalkMode;
    return true;
  }

  const Vec3 position{state.x, state.y, state.z};
  const float speed = (input.flags & SprintFlag) != 0u
                          ? SprintWalkSpeedBlocksPerSecond
                          : WalkSpeedBlocksPerSecond;
  const float input_length = horizontal_length(input.move_x, input.move_z);
  const float input_scale = input_length > 1.0f ? 1.0f / input_length : 1.0f;
  const Vec3 horizontal_target =
      move_yaw_relative(input.move_x * input_scale * speed,
                        input.move_z * input_scale * speed, yaw);

  BroadPhaseLayerMap broad_phase_layers;
  ObjectVsBroadPhaseFilter broad_phase_filter;
  ObjectLayerPairFilter layer_pair_filter;
  JPH::PhysicsSystem system;
  system.Init(2048, 0, 2048, 2048, broad_phase_layers, broad_phase_filter,
              layer_pair_filter);

  JPH::BodyInterface &bodies = system.GetBodyInterface();
  JPH::BodyCreationSettings mesh_body_settings(
      mesh_shape, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
      JPH::EMotionType::Static, Layers::Static);
  const JPH::BodyID mesh_body =
      bodies.CreateBody(mesh_body_settings)->GetID();
  bodies.AddBody(mesh_body, JPH::EActivation::DontActivate);
  system.OptimizeBroadPhase();

  JPH::TempAllocatorImpl allocator(1024 * 1024);
  JPH::Ref<JPH::CharacterVirtualSettings> settings =
      new JPH::CharacterVirtualSettings();
  // Same character box and feel as the voxel walk; the mesh body provides the
  // exact collision surface.
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
  character.SetLinearVelocity(JPH::Vec3(velocity_x, velocity_y, velocity_z));

  JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
  update_settings.mStickToFloorStepDown = JPH::Vec3(0.0f, -0.25f, 0.0f);
  update_settings.mWalkStairsStepUp = JPH::Vec3::sZero();
  update_settings.mWalkStairsStepDownExtra = JPH::Vec3::sZero();
  character.ExtendedUpdate(
      dt, JPH::Vec3(0.0f, -Gravity, 0.0f), update_settings,
      system.GetDefaultBroadPhaseLayerFilter(Layers::Character),
      system.GetDefaultLayerFilter(Layers::Character), {}, {}, allocator);

  // Mesh collision is exact, so no voxel-style penetration resolve runs.
  const JPH::RVec3 next_base_position = character.GetPosition();
  const JPH::RVec3 next_position(next_base_position.GetX(),
                                 next_base_position.GetY() + EyeOffset,
                                 next_base_position.GetZ());
  const float foot_y = static_cast<float>(base_position.GetY());
  const bool block_x =
      has_blocking_wall_contact(character, 0, velocity_x, foot_y);
  const bool block_z =
      has_blocking_wall_contact(character, 2, velocity_z, foot_y);
  log_physics_diagnostics(input, dt, position,
                          Vec3{position.x + velocity_x * dt,
                               position.y + velocity_y * dt,
                               position.z + velocity_z * dt},
                          next_position, velocity_x, velocity_y, velocity_z,
                          block_x, block_z, was_grounded, character, nullptr,
                          nullptr);
  state.x = static_cast<float>(next_position.GetX());
  state.y = static_cast<float>(next_position.GetY());
  state.z = static_cast<float>(next_position.GetZ());
  state.pitch = pitch;
  state.yaw = yaw;
  // A wall-top contact can report support while the character is still rising.
  // Landing must not cancel upward jump momentum at that edge.
  const bool next_grounded = velocity_y <= 0.0f && is_grounded(character);
  const bool keeps_ground =
      was_grounded && !jump_requested && velocity_y <= 0.0f;
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

} // namespace octaryn::character_motion
