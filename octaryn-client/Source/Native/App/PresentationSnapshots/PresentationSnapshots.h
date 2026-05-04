#pragma once

#include "PresentationState.h"
#include "WorldStream.h"
#include "octaryn_client_camera.h"
#include "ChunkView.h"
#include "HostExports.h"
#include "octaryn_singleplayer_server_session.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace octaryn_client_app {

struct client_server_stream_poll_state {
  server_chunk_stream_file active_server_stream{};
  std::filesystem::file_time_type active_server_stream_write_time{};
  uint64_t active_server_stream_override_signature = 0u;
  bool loaded_server_world_blocks = false;
};

void place_camera_over_snapshot(octaryn_client_camera &camera,
                                const std::vector<presentation_block> &blocks);
bool poll_server_stream_presentation(
    const singleplayer_server_session &server_session,
    bool game_modules_disabled,
    const chunk_view &empty_world_mesh_chunk_view,
    uint64_t frame_index, client_server_stream_poll_state &poll_state,
    server_world_time_state &world_time,
    std::vector<presentation_block> &world_snapshot_blocks,
    std::vector<presentation_block> &world_surface_blocks,
    block_lookup &world_block_lookup, octaryn_client_camera &camera,
    bool &empty_world_stream_mesh_dirty, int &result);

} // namespace octaryn_client_app
