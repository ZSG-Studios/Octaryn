#pragma once

#include "Camera.h"
#include "EmptyWorldMesh.h"
#include "RuntimeControls.h"

#include <cstdint>

namespace octaryn_client_app {

void apply_render_distance_far_plane(camera &camera, int render_distance);
void apply_movement_probe_render_distance(runtime_controls &controls);
bool apply_movement_probe_camera_spawn(camera &camera);
void align_movement_probe_camera_to_terrain(camera &camera,
                                            uint64_t frame_index);
void log_camera_terrain_state(
    const camera &camera, const block_lookup &world_block_lookup,
    uint64_t frame_index);
void log_frame_phase_profile(uint64_t frame_index, float controller_ms,
                             float terrain_align_ms, float raycast_ms,
                             float intent_ms, float poll_stream_ms,
                             float host_tick_ms, float mesh_update_ms,
                             float presentation_ms);

} // namespace octaryn_client_app
