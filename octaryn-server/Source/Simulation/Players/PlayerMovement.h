#pragma once

#include "PlayerSimulation.h"

namespace octaryn::server::simulation::players {

void move_fly(const OctarynServerPlayerInput &input, float dt,
              OctarynServerPlayerState &state, float pitch, float yaw);

void move_walk(const OctarynServerPlayerInput &input, float dt,
               OctarynServerPlayerState &state, float pitch, float yaw,
               octaryn_server_player_block_query_fn block_query, void *context);

} // namespace octaryn::server::simulation::players
