#pragma once

#include <SDL3/SDL.h>

auto gpu_upload_command_buffer() -> SDL_GPUCommandBuffer*&;
auto gpu_upload_copy_pass() -> SDL_GPUCopyPass*&;
