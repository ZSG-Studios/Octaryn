#pragma once

#include "Input.h"
#include "PresentationState.h"
#include "Camera.h"
#include "HostExports.h"

#include <cstdint>
#include <vector>

struct empty_world_dirty_column;

namespace octaryn_client_app {

struct client_block_raycast_hit {
  bool has_hit = false;
  block_position_key hit{};
  block_position_key adjacent{};
  uint16_t block = 0u;
};

client_block_raycast_hit
raycast_block_interaction(const camera &camera,
                          const block_lookup &lookup);
client_block_raycast_hit
raycast_native_empty_world_interaction(const camera &camera,
                                       const block_lookup &overrides);
bool write_block_interaction_intent(
    const octaryn_host_frame_snapshot &frame,
    const client_input_debug_state &input, const camera &camera,
    const client_block_raycast_hit &hit, uint16_t selected_place_block,
    std::vector<presentation_block> &world_blocks, block_lookup &lookup,
    bool preserve_air_edits, bool apply_local_edits,
    std::vector<empty_world_dirty_column> &dirty_columns);

} // namespace octaryn_client_app
