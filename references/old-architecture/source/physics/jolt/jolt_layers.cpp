#include "physics/jolt/jolt_layers.h"

#include <Jolt/Jolt.h>

namespace octaryn::physics::jolt_layers {
namespace {

bool collides_with_static_terrain(JPH::ObjectLayer layer)
{
    return layer == objects::DYNAMIC_BODY || layer == objects::SENSOR || layer == objects::PLAYER;
}

bool collides_with_dynamic_body(JPH::ObjectLayer layer)
{
    return layer == objects::STATIC_TERRAIN || layer == objects::DYNAMIC_BODY || layer == objects::SENSOR ||
           layer == objects::PLAYER;
}

bool collides_with_sensor(JPH::ObjectLayer layer)
{
    return layer == objects::STATIC_TERRAIN || layer == objects::DYNAMIC_BODY || layer == objects::PLAYER;
}

bool collides_with_player(JPH::ObjectLayer layer)
{
    return layer == objects::STATIC_TERRAIN || layer == objects::DYNAMIC_BODY || layer == objects::SENSOR;
}

} // namespace

BroadPhaseLayerInterface::BroadPhaseLayerInterface()
{
    object_to_broadphase_[objects::STATIC_TERRAIN] = broadphase::STATIC_TERRAIN;
    object_to_broadphase_[objects::DYNAMIC_BODY] = broadphase::DYNAMIC_BODY;
    object_to_broadphase_[objects::SENSOR] = broadphase::SENSOR;
    object_to_broadphase_[objects::PLAYER] = broadphase::PLAYER;
}

JPH::uint BroadPhaseLayerInterface::GetNumBroadPhaseLayers() const
{
    return broadphase::COUNT;
}

JPH::BroadPhaseLayer BroadPhaseLayerInterface::GetBroadPhaseLayer(JPH::ObjectLayer layer) const
{
    if (layer >= objects::COUNT)
    {
        JPH_ASSERT(false);
        return JPH::cBroadPhaseLayerInvalid;
    }
    return object_to_broadphase_[layer];
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char* BroadPhaseLayerInterface::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const
{
    switch (static_cast<JPH::BroadPhaseLayer::Type>(layer))
    {
    case static_cast<JPH::BroadPhaseLayer::Type>(broadphase::STATIC_TERRAIN):
        return "static_terrain";
    case static_cast<JPH::BroadPhaseLayer::Type>(broadphase::DYNAMIC_BODY):
        return "dynamic_body";
    case static_cast<JPH::BroadPhaseLayer::Type>(broadphase::SENSOR):
        return "sensor";
    case static_cast<JPH::BroadPhaseLayer::Type>(broadphase::PLAYER):
        return "player";
    default:
        JPH_ASSERT(false);
        return "invalid";
    }
}
#endif

bool ObjectVsBroadPhaseLayerFilter::ShouldCollide(JPH::ObjectLayer object_layer, JPH::BroadPhaseLayer broadphase_layer) const
{
    switch (static_cast<JPH::BroadPhaseLayer::Type>(broadphase_layer))
    {
    case static_cast<JPH::BroadPhaseLayer::Type>(broadphase::STATIC_TERRAIN):
        return collides_with_static_terrain(object_layer);
    case static_cast<JPH::BroadPhaseLayer::Type>(broadphase::DYNAMIC_BODY):
        return collides_with_dynamic_body(object_layer);
    case static_cast<JPH::BroadPhaseLayer::Type>(broadphase::SENSOR):
        return collides_with_sensor(object_layer);
    case static_cast<JPH::BroadPhaseLayer::Type>(broadphase::PLAYER):
        return collides_with_player(object_layer);
    default:
        JPH_ASSERT(false);
        return false;
    }
}

bool ObjectLayerPairFilter::ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const
{
    switch (first)
    {
    case objects::STATIC_TERRAIN:
        return collides_with_static_terrain(second);
    case objects::DYNAMIC_BODY:
        return collides_with_dynamic_body(second);
    case objects::SENSOR:
        return collides_with_sensor(second);
    case objects::PLAYER:
        return collides_with_player(second);
    default:
        JPH_ASSERT(false);
        return false;
    }
}

} // namespace octaryn::physics::jolt_layers
