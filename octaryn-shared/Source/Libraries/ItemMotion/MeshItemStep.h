#pragma once

#include "ItemMotion.h"
#include "MeshCollisionWorld.h"

namespace octaryn::item_motion {

// One fixed substep against the cached Box3D map world. The collision capsule
// spans +- half the radius around the item centre, so the swept volume is a
// sphere of the configured radius.
void step_item_substep(ItemState &state, float dt, const ItemStepParams &params,
                       character_motion::MeshCollisionWorld *world);

} // namespace octaryn::item_motion
