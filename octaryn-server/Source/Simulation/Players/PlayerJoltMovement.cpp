#include "PlayerJoltMovement.h"

// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
// clang-format on

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

JPH_SUPPRESS_WARNINGS

namespace {

constexpr uint32_t JumpFlag = 1u << 0u;
constexpr uint32_t SprintFlag = 1u << 1u;
constexpr uint32_t SolidBlockFlag = 1u << 16u;
constexpr uint32_t WalkMode = 0u;

constexpr float WalkSpeedBlocksPerSecond = 5.0f;
constexpr float SprintWalkSpeedBlocksPerSecond = 9.0f;
constexpr float CollisionRadius = 0.3f;
constexpr float CollisionHeight = 1.8f;
constexpr float CollisionHalfHeight = CollisionHeight * 0.5f;
constexpr float EyeOffset = 1.62f;
constexpr float Gravity = 24.0f;
constexpr float JumpSpeed = 8.0f;
constexpr float AirAcceleration = 6.0f;
constexpr float CollisionScanVerticalPadding = 1.25f;
constexpr int32_t BlockScanPadding = 2;

namespace Layers {
constexpr JPH::ObjectLayer Static = 0;
constexpr JPH::ObjectLayer Character = 1;
constexpr JPH::ObjectLayer Count = 2;
} // namespace Layers

namespace BroadPhaseLayers {
const JPH::BroadPhaseLayer Static{0};
const JPH::BroadPhaseLayer Character{1};
constexpr JPH::uint Count = 2;
} // namespace BroadPhaseLayers

struct Vec3 {
  float x;
  float y;
  float z;
};

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer first,
                     JPH::ObjectLayer second) const override {
    if (first == Layers::Static) {
      return second == Layers::Character;
    }
    if (first == Layers::Character) {
      return true;
    }
    return false;
  }
};

class BroadPhaseLayerMap final : public JPH::BroadPhaseLayerInterface {
public:
  BroadPhaseLayerMap() {
    layers_[Layers::Static] = BroadPhaseLayers::Static;
    layers_[Layers::Character] = BroadPhaseLayers::Character;
  }

  JPH::uint GetNumBroadPhaseLayers() const override {
    return BroadPhaseLayers::Count;
  }

  JPH::BroadPhaseLayer
  GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
    return layers_[layer < Layers::Count ? layer : Layers::Static];
  }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  const char *
  GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
    if (layer == BroadPhaseLayers::Static) {
      return "static";
    }
    if (layer == BroadPhaseLayers::Character) {
      return "character";
    }
    return "invalid";
  }
#endif

private:
  JPH::BroadPhaseLayer layers_[Layers::Count];
};

class ObjectVsBroadPhaseFilter final
    : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer layer,
                     JPH::BroadPhaseLayer broad_phase_layer) const override {
    if (layer == Layers::Static) {
      return broad_phase_layer == BroadPhaseLayers::Character;
    }
    if (layer == Layers::Character) {
      return true;
    }
    return false;
  }
};

void jolt_trace(const char *format, ...) {
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  std::fputc('\n', stderr);
  va_end(args);
}

void initialize_jolt() {
  static std::once_flag once;
  std::call_once(once, [] {
    JPH::RegisterDefaultAllocator();
    JPH::Trace = jolt_trace;
    if (!JPH::Factory::sInstance) {
      JPH::Factory::sInstance = new JPH::Factory();
    }
    JPH::RegisterTypes();
  });
}

int32_t floor_to_int(float value) {
  return static_cast<int32_t>(std::floor(value));
}

uint32_t query_block(octaryn_server_player_block_query_fn block_query,
                     void *context, int32_t x, int32_t y, int32_t z) {
  return block_query ? block_query(context, x, y, z) : 0u;
}

bool is_solid_block_info(uint32_t block_info) {
  return (block_info & SolidBlockFlag) != 0u;
}

Vec3 move_yaw_relative(float x, float z, float yaw) {
  const float yaw_sine = std::sin(yaw);
  const float yaw_cosine = std::cos(yaw);
  return Vec3{yaw_cosine * x + yaw_sine * z, 0.0f,
              -(yaw_cosine * z) + yaw_sine * x};
}

