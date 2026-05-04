#pragma once

#include "octaryn_client_app_host_commands.h"
#include "octaryn_client_app_input.h"
#include "octaryn_client_app_presentation_state.h"
#include "octaryn_client_camera.h"
#include "octaryn_client_frame_profile.h"

#include <cstdint>
#include <vector>

namespace octaryn_client_app {

void log_live_client_frame(uint64_t frame_index,
                           const client_input_debug_state &input,
                           const client_command_frame_counts &commands,
                           const octaryn_client_camera &camera,
                           uint32_t drained_updates,
                           const std::vector<presentation_block> &blocks);
void log_frame_profile(uint64_t frame_index,
                       const octaryn_client_frame_profile_snapshot &profile,
                       uint8_t debug_overlay_enabled);

} // namespace octaryn_client_app
