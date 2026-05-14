#include <SDL3/SDL.h>

#include <array>

#include "core/check.h"
#include "render/buffer/buffer.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"

namespace {

constexpr Uint32 kChunkSlotCount = static_cast<Uint32>(MAX_WORLD_WIDTH * MAX_WORLD_WIDTH);

std::array<chunk_draw_descriptor_t, MAX_WORLD_WIDTH * MAX_WORLD_WIDTH> g_chunk_descriptors = {};
SDL_Mutex* g_descriptor_mutex = nullptr;
cpu_buffer_t g_cpu_chunk_descriptors = {};
gpu_buffer_t g_gpu_chunk_descriptors = {};
bool g_descriptors_dirty = false;
bool g_reframe_active = false;

bool upload_gpu_buffer(gpu_buffer_t* gpu, cpu_buffer_t* cpu)
{
    if (cpu->size == 0)
    {
        gpu_buffer_clear(gpu);
        return true;
    }
    if (!gpu_buffer_begin_upload(gpu))
    {
        return false;
    }
    const bool upload_ok = gpu_buffer_upload(gpu, cpu);
    if (!upload_ok)
    {
        gpu_buffer_abort_upload(gpu);
        return false;
    }
    return gpu_buffer_end_upload(gpu);
}

void reset_descriptor_locked(Uint32 slot)
{
    chunk_draw_descriptor_t descriptor = {};
    descriptor.slot_id = slot;
    g_chunk_descriptors[slot] = descriptor;
}

void sync_descriptor_locked(chunk_t* chunk)
{
    if (!chunk || chunk->slot_id >= kChunkSlotCount)
    {
        return;
    }

    chunk_draw_descriptor_t descriptor = {};
    descriptor.chunk_position[0] = chunk->position[0];
    descriptor.chunk_position[1] = chunk->position[1];
    descriptor.slot_id = chunk->slot_id;
    for (int i = 0; i < MESH_TYPE_COUNT; ++i)
    {
        descriptor.face_offsets[i] = chunk->pooled_face_offsets[i];
        descriptor.face_counts[i] = chunk->pooled_face_counts[i];
    }
    descriptor.skylight_offset = chunk->pooled_skylight_offset;
    descriptor.skylight_count = chunk->pooled_skylight_count;
    if (chunk->pooled_face_counts[MESH_TYPE_OPAQUE] > 0)
    {
        descriptor.flags |= CHUNK_DRAW_DESCRIPTOR_HAS_OPAQUE;
    }
    if (chunk->pooled_face_counts[MESH_TYPE_TRANSPARENT] > 0)
    {
        descriptor.flags |= CHUNK_DRAW_DESCRIPTOR_HAS_TRANSPARENT;
    }
    if (chunk->pooled_skylight_count > 0)
    {
        descriptor.flags |= CHUNK_DRAW_DESCRIPTOR_HAS_SKYLIGHT;
    }
    if (SDL_GetAtomicInt(&chunk->render_mesh_valid) != 0)
    {
        descriptor.flags |= CHUNK_DRAW_DESCRIPTOR_RENDER_READY;
    }
    g_chunk_descriptors[chunk->slot_id] = descriptor;
}

void clear_chunk_render_metadata(chunk_t* chunk)
{
    if (!chunk)
    {
        return;
    }
    SDL_zeroa(chunk->pooled_face_offsets);
    SDL_zeroa(chunk->pooled_face_counts);
    chunk->pooled_skylight_offset = 0;
    chunk->pooled_skylight_count = 0;
}

void sync_all_descriptors_locked(void)
{
    for (Uint32 slot = 0; slot < kChunkSlotCount; ++slot)
    {
        chunk_t* chunk = world_get_chunk_slot_internal(static_cast<int>(slot / MAX_WORLD_WIDTH),
                                                       static_cast<int>(slot % MAX_WORLD_WIDTH));
        if (chunk && chunk->slot_id == slot)
        {
            sync_descriptor_locked(chunk);
        }
        else
        {
            reset_descriptor_locked(slot);
        }
    }
}

bool upload_chunk_descriptors_locked(void)
{
    if (!g_descriptors_dirty)
    {
        return true;
    }
    cpu_buffer_clear(&g_cpu_chunk_descriptors);
    cpu_buffer_append_many(&g_cpu_chunk_descriptors, g_chunk_descriptors.data(), kChunkSlotCount);
    if (!upload_gpu_buffer(&g_gpu_chunk_descriptors, &g_cpu_chunk_descriptors))
    {
        return false;
    }
    g_descriptors_dirty = false;
    return true;
}

} // namespace

const gpu_buffer_t* world_gpu_chunk_descriptors_internal(void)
{
    return &g_gpu_chunk_descriptors;
}

Uint32 world_chunk_face_offset_internal(const chunk_t* chunk, mesh_type_t mesh_type)
{
    return chunk ? chunk->pooled_face_offsets[mesh_type] : 0u;
}

Uint32 world_chunk_face_count_internal(const chunk_t* chunk, mesh_type_t mesh_type)
{
    return chunk ? chunk->pooled_face_counts[mesh_type] : 0u;
}

Uint32 world_chunk_descriptor_index_internal(const chunk_t* chunk)
{
    return chunk ? chunk->slot_id : 0xFFFFFFFFu;
}

Uint32 world_chunk_skylight_offset_internal(const chunk_t* chunk)
{
    return chunk ? chunk->pooled_skylight_offset : 0u;
}