void add_collision_blocks(JPH::PhysicsSystem &system,
                          octaryn_server_player_block_query_fn block_query,
                          void *context, const Vec3 &position,
                          const Vec3 &target, float dt) {
  JPH::BodyInterface &bodies = system.GetBodyInterface();
  JPH::RefConst<JPH::Shape> block_shape =
      new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));

  const float min_x = std::min(position.x, target.x) - CollisionRadius;
  const float max_x = std::max(position.x, target.x) + CollisionRadius;
  const float min_y = std::min(position.y, target.y) - EyeOffset -
                      CollisionScanVerticalPadding - Gravity * dt;
  const float max_y = std::max(position.y, target.y) + CollisionHeight -
                      EyeOffset + CollisionScanVerticalPadding;
  const float min_z = std::min(position.z, target.z) - CollisionRadius;
  const float max_z = std::max(position.z, target.z) + CollisionRadius;

  for (int32_t z = floor_to_int(min_z) - BlockScanPadding;
       z <= floor_to_int(max_z) + BlockScanPadding; z++) {
    for (int32_t y = floor_to_int(min_y) - BlockScanPadding;
         y <= floor_to_int(max_y) + BlockScanPadding; y++) {
      for (int32_t x = floor_to_int(min_x) - BlockScanPadding;
           x <= floor_to_int(max_x) + BlockScanPadding; x++) {
        if (!is_solid_block_info(query_block(block_query, context, x, y, z))) {
          continue;
        }

        const JPH::BodyCreationSettings settings(
            block_shape,
            JPH::RVec3(static_cast<float>(x) + 0.5f,
                       static_cast<float>(y) + 0.5f,
                       static_cast<float>(z) + 0.5f),
            JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::Static);
        bodies.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
      }
    }
  }
}

bool is_grounded(JPH::CharacterVirtual &character) {
  return character.GetGroundState() ==
         JPH::CharacterBase::EGroundState::OnGround;
}

bool has_blocking_wall_contact(const JPH::CharacterVirtual &character, int axis,
                               float velocity, float foot_y) {
  if (std::fabs(velocity) <= 0.0001f) {
    return false;
  }

  for (const JPH::CharacterVirtual::Contact &contact :
       character.GetActiveContacts()) {
    if (!contact.mHadCollision) {
      continue;
    }
    if (static_cast<float>(contact.mPosition.GetY()) <= foot_y + 0.75f) {
      continue;
    }
    const float normal_y = contact.mContactNormal.GetY();
    const float normal = axis == 0 ? contact.mContactNormal.GetX()
                                   : contact.mContactNormal.GetZ();
    if (std::fabs(normal_y) > 0.35f || std::fabs(normal) < 0.5f) {
      continue;
    }
    if (normal * velocity < -0.001f) {
      return true;
    }
  }
  return false;
}

bool body_hits_solid(octaryn_server_player_block_query_fn block_query,
                     void *context, float x, float base_y, float z) {
  constexpr float Skin = 0.02f;
  const int32_t min_x = floor_to_int(x - CollisionRadius + Skin);
  const int32_t max_x = floor_to_int(x + CollisionRadius - Skin);
  const int32_t min_z = floor_to_int(z - CollisionRadius + Skin);
  const int32_t max_z = floor_to_int(z + CollisionRadius - Skin);
  const int32_t min_y = floor_to_int(base_y + Skin);
  const int32_t max_y = floor_to_int(base_y + CollisionHeight - Skin);
  for (int32_t y = min_y; y <= max_y; ++y) {
    if (is_solid_block_info(query_block(block_query, context, min_x, y, min_z)) ||
        is_solid_block_info(query_block(block_query, context, max_x, y, min_z)) ||
        is_solid_block_info(query_block(block_query, context, min_x, y, max_z)) ||
        is_solid_block_info(query_block(block_query, context, max_x, y, max_z))) {
      return true;
    }
  }
  return false;
}

JPH::RVec3 resolve_body_penetration(
    octaryn_server_player_block_query_fn block_query, void *context,
    const Vec3 &previous_eye_position, const JPH::RVec3 &next_base_position) {
  const float next_x = static_cast<float>(next_base_position.GetX());
  const float next_y = static_cast<float>(next_base_position.GetY());
  const float next_z = static_cast<float>(next_base_position.GetZ());
  if (!body_hits_solid(block_query, context, next_x, next_y, next_z)) {
    return next_base_position;
  }
  const float previous_base_y = previous_eye_position.y - EyeOffset;
  if (!body_hits_solid(block_query, context, previous_eye_position.x,
                       previous_base_y, previous_eye_position.z)) {
    return JPH::RVec3(previous_eye_position.x, previous_base_y,
                     previous_eye_position.z);
  }
  if (!body_hits_solid(block_query, context, previous_eye_position.x, next_y,
                       next_z)) {
    return JPH::RVec3(previous_eye_position.x, next_y, next_z);
  }
  if (!body_hits_solid(block_query, context, next_x, next_y,
                       previous_eye_position.z)) {
    return JPH::RVec3(next_x, next_y, previous_eye_position.z);
  }
  for (float y = std::max(previous_base_y, next_y); y < next_y + 3.0f;
       y += 0.125f) {
    if (!body_hits_solid(block_query, context, next_x, y, next_z)) {
      return JPH::RVec3(next_x, y, next_z);
    }
  }
  return JPH::RVec3(previous_eye_position.x, std::max(previous_base_y, next_y),
                   previous_eye_position.z);
}

