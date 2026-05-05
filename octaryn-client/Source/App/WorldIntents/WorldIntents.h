#pragma once

#include "ChunkView.h"
#include "HostExports.h"
#include "SingleplayerServerSession.h"

namespace octaryn_client_app {

struct client_world_time_controls {
  int32_t speed_index = 2;
  double speed_multiplier = 60.0;
  bool dirty = true;
};

bool write_world_time_intent(const singleplayer_server_session &session,
                             const client_world_time_controls &controls);
bool write_chunk_view_intent(const chunk_view &view,
                             const chunk_view &previous_view,
                             uint64_t epoch);
bool chunk_view_intent_needs_progress(const chunk_view &view);
void acknowledge_chunk_view_stream(int32_t center_x,
                                   int32_t center_z,
                                   uint32_t radius);
bool write_player_input_intent(const octaryn_host_frame_snapshot &frame);

} // namespace octaryn_client_app
