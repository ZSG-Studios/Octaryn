#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace octaryn::physics::jolt_layers {

namespace objects {
constexpr JPH::ObjectLayer STATIC_TERRAIN = 0;
constexpr JPH::ObjectLayer DYNAMIC_BODY = 1;
constexpr JPH::ObjectLayer SENSOR = 2;
constexpr JPH::ObjectLayer PLAYER = 3;
constexpr JPH::ObjectLayer COUNT = 4;
} // namespace objects

namespace broadphase {
constexpr JPH::BroadPhaseLayer STATIC_TERRAIN(0);
constexpr JPH::BroadPhaseLayer DYNAMIC_BODY(1);
constexpr JPH::BroadPhaseLayer SENSOR(2);
constexpr JPH::BroadPhaseLayer PLAYER(3);
constexpr JPH::uint COUNT = 4;
} // namespace broadphase

class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterface();

    JPH::uint GetNumBroadPhaseLayers() const override;
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif

private:
    JPH::BroadPhaseLayer object_to_broadphase_[objects::COUNT];
};

class ObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer object_layer, JPH::BroadPhaseLayer broadphase_layer) const override;
};

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override;
};

} // namespace octaryn::physics::jolt_layers
