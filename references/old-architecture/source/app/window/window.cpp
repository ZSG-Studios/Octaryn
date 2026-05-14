#include "internal.h"

void main_window_init(main_window_t* state)
{
    *state = {};
    state->present_mode = SDL_GPU_PRESENTMODE_VSYNC;
}
