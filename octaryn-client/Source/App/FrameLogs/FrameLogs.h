#pragma once

#include "HostCommands.h"
#include "Input.h"
#include "PresentationState.h"
#include "Camera.h"
#include "FrameProfile.h"

#include <cstdint>
#include <vector>

namespace octaryn_client_app {

void log_live_client_frame(uint64_t frame_index,
                           const client_input_debug_state &input,
                           const client_command_frame_counts &commands,
                           const camera &camera,
                           uint32_t drained_updates,
                           const std::vector<presentation_block> &blocks);
void log_frame_profile(uint64_t frame_index,
                       const frame_profile_snapshot &profile,
                       uint8_t debug_overlay_enabled);

} // namespace octaryn_client_app
