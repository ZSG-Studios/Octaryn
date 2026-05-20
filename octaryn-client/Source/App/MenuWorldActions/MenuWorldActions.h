#pragma once

#include "RuntimeControls.h"
#include "SingleplayerServerSession.h"
#include "WorldIntents.h"

#include "Camera.h"

namespace octaryn_client_app {

enum menu_action_result : uint32_t {
  MENU_ACTION_RESULT_IGNORED = 0u,
  MENU_ACTION_RESULT_COMPLETED = 1u,
  MENU_ACTION_RESULT_FAILED = 2u,
  MENU_ACTION_RESULT_FATAL = 3u,
};

menu_action_result run_menu_action(
    singleplayer_server_session &server_session, bool game_modules_disabled,
    const camera &camera, int render_distance,
    const client_world_time_controls &world_time_controls, uint32_t action,
    uint32_t world_slot, const char *server_address, const char *server_port,
    const char *world_name, uint32_t &menu_status_code, int &result);

void refresh_singleplayer_world_slots(runtime_controls &controls);

} // namespace octaryn_client_app