bool physics_debug_enabled() {
  static const bool enabled = [] {
    const char *value = std::getenv("OCTARYN_SERVER_PLAYER_PHYSICS_DEBUG");
    return value && value[0] != '\0' && std::strcmp(value, "0") != 0;
  }();
  return enabled;
}

float horizontal_length(float x, float z) {
  return std::sqrt(x * x + z * z);
}

void log_physics_diagnostics(
    const OctarynServerPlayerInput &input, float dt, const Vec3 &position,
    const Vec3 &target, const JPH::RVec3 &next_position, float velocity_x,
    float velocity_y, float velocity_z, bool block_x, bool block_z,
    bool was_grounded, const JPH::CharacterVirtual &character,
    octaryn_server_player_block_query_fn block_query, void *context) {
  if (!physics_debug_enabled()) {
    return;
  }

  const float actual_x = static_cast<float>(next_position.GetX()) - position.x;
  const float actual_z = static_cast<float>(next_position.GetZ()) - position.z;
  const float desired_horizontal = horizontal_length(velocity_x, velocity_z);
  const float actual_horizontal = horizontal_length(actual_x, actual_z);
  const bool moving = desired_horizontal > 0.01f;
  const bool stalled = moving && actual_horizontal < 0.001f;
  if (!stalled && !(block_x || block_z)) {
    return;
  }

  std::fprintf(stderr,
               "server_player_jolt_profile dt=%.6f input=(%.3f,%.3f,%.3f) "
               "flags=%u pos=(%.3f,%.3f,%.3f) target=(%.3f,%.3f,%.3f) "
               "next=(%.3f,%.3f,%.3f) velocity=(%.3f,%.3f,%.3f) "
               "actual=(%.6f,%.6f) block=(%u,%u) grounded=(%u,%u) "
               "contacts=%zu foot_y=%.3f eye_offset=%.3f\n",
               dt, input.move_x, input.move_y, input.move_z,
               static_cast<unsigned>(input.flags), position.x, position.y,
               position.z, target.x, target.y, target.z,
               static_cast<float>(next_position.GetX()),
               static_cast<float>(next_position.GetY()),
               static_cast<float>(next_position.GetZ()), velocity_x,
               velocity_y, velocity_z, actual_x, actual_z,
               block_x ? 1u : 0u, block_z ? 1u : 0u,
               was_grounded ? 1u : 0u, is_grounded(
                                       const_cast<JPH::CharacterVirtual &>(
                                           character))
                                       ? 1u
                                       : 0u,
               character.GetActiveContacts().size(), position.y - EyeOffset,
               EyeOffset);

  int index = 0;
  for (const JPH::CharacterVirtual::Contact &contact :
       character.GetActiveContacts()) {
    if (index >= 8) {
      break;
    }
    std::fprintf(stderr,
                 "server_player_jolt_contact index=%d had=%u pos=(%.3f,%.3f,"
                 "%.3f) normal=(%.3f,%.3f,%.3f)\n",
                 index, contact.mHadCollision ? 1u : 0u,
                 static_cast<float>(contact.mPosition.GetX()),
                 static_cast<float>(contact.mPosition.GetY()),
                 static_cast<float>(contact.mPosition.GetZ()),
                 contact.mContactNormal.GetX(), contact.mContactNormal.GetY(),
                 contact.mContactNormal.GetZ());
    index++;
  }
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

  const bool was_grounded =
      is_grounded(character) ||
      (state.is_on_ground != 0u && state.velocity_y <= 0.1f);
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
  update_settings.mStickToFloorStepDown = JPH::Vec3::sZero();
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
  const bool next_grounded = is_grounded(character);
  const float delta_y = state.y - position.y;
  state.velocity_x = (state.x - position.x) / dt;
  if (next_grounded) {
    state.velocity_y = 0.0f;
  } else if (!jump_requested && velocity_y <= 0.0f && delta_y > 0.0f) {
    state.velocity_y = std::min(velocity_y, 0.0f);
  } else {
    state.velocity_y = velocity_y;
  }
  state.velocity_z = (state.z - position.z) / dt;
  state.is_on_ground = next_grounded ? 1u : 0u;
  state.control_mode = WalkMode;
  state.jump_held = jump_held ? 1u : 0u;
  return true;
}

} // namespace octaryn::server::simulation::players
