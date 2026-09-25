#pragma once

#include "CharacterMotion.h"
#include "CharacterGeometry.h"

// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/PhysicsSystem.h>
// clang-format on

#include <cstdint>

namespace octaryn::character_motion {

constexpr uint32_t SolidBlockFlag = 1u << 16u;
constexpr float Gravity = 24.0f;

struct Vec3 {
  float x;
  float y;
  float z;
};

namespace Layers {
constexpr JPH::ObjectLayer Static = 0;
constexpr JPH::ObjectLayer Character = 1;
constexpr JPH::ObjectLayer Count = 2;
} // namespace Layers

namespace BroadPhaseLayers {
inline const JPH::BroadPhaseLayer Static{0};
inline const JPH::BroadPhaseLayer Character{1};
constexpr JPH::uint Count = 2;
} // namespace BroadPhaseLayers

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer first,
                     JPH::ObjectLayer second) const override;
};

class BroadPhaseLayerMap final : public JPH::BroadPhaseLayerInterface {
public:
  BroadPhaseLayerMap();
  JPH::uint GetNumBroadPhaseLayers() const override;
  JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif

private:
  JPH::BroadPhaseLayer layers_[Layers::Count];
};

class ObjectVsBroadPhaseFilter final
    : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer layer,
                     JPH::BroadPhaseLayer broad_phase_layer) const override;
};

void initialize_jolt();
bool is_solid_block_info(uint32_t block_info);
void add_collision_blocks(JPH::PhysicsSystem &system,
 SolidQuery block_query,
                          void *context, const Vec3 &position,
                          const Vec3 &target, float dt);
bool is_grounded(JPH::CharacterVirtual &character);
bool has_blocking_wall_contact(const JPH::CharacterVirtual &character, int axis,
                               float velocity, float foot_y);
bool has_floor_support(SolidQuery block_query,
                       void *context, float x, float base_y, float z);
JPH::RVec3 resolve_body_penetration(
 SolidQuery block_query, void *context,
    const Vec3 &previous_eye_position, const JPH::RVec3 &next_base_position);
void log_physics_diagnostics(
 const Input &input, float dt, const Vec3 &position,
    const Vec3 &target, const JPH::RVec3 &next_position, float velocity_x,
    float velocity_y, float velocity_z, bool block_x, bool block_z,
    bool was_grounded, const JPH::CharacterVirtual &character,
 SolidQuery block_query, void *context);

} // namespace octaryn::character_motion