Uint32 world_chunk_skylight_count_internal(const chunk_t* chunk)
{
    return chunk ? chunk->pooled_skylight_count : 0u;
}

Uint32 world_chunk_descriptor_capacity_internal(void)
{
    return kChunkSlotCount;
}

void world_runtime_render_pools_init_internal(SDL_GPUDevice* device)
{
    g_descriptor_mutex = SDL_CreateMutex();
    if (!g_descriptor_mutex)
    {
        SDL_Log("Failed to create chunk descriptor mutex: %s", SDL_GetError());
        return;
    }

    cpu_buffer_init(&g_cpu_chunk_descriptors, device, sizeof(chunk_draw_descriptor_t));
    gpu_buffer_init(&g_gpu_chunk_descriptors, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    for (Uint32 slot = 0; slot < kChunkSlotCount; ++slot)
    {
        reset_descriptor_locked(slot);
    }

    g_descriptors_dirty = true;
    SDL_LockMutex(g_descriptor_mutex);
    upload_chunk_descriptors_locked();
    SDL_UnlockMutex(g_descriptor_mutex);
}

void world_runtime_render_pools_free_internal(void)
{
    g_descriptors_dirty = false;
    g_reframe_active = false;
    cpu_buffer_free(&g_cpu_chunk_descriptors);
    gpu_buffer_free(&g_gpu_chunk_descriptors);
    SDL_DestroyMutex(g_descriptor_mutex);
    g_descriptor_mutex = nullptr;
    g_chunk_descriptors = {};
}

void world_runtime_sync_chunk_descriptor_internal(chunk_t* chunk)
{
    if (!g_descriptor_mutex)
    {
        return;
    }
    SDL_LockMutex(g_descriptor_mutex);
    sync_descriptor_locked(chunk);
    g_descriptors_dirty = true;
    SDL_UnlockMutex(g_descriptor_mutex);
}

void world_runtime_flush_chunk_descriptors_internal(void)
{
    if (!g_descriptor_mutex)
    {
        return;
    }
    SDL_LockMutex(g_descriptor_mutex);
    upload_chunk_descriptors_locked();
    SDL_UnlockMutex(g_descriptor_mutex);
}

Uint32 world_runtime_dirty_pooled_chunk_slot_count_internal(void)
{
    return 0u;
}

bool world_runtime_upload_chunk_lighting_data_internal(chunk_t* chunk,
                                                       const cpu_buffer_t* skylight,
                                                       Uint32 dirty_flags)
{
    return world_runtime_commit_chunk_lighting_data_internal(chunk,
                                                            skylight ? skylight->size : 0u,
                                                            dirty_flags);
}

bool world_runtime_commit_chunk_lighting_data_internal(chunk_t* chunk,
                                                       Uint32 skylight_byte_count,
                                                       Uint32 dirty_flags)
{
    if (!chunk || !g_descriptor_mutex)
    {
        return false;
    }

    SDL_LockMutex(g_descriptor_mutex);
    if (dirty_flags & LIGHT_DIRTY_SKYLIGHT)
    {
        CHECK((skylight_byte_count % sizeof(Uint32)) == 0);
        chunk->pooled_skylight_offset = 0;
        chunk->pooled_skylight_count = skylight_byte_count / sizeof(Uint32);
    }
    sync_descriptor_locked(chunk);
    g_descriptors_dirty = true;
    SDL_UnlockMutex(g_descriptor_mutex);
    return true;
}

void world_runtime_begin_pooled_chunk_slot_reframe_internal(void)
{
    if (!g_descriptor_mutex)
    {
        return;
    }
    SDL_LockMutex(g_descriptor_mutex);
    g_reframe_active = true;
    SDL_UnlockMutex(g_descriptor_mutex);
}

void world_runtime_move_pooled_chunk_slot_internal(Uint32 source_slot, Uint32 destination_slot)
{
    (void)source_slot;
    if (!g_descriptor_mutex)
    {
        return;
    }
    CHECK(destination_slot < kChunkSlotCount);
}

void world_runtime_clear_pooled_chunk_slot_internal(Uint32 slot)
{
    if (!g_descriptor_mutex)
    {
        return;
    }
    CHECK(slot < kChunkSlotCount);

    SDL_LockMutex(g_descriptor_mutex);
    if (!g_reframe_active)
    {
        reset_descriptor_locked(slot);
    }
    g_descriptors_dirty = true;
    SDL_UnlockMutex(g_descriptor_mutex);
}

void world_runtime_drop_pooled_chunk_slot_internal(Uint32 slot)
{
    if (!g_descriptor_mutex)
    {
        return;
    }
    CHECK(slot < kChunkSlotCount);

    SDL_LockMutex(g_descriptor_mutex);
    chunk_t* chunk = world_get_chunk_slot_internal(static_cast<int>(slot / MAX_WORLD_WIDTH),
                                                   static_cast<int>(slot % MAX_WORLD_WIDTH));
    if (chunk && chunk->slot_id == slot)
    {
        clear_chunk_render_metadata(chunk);
    }
    reset_descriptor_locked(slot);
    g_descriptors_dirty = true;
    SDL_UnlockMutex(g_descriptor_mutex);
}

void world_runtime_commit_pooled_chunk_slot_reframe_internal(void)
{
    if (!g_descriptor_mutex)
    {
        return;
    }
    SDL_LockMutex(g_descriptor_mutex);
    CHECK(g_reframe_active);
    sync_all_descriptors_locked();
    g_reframe_active = false;
    g_descriptors_dirty = true;
    SDL_UnlockMutex(g_descriptor_mutex);
}
