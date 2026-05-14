#include "world/edit/internal.h"

#include "core/env.h"
#include "core/persistence/persistence.h"
#include "core/profile.h"
#include "world/edit/bookkeeping.h"
#include "world/edit/environment.h"
#include "world/edit/light_dirty.h"
#include "world/edit/schedule.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"

world_block_edit_apply_result_t world_try_apply_block_edit_detailed(const int position[3], block_t block)
{
    const Uint64 edit_start = oct_profile_now_ticks();
    Uint64 stage_start = edit_start;
    world_block_edit_apply_result_t result = {};
    if (position[1] < 0 || position[1] >= CHUNK_HEIGHT)
    {
        return result;
    }

    int chunk_x = 0;
    int chunk_z = 0;
    chunk_t* chunk = NULL;
    if (!world_try_get_loaded_chunk_at(position, &chunk_x, &chunk_z, &chunk))
    {
        return result;
    }
    const float locate_ms = oct_profile_elapsed_ms(stage_start);
    stage_start = oct_profile_now_ticks();
    if (SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_RUNNING ||
        SDL_GetAtomicInt(&chunk->light_state) == JOB_STATE_RUNNING)
    {
        return result;
    }
    if (!world_edit_is_supported_block(block, position))
    {
        return result;
    }
    const float readiness_ms = oct_profile_elapsed_ms(stage_start);

    stage_start = oct_profile_now_ticks();
    persistence_set_block(world_chunk_world_x(chunk), world_chunk_world_z(chunk), position[0], position[1], position[2], block);
    const float persistence_ms = oct_profile_elapsed_ms(stage_start);

    stage_start = oct_profile_now_ticks();
    int local_x = position[0];
    int local_y = position[1];
    int local_z = position[2];
    world_chunk_to_local(chunk, &local_x, &local_y, &local_z);
    block_t old_block = world_chunk_set_block(chunk, position[0], position[1], position[2], block);
    if (old_block == block)
    {
        result.applied = true;
        return result;
    }
    result.applied = true;
    result.changed = true;
    const float block_ms = oct_profile_elapsed_ms(stage_start);

    stage_start = oct_profile_now_ticks();
    world_edit_request_urgent_mesh_regen_for_edit(chunk_x, chunk_z, local_x, local_z);
    const float mesh_schedule_ms = oct_profile_elapsed_ms(stage_start);

    stage_start = oct_profile_now_ticks();
    if (old_block != BLOCK_EMPTY)
    {
        world_edit_add_hidden_render_block(position,
                                           world_chunk_world_x(chunk),
                                           world_chunk_world_z(chunk),
                                           world_chunk_mesh_epoch(chunk));
    }
    else
    {
        world_edit_remove_hidden_render_block(position);
    }
    const float hidden_ms = oct_profile_elapsed_ms(stage_start);

    stage_start = oct_profile_now_ticks();
    const Uint32 light_flags = world_edit_get_light_dirty_flags(old_block, block);
    if (light_flags != 0)
    {
        if (light_flags & WORLD_LIGHT_DIRTY_BLOCK)
        {
            world_request_chunk_light_regen_neighborhood_flags(chunk_x, chunk_z, WORLD_LIGHT_DIRTY_BLOCK);
        }
        if (light_flags & WORLD_LIGHT_DIRTY_SKYLIGHT)
        {
            world_edit_request_skylight_regen_for_edit(chunk_x, chunk_z, local_x, local_z);
        }
    }
    const float light_schedule_ms = oct_profile_elapsed_ms(stage_start);

    stage_start = oct_profile_now_ticks();
    world_edit_begin_trace(position,
                           world_chunk_world_x(chunk),
                           world_chunk_world_z(chunk),
                           block,
                           static_cast<Uint32>(light_flags),
                           world_chunk_mesh_epoch(chunk),
                           light_flags != 0 ? world_chunk_light_epoch(chunk) : 0);
    const float trace_ms = oct_profile_elapsed_ms(stage_start);

    stage_start = oct_profile_now_ticks();
    if (position[1] + 1 < CHUNK_HEIGHT)
    {
        int above[3] = {position[0], position[1] + 1, position[2]};
        block_t above_block = world_get_block(above);
        if (above_block != BLOCK_EMPTY && !world_edit_is_supported_block(above_block, above))
        {
            world_set_block(above, BLOCK_EMPTY);
        }
    }
    const float above_ms = oct_profile_elapsed_ms(stage_start);

    stage_start = oct_profile_now_ticks();
    if (world_edit_should_update_water_region(position, old_block, block))
    {
        world_edit_update_water_region(position);
    }
    const float water_ms = oct_profile_elapsed_ms(stage_start);
    const float total_ms = oct_profile_elapsed_ms(edit_start);
    if ((oct_env_flag_enabled("OCTARYN_LOG_WORLD_TIMING") || oct_env_flag_enabled("OCTARYN_LOG_RENDER_PROFILE")) &&
        total_ms >= 0.75f)
    {
        SDL_Log("Block edit timing | total=%.2f locate=%.2f ready=%.2f persist=%.2f block=%.2f mesh=%.2f hidden=%.2f light=%.2f trace=%.2f above=%.2f water=%.2f pos=(%d,%d,%d) old=%d new=%d flags=%u",
                total_ms,
                locate_ms,
                readiness_ms,
                persistence_ms,
                block_ms,
                mesh_schedule_ms,
                hidden_ms,
                light_schedule_ms,
                trace_ms,
                above_ms,
                water_ms,
                position[0],
                position[1],
                position[2],
                static_cast<int>(old_block),
                static_cast<int>(block),
                light_flags);
    }
    return result;
}

bool world_try_apply_block_edit(const int position[3], block_t block)
{
    return world_try_apply_block_edit_detailed(position, block).applied;
}
