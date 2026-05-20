#pragma once

#include "PlayerSimulation.h"

namespace octaryn::server::simulation::players {

bool move_walk_with_jolt(const OctarynServerPlayerInput &input, float dt,
                         OctarynServerPlayerState &state, float pitch,
                         float yaw,
                         octaryn_server_player_block_query_fn block_query,
                         void *context);

} // namespace octaryn::server::simulation::players
