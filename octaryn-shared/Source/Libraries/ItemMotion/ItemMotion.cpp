#include "ItemMotion.h"

#include "MeshItemStep.h"

#include <algorithm>
#include <cmath>

namespace octaryn::item_motion {
namespace {

// Substeps keep plane-solve iterations accurate at high speed; the swept cast
// already prevents tunnelling at any step size.
constexpr float MaxSubstepSeconds = 1.0f / 120.0f;
constexpr int MaxSubsteps = 8;

} // namespace

ItemStepParams default_item_params() { return ItemStepParams{}; }

void step_item_on_mesh(ItemState &state, float dt,
                       const ItemStepParams &params,
                       const character_motion::MeshCollision &mesh) {
  if (dt <= 0.0f || state.sleeping != 0u) {
    return;
  }
  character_motion::MeshCollisionWorld *world =
      character_motion::acquire_mesh_world(mesh);
  if (world == nullptr) {
    return;
  }

  const int substeps = std::clamp(
      static_cast<int>(std::ceil(dt / MaxSubstepSeconds)), 1, MaxSubsteps);
  const float substep_dt = dt / static_cast<float>(substeps);
  for (int i = 0; i < substeps && state.sleeping == 0u; ++i) {
    step_item_substep(state, substep_dt, params, world);
  }
}

} // namespace octaryn::item_motion
