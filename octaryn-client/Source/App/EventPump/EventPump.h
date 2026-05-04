#pragma once

#include "Input.h"
#include "WorldIntents.h"
#include "BlockAtlas.h"
#include "RuntimeControls.h"
#include "octaryn_client_swapchain.h"

#include <SDL3/SDL.h>

#include <cstdint>

namespace octaryn_client_app {

struct block_selection_state {
  uint16_t selected_block = 29u;
  uint64_t change_count = 0u;
};

void poll_events(
    SDL_Window *window, SDL_GPUDevice *gpu_device,
    octaryn_client_frame_pacing &frame_pacing,
    octaryn_client_swapchain_state &swapchain_state,
    runtime_controls &runtime_controls, client_key_state &keys,
    client_world_time_controls &world_time_controls,
    block_selection_state &block_selection,
    const octaryn::client::rendering::BlockAtlas &atlas,
    bool game_modules_disabled, pointer_motion_debug_state &pointer_motion,
    pointer_click_debug_state &pointer_click, bool &running,
    uint64_t frame_index);

} // namespace octaryn_client_app
