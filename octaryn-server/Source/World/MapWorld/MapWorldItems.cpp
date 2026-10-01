#include "MapWorld.h"

#include "ItemMotion.h"
#include "MapSceneGeometry.h"
#include "MapWorldSession.h"

#include <cstdint>
#include <algorithm>
#include <cmath>

extern "C" {

int octaryn_server_map_world_step_item(void *handle,
                                       octaryn_world_item_state *state,
                                       double delta_seconds) {
  auto *world =
      static_cast<octaryn::server::map_world::ServerMapWorld *>(handle);
  if (world == nullptr || state == nullptr || delta_seconds <= 0.0) {
    return -1;
  }
  if (delta_seconds > 0.25) {
    delta_seconds = 0.25;
  }
  const float radius = 2 + static_cast<float>(delta_seconds) *
      std::hypot(state->velocity_x,state->velocity_y,state->velocity_z)+24*float(delta_seconds*delta_seconds);
  // A pending tile holds the exact state; returning unavailable would activate
  // the host's analytic fall path and allow an item to cross missing geometry.
  if (!world->ready(state->x, state->y, state->z, radius)) return 0;

  octaryn::item_motion::ItemState item{};
  item.x = state->x;
  item.y = state->y;
  item.z = state->z;
  item.velocity_x = state->velocity_x;
  item.velocity_y = state->velocity_y;
  item.velocity_z = state->velocity_z;
  item.grounded = state->grounded;
  item.sleeping = state->sleeping;
  item.sleep_timer = state->sleep_timer;

  const auto mesh = world->collision();
  const auto params = octaryn::item_motion::default_item_params();
  octaryn::item_motion::step_item_on_mesh(item,
                                          static_cast<float>(delta_seconds),
                                          params, mesh);

  state->x = item.x;
  state->y = item.y;
  state->z = item.z;
  state->velocity_x = item.velocity_x;
  state->velocity_y = item.velocity_y;
  state->velocity_z = item.velocity_z;
  state->grounded = item.grounded;
  state->sleeping = item.sleeping;
  state->sleep_timer = item.sleep_timer;
  return 0;
}

} // extern "C"
