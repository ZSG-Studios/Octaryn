#include "world/runtime/private.h"

#include <SDL3/SDL.h>

#include "render/buffer/buffer.h"

namespace {

cpu_buffer_t g_cpu_indices{};
gpu_buffer_t g_gpu_indices{};
SDL_Mutex* g_index_mutex = nullptr;

} // namespace

void world_runtime_indices_init_internal(void)
{
    g_index_mutex = SDL_CreateMutex();
    if (!g_index_mutex)
    {
        SDL_Log("Failed to create mutex: %s", SDL_GetError());
    }
    cpu_buffer_init(&g_cpu_indices, world_device_internal(), sizeof(Uint32));
    gpu_buffer_init(&g_gpu_indices, world_device_internal(), SDL_GPU_BUFFERUSAGE_INDEX);
}

void world_runtime_indices_free_internal(void)
{
    cpu_buffer_free(&g_cpu_indices);
    gpu_buffer_free(&g_gpu_indices);
    SDL_DestroyMutex(g_index_mutex);
    g_index_mutex = nullptr;
}

bool world_gen_indices_internal(Uint32 size)
{
    SDL_LockMutex(g_index_mutex);
    const Uint32 quad_count = (size + 3u) / 4u;
    const Uint32 index_count = quad_count * 6u;
    if (g_gpu_indices.size >= index_count)
    {
        SDL_UnlockMutex(g_index_mutex);
        return true;
    }
    if (!gpu_buffer_begin_upload(&g_gpu_indices))
    {
        SDL_UnlockMutex(g_index_mutex);
        return false;
    }
    static const Uint32 INDICES[] = {0, 1, 2, 3, 2, 1};
    cpu_buffer_clear(&g_cpu_indices);
    if (!cpu_buffer_reserve(&g_cpu_indices, index_count))
    {
        gpu_buffer_abort_upload(&g_gpu_indices);
        SDL_UnlockMutex(g_index_mutex);
        return false;
    }
    Uint32* indices = reinterpret_cast<Uint32*>(g_cpu_indices.data);
    Uint32 write_index = 0;
    for (Uint32 i = 0; i < quad_count; i++)
    {
        for (Uint32 j = 0; j < SDL_arraysize(INDICES); j++)
        {
            indices[write_index++] = i * 4u + INDICES[j];
        }
    }
    g_cpu_indices.size = index_count;
    if (!gpu_buffer_upload(&g_gpu_indices, &g_cpu_indices))
    {
        gpu_buffer_abort_upload(&g_gpu_indices);
        SDL_UnlockMutex(g_index_mutex);
        return false;
    }
    const bool submit_ok = gpu_buffer_end_upload(&g_gpu_indices);
    SDL_UnlockMutex(g_index_mutex);
    return submit_ok;
}

const gpu_buffer_t* world_gpu_indices_internal(void)
{
    return &g_gpu_indices;
}
