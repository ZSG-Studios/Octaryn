#pragma once

#include <SDL3/SDL.h>

#include "app/runtime/frame_pacing.h"

typedef struct main_window
{
    float accumulated_ms;
    int accumulated_frames;
    Uint64 last_title_update;
    Uint64 submitted_fps_window_ticks;
    Uint64 last_submitted_frame_ticks;
    Uint32 submitted_fps_window_frames;
    Uint32 debug_fps_tenths;
    Uint32 debug_frame_time_hundredths;
    Uint32 debug_loop_fps_tenths;
    Uint32 debug_loop_frame_time_hundredths;
    Uint32 debug_submitted_fps_tenths;
    Uint32 debug_submitted_frame_time_hundredths;
    Uint32 debug_last_submitted_fps_tenths;
    Uint32 debug_last_submitted_frame_time_hundredths;
    SDL_GPUPresentMode present_mode;
}
main_window_t;

void main_window_init(main_window_t* state);
void main_window_update_title(main_window_t* state, SDL_Window* window, const char* app_name, Uint64 now_ticks, float dt);
void main_window_note_submitted_frame(main_window_t* state, Uint64 now_ticks);
Uint32 main_window_debug_fps_tenths(const main_window_t* state);
Uint32 main_window_debug_frame_time_hundredths(const main_window_t* state);
Uint32 main_window_debug_submitted_fps_tenths(const main_window_t* state);
Uint32 main_window_debug_submitted_frame_time_hundredths(const main_window_t* state);
Uint32 main_window_debug_last_submitted_fps_tenths(const main_window_t* state);
Uint32 main_window_debug_last_submitted_frame_time_hundredths(const main_window_t* state);
bool main_window_toggle_fullscreen(SDL_Window* window);
bool main_window_configure_swapchain(main_window_t* state,
                                     SDL_GPUDevice* device,
                                     SDL_Window* window,
                                     app_frame_pacing_t* frame_pacing);
const char* main_window_present_mode_name(const main_window_t* state);
bool main_window_apply_best_fullscreen(SDL_Window* window);
bool main_window_show(SDL_Window* window);
void main_window_finish_show(SDL_Window* window);
