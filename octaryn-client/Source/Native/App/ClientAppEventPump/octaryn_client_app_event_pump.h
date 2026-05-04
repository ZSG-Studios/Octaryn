#pragma once

#include "octaryn_client_app_input.h"
#include "octaryn_client_app_world_intents.h"
#include "octaryn_client_block_atlas.h"
#include "octaryn_client_runtime_controls.h"
#include "octaryn_client_swapchain.h"

#include <SDL3/SDL.h>

#include <cstdint>

namespace octaryn_client_app {

struct block_selection_state {
  uint16_t selected_block = 29u;
  uint64_t change_count = 0u;
};

void poll_client_app_events(
    SDL_Window *window, SDL_GPUDevice *gpu_device,
    octaryn_client_frame_pacing &frame_pacing,
    octaryn_client_swapchain_state &swapchain_state,
    octaryn_client_runtime_controls &runtime_controls, client_key_state &keys,
    client_world_time_controls &world_time_controls,
    block_selection_state &block_selection,
    const octaryn::client::rendering::ClientBlockAtlas &atlas,
    bool game_modules_disabled, pointer_motion_debug_state &pointer_motion,
    pointer_click_debug_state &pointer_click, bool &running,
    uint64_t frame_index);

} // namespace octaryn_client_app
