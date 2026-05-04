#pragma once

#include "Input.h"
#include "PresentationState.h"
#include "octaryn_client_camera.h"
#include "octaryn_client_host_exports.h"

#include <cstdint>
#include <vector>

namespace octaryn_client_app {

struct client_block_raycast_hit {
  bool has_hit = false;
  block_position_key hit{};
  block_position_key adjacent{};
  uint16_t block = 0u;
};

client_block_raycast_hit
raycast_block_interaction(const octaryn_client_camera &camera,
                          const block_lookup &lookup);
client_block_raycast_hit
raycast_native_empty_world_interaction(const octaryn_client_camera &camera,
                                       const block_lookup &overrides);
bool write_block_interaction_intent(
    const octaryn_host_frame_snapshot &frame,
    const client_input_debug_state &input, const octaryn_client_camera &camera,
    const client_block_raycast_hit &hit, uint16_t selected_place_block,
    std::vector<presentation_block> &world_blocks, block_lookup &lookup,
    bool preserve_air_edits);

} // namespace octaryn_client_app
