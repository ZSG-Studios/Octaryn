#include "PlayerJoltWorld.h"

// clang-format off
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
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

namespace octaryn::character_motion {
namespace {
constexpr float CollisionScanVerticalPadding = 1.25f;
constexpr int32_t BlockScanPadding = 2;

void jolt_trace(const char *format, ...) {
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  std::fputc('\n', stderr);
  va_end(args);
}

int32_t floor_to_int(float value) {
  return static_cast<int32_t>(std::floor(value));
}

uint32_t query_block(SolidQuery block_query,
                     void *context, int32_t x, int32_t y, int32_t z) {
  return block_query ? block_query(context, x, y, z) : 0u;
}

bool body_hits_solid(SolidQuery block_query,
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

bool physics_debug_enabled() {
  static const bool enabled = [] {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) // getenv: no SDL dependency on the server.
#endif
    const char *value = std::getenv("OCTARYN_SERVER_PLAYER_PHYSICS_DEBUG");
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    return value && value[0] != '\0' && std::strcmp(value, "0") != 0;
  }();
  return enabled;
}

float horizontal_length(float x, float z) {
  return std::sqrt(x * x + z * z);
}
} // namespace

bool ObjectLayerPairFilter::ShouldCollide(JPH::ObjectLayer first,
                                          JPH::ObjectLayer second) const {
  if (first == Layers::Static) {
    return second == Layers::Character;
  }
  if (first == Layers::Character) {
    return true;
  }
  return false;
}

BroadPhaseLayerMap::BroadPhaseLayerMap() {
  layers_[Layers::Static] = BroadPhaseLayers::Static;
  layers_[Layers::Character] = BroadPhaseLayers::Character;
}

JPH::uint BroadPhaseLayerMap::GetNumBroadPhaseLayers() const {
  return BroadPhaseLayers::Count;
}

JPH::BroadPhaseLayer
BroadPhaseLayerMap::GetBroadPhaseLayer(JPH::ObjectLayer layer) const {
  return layers_[layer < Layers::Count ? layer : Layers::Static];
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char *BroadPhaseLayerMap::GetBroadPhaseLayerName(
    JPH::BroadPhaseLayer layer) const {
  if (layer == BroadPhaseLayers::Static) {
    return "static";
  }
  if (layer == BroadPhaseLayers::Character) {
    return "character";
  }
  return "invalid";
}
#endif

bool ObjectVsBroadPhaseFilter::ShouldCollide(
    JPH::ObjectLayer layer, JPH::BroadPhaseLayer broad_phase_layer) const {
  if (layer == Layers::Static) {
    return broad_phase_layer == BroadPhaseLayers::Character;
  }
  if (layer == Layers::Character) {
    return true;
  }
  return false;
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

bool is_solid_block_info(uint32_t block_info) {
  return (block_info & SolidBlockFlag) != 0u;
}

void add_collision_blocks(JPH::PhysicsSystem &system,
 SolidQuery block_query,
                          void *context, const Vec3 &position,
                          const Vec3 &target, float dt) {
  JPH::BodyInterface &bodies = system.GetBodyInterface();
  // Voxels meet at planar faces; rounded boxes introduce grooves and sloped
  // contact normals at every seam, slowing grounded movement across the floor.
  JPH::RefConst<JPH::Shape> block_shape =
      new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f), 0.0f);
  JPH::StaticCompoundShapeSettings terrain;
  const JPH::Vec3 origin(std::floor(position.x), std::floor(position.y),
                         std::floor(position.z));

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

        terrain.AddShape(JPH::Vec3(static_cast<float>(x) + 0.5f,
                                   static_cast<float>(y) + 0.5f,
                                   static_cast<float>(z) + 0.5f) - origin,
                         JPH::Quat::sIdentity(), block_shape);
      }
    }
  }
  if (terrain.mSubShapes.empty()) return;
  // Jolt's enhanced internal-edge removal compares contacts within one body.
  // A compound preserves the exact voxel union while allowing it to discard
  // the hidden side contacts between adjacent floor/wall blocks.
  const auto terrain_shape = terrain.Create();
  const JPH::BodyCreationSettings settings(
      terrain_shape.Get(), JPH::RVec3(origin), JPH::Quat::sIdentity(),
      JPH::EMotionType::Static, Layers::Static);
  bodies.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
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

bool has_floor_support(SolidQuery block_query,
                       void *context, float x, float base_y, float z) {
  constexpr float Skin = 0.04f;
  constexpr float SupportDepth = 0.16f;
  const int32_t min_x = floor_to_int(x - CollisionRadius + Skin);
  const int32_t max_x = floor_to_int(x + CollisionRadius - Skin);
  const int32_t min_z = floor_to_int(z - CollisionRadius + Skin);
  const int32_t max_z = floor_to_int(z + CollisionRadius - Skin);
  const int32_t floor_y = floor_to_int(base_y - SupportDepth);
  return is_solid_block_info(query_block(block_query, context, min_x, floor_y, min_z)) ||
         is_solid_block_info(query_block(block_query, context, max_x, floor_y, min_z)) ||
         is_solid_block_info(query_block(block_query, context, min_x, floor_y, max_z)) ||
         is_solid_block_info(query_block(block_query, context, max_x, floor_y, max_z));
}

JPH::RVec3 resolve_body_penetration(
 SolidQuery block_query, void *context,
    const Vec3 &previous_eye_position, const JPH::RVec3 &next_base_position) {
  const float next_x = static_cast<float>(next_base_position.GetX());
  const float next_y = static_cast<float>(next_base_position.GetY());
  const float next_z = static_cast<float>(next_base_position.GetZ());
  if (!body_hits_solid(block_query, context, next_x, next_y, next_z)) {
    return next_base_position;
  }
  const float previous_base_y = previous_eye_position.y - EyeOffset;
  // Prefer keeping each axis of progress before freezing: jumping tight to a
  // wall must keep rising (slide) instead of losing the whole tick to a full
  // revert. The full freeze below runs only when every partial also embeds.
  if (!body_hits_solid(block_query, context, previous_eye_position.x, next_y,
                       next_z)) {
    return JPH::RVec3(previous_eye_position.x, next_y, next_z);
  }
  if (!body_hits_solid(block_query, context, next_x, next_y,
                       previous_eye_position.z)) {
    return JPH::RVec3(next_x, next_y, previous_eye_position.z);
  }
  if (!body_hits_solid(block_query, context, previous_eye_position.x,
                       previous_base_y, previous_eye_position.z)) {
    return JPH::RVec3(previous_eye_position.x, previous_base_y,
                     previous_eye_position.z);
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

void log_physics_diagnostics(
 const Input &input, float dt, const Vec3 &position,
    const Vec3 &target, const JPH::RVec3 &next_position, float velocity_x,
    float velocity_y, float velocity_z, bool block_x, bool block_z,
    bool was_grounded, const JPH::CharacterVirtual &character,
 SolidQuery block_query, void *context) {
  (void)block_query;
  (void)context;
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
               was_grounded ? 1u : 0u,
               character.GetGroundState() ==
                       JPH::CharacterBase::EGroundState::OnGround
                   ? 1u
                   : 0u,
               character.GetActiveContacts().size(), position.y - EyeOffset,
               EyeOffset);
}

} // namespace octaryn::character_motion
