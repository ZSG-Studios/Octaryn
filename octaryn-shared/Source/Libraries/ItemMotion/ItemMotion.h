#pragma once

#include <cstdint>

#include "CharacterMotion.h"

namespace octaryn::item_motion {

// Ballistic state of one dropped item. Position is the collision capsule
// centre in world space. Sleeping items are settled and skip all work until
// the owning module teleports them or applies a new impulse.
struct ItemState {
  float x{}, y{}, z{};
  float velocity_x{}, velocity_y{}, velocity_z{};
  // Resting contact found during the last step.
  std::uint32_t grounded{};
  // Settled: no motion. Modules map this to their own "resting" flag.
  std::uint32_t sleeping{};
  // Seconds spent below sleep speed while supported.
  float sleep_timer{};
};

struct ItemStepParams {
  float radius{0.25f};           // collision capsule radius, metres
  float gravity{24.0f};          // m/s^2, matches the character world gravity
  float restitution{0.42f};      // normal velocity kept after a bounce [0..1]
  float bounce_speed{1.0f};      // below this impact speed contacts absorb
  float contact_friction{3.0f};  // tangential velocity removed per contact second
  float air_drag{0.05f};         // velocity removed per second while airborne
  float max_speed{60.0f};        // terminal clamp, m/s
  float sleep_speed{0.35f};      // below this with support the item settles
  float rest_speed{0.6f};        // below this with support static friction stops it
  float sleep_delay{0.4f};       // seconds below sleep speed before sleeping
};

ItemStepParams default_item_params();

// Steps one dropped item against the map collision soup. Motion is swept, so
// fast tosses never tunnel through thin geometry; contacts reflect with
// restitution and slide with friction; supported slow items settle to sleep.
// Deterministic: query-only against the cached static mesh world, no dynamic
// bodies and no internal Box3D stepping, matching the character mover.
void step_item_on_mesh(ItemState &state, float dt, const ItemStepParams &params,
                       const character_motion::MeshCollision &mesh);

} // namespace octaryn::item_motion
