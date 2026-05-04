#pragma once

#include "octaryn_client_chunk_view.h"
#include "HostExports.h"
#include "octaryn_singleplayer_server_session.h"

namespace octaryn_client_app {

struct client_world_time_controls {
  int32_t speed_index = 2;
  double speed_multiplier = 60.0;
  bool dirty = true;
};

bool write_world_time_intent(const singleplayer_server_session &session,
                             const client_world_time_controls &controls);
bool write_chunk_view_intent(const octaryn_client_chunk_view &view,
                             const octaryn_client_chunk_view &previous_view,
                             uint64_t epoch);
bool write_player_input_intent(const octaryn_host_frame_snapshot &frame);

} // namespace octaryn_client_app
