#include <SDL3/SDL.h>

#include "render/buffer/buffer.h"
#include "core/check.h"
#include "world/chunks/state.h"
#include "world/runtime/private.h"

namespace {

auto first_non_empty_light_buffer(chunk_t* chunk, cpu_buffer_t* skylight, Uint32 dirty_flags)
    -> gpu_buffer_t*
{
    if ((dirty_flags & LIGHT_DIRTY_SKYLIGHT) && skylight && skylight->size)
    {
        return &chunk->gpu_skylight;
    }
    return nullptr;
}

} // namespace

bool world_chunk_runtime_prepare_lights(chunk_t* chunk,
                                        const cpu_buffer_t* skylight,
                                        Uint32 dirty_flags,
                                        Uint32* skylight_byte_count)
{
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    CHECK(SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING);

    *skylight_byte_count = (dirty_flags & LIGHT_DIRTY_SKYLIGHT) && skylight ? skylight->size : 0u;
    return true;
}

gpu_buffer_t* world_chunk_runtime_first_light_upload_buffer(chunk_t* chunk,
                                                           const cpu_buffer_t* skylight,
                                                           Uint32 dirty_flags)
{
    return first_non_empty_light_buffer(chunk,
                                        const_cast<cpu_buffer_t*>(skylight),
                                        dirty_flags);
}

bool world_chunk_runtime_stage_prepared_lights(chunk_t* chunk,
                                               cpu_buffer_t* skylight,
                                               Uint32 dirty_flags)
{
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    CHECK(SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING);

    bool upload_ok = true;
    if (dirty_flags & LIGHT_DIRTY_SKYLIGHT)
    {
        if (skylight && skylight->size)
        {
            upload_ok = gpu_buffer_upload(&chunk->gpu_skylight, skylight) && upload_ok;
        }
        else
        {
            gpu_buffer_clear(&chunk->gpu_skylight);
        }
    }
    return upload_ok;
}

bool world_chunk_runtime_commit_lights(chunk_t* chunk,
                                       Uint32 skylight_byte_count,
                                       Uint32 dirty_flags)
{
    return world_runtime_commit_chunk_lighting_data_internal(chunk,
                                                            skylight_byte_count,
                                                            dirty_flags);
}

bool world_chunk_runtime_upload_lights(chunk_t* chunk, cpu_buffer_t* skylight, Uint32 dirty_flags)
{
    Uint32 skylight_byte_count = 0;
    if (!world_chunk_runtime_prepare_lights(chunk, skylight, dirty_flags, &skylight_byte_count))
    {
        return false;
    }

    gpu_buffer_t* first_upload = world_chunk_runtime_first_light_upload_buffer(chunk, skylight, dirty_flags);
    const bool needs_submit = first_upload != nullptr;
    if (needs_submit && !gpu_buffer_begin_upload(first_upload))
    {
        return false;
    }
    if (!world_chunk_runtime_stage_prepared_lights(chunk, skylight, dirty_flags))
    {
        if (needs_submit)
        {
            gpu_buffer_abort_upload(first_upload);
        }
        return false;
    }
    if (needs_submit && !gpu_buffer_end_upload(first_upload))
    {
        return false;
    }
    return world_chunk_runtime_commit_lights(chunk, skylight_byte_count, dirty_flags);
}
