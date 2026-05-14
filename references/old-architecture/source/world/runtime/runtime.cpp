#include "world/runtime/private.h"

#include <SDL3/SDL.h>

#include <vector>

#include "render/buffer/buffer.h"
#include "world/lighting/skylight_cpu.h"

namespace {

constexpr Uint8 kSkylightMax = 15;
constexpr int kSkylightBufferWidth = skylight_cpu::kOutputWidth;
constexpr int kSkylightBufferDepth = skylight_cpu::kOutputDepth;

SDL_GPUDevice* g_device = nullptr;
cpu_buffer_t g_cpu_empty_skylight{};
gpu_buffer_t g_gpu_empty_skylight{};

void gen_skylight(void)
{
    if (!gpu_buffer_begin_upload(&g_gpu_empty_skylight))
    {
        return;
    }
    std::vector<Uint8> empty(static_cast<size_t>(kSkylightBufferWidth) * CHUNK_HEIGHT * kSkylightBufferDepth, kSkylightMax);
    cpu_buffer_append_many(&g_cpu_empty_skylight, empty.data(), static_cast<Uint32>(empty.size()));
    if (!gpu_buffer_upload(&g_gpu_empty_skylight, &g_cpu_empty_skylight))
    {
        gpu_buffer_abort_upload(&g_gpu_empty_skylight);
        SDL_Log("Failed to upload empty skylight buffer");
        return;
    }
    if (!gpu_buffer_end_upload(&g_gpu_empty_skylight))
    {
        SDL_Log("Failed to upload empty skylight buffer");
    }
}

} // namespace

SDL_GPUDevice* world_device_internal(void)
{
    return g_device;
}

void world_runtime_buffers_init_internal(SDL_GPUDevice* device)
{
    g_device = device;
    world_runtime_indices_init_internal();
    cpu_buffer_init(&g_cpu_empty_skylight, g_device, sizeof(Uint8));
    gpu_buffer_init(&g_gpu_empty_skylight, g_device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
}

void world_runtime_gen_empty_skylight_internal(void)
{
    gen_skylight();
}

void world_runtime_buffers_free_internal(void)
{
    world_runtime_indices_free_internal();
    cpu_buffer_free(&g_cpu_empty_skylight);
    gpu_buffer_free(&g_gpu_empty_skylight);
    g_device = nullptr;
}

const gpu_buffer_t* world_gpu_empty_skylight_internal(void)
{
    return &g_gpu_empty_skylight;
}
