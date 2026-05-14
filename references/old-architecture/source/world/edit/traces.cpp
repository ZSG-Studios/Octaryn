#include "world/edit/bookkeeping.h"

#include <SDL3/SDL.h>

#include <vector>

#include "core/env.h"
#include "core/profile.h"
#include "world/runtime/internal.h"

namespace {

struct edit_trace_t
{
    int position[3];
    int chunk_world_x;
    int chunk_world_z;
    block_t expected_block;
    bool expected_solid;
    Uint32 light_dirty_flags;
    int target_mesh_epoch;
    int target_light_epoch;
    Uint64 submitted_at_ticks;
    bool mesh_logged;
    bool light_logged;
};

std::vector<edit_trace_t> g_edit_traces;

constexpr Uint64 kEditTraceWatchdogNs = 10ull * SDL_NS_PER_SECOND;

bool edit_trace_enabled()
{
    static const bool enabled = oct_env_flag_enabled("OCTARYN_EDIT_TRACE");
    return enabled;
}

void prune_edit_traces(Uint64 now_ticks)
{
    for (size_t i = 0; i < g_edit_traces.size();)
    {
        const edit_trace_t& trace = g_edit_traces[i];
        const bool done = trace.mesh_logged && (trace.light_logged || trace.light_dirty_flags == 0);
        const bool expired = (now_ticks - trace.submitted_at_ticks) >= kEditTraceWatchdogNs;
        if (done || expired)
        {
            g_edit_traces[i] = g_edit_traces.back();
            g_edit_traces.pop_back();
            continue;
        }
        ++i;
    }
}

} // namespace

void world_edit_begin_trace(const int position[3],
                            int chunk_world_x,
                            int chunk_world_z,
                            block_t expected_block,
                            Uint32 light_dirty_flags,
                            int target_mesh_epoch,
                            int target_light_epoch)
{
    if (!edit_trace_enabled())
    {
        return;
    }
    OCT_PROFILE_ZONE("world_edit_trace_begin");
    const Uint64 now_ticks = SDL_GetTicksNS();
    prune_edit_traces(now_ticks);
    g_edit_traces.push_back({
        {position[0], position[1], position[2]},
        chunk_world_x,
        chunk_world_z,
        expected_block,
        block_is_solid(expected_block),
        light_dirty_flags,
        target_mesh_epoch,
        target_light_epoch,
        now_ticks,
        false,
        light_dirty_flags == 0,
    });
    const block_t actual = world_get_block(position);
    const bool block_ok = actual == expected_block;
    const bool collision_source_ok = block_is_solid(actual) == block_is_solid(expected_block);
    SDL_Log("Edit validation | apply (%d, %d, %d) expected=%d actual=%d block_ok=%d solid_ok=%d mesh_epoch=%d light_epoch=%d flags=%u",
            position[0],
            position[1],
            position[2],
            expected_block,
            actual,
            block_ok,
            collision_source_ok,
            target_mesh_epoch,
            target_light_epoch,
            light_dirty_flags);
}

void world_note_edit_mesh_upload(chunk_t* chunk)
{
    if (!edit_trace_enabled())
    {
        return;
    }
    OCT_PROFILE_ZONE("world_edit_trace_mesh_upload");
    const Uint64 now_ticks = SDL_GetTicksNS();
    for (edit_trace_t& trace : g_edit_traces)
    {
        if (trace.mesh_logged)
        {
            continue;
        }
        if (trace.chunk_world_x != world_chunk_world_x(chunk) || trace.chunk_world_z != world_chunk_world_z(chunk))
        {
            continue;
        }
        if (!world_chunk_mesh_uploaded_at_least(chunk, trace.target_mesh_epoch))
        {
            continue;
        }
        trace.mesh_logged = true;
        const float latency_ms = static_cast<float>(now_ticks - trace.submitted_at_ticks) * 1e-6f;
        const block_t actual = world_get_block(trace.position);
        SDL_Log("Edit validation | mesh upload (%d, %d, %d) latency=%.2f ms expected=%d actual=%d hidden=%d",
                trace.position[0],
                trace.position[1],
                trace.position[2],
                latency_ms,
                trace.expected_block,
                actual,
                trace.expected_block == BLOCK_EMPTY);
    }
    prune_edit_traces(now_ticks);
}

void world_note_edit_light_upload(chunk_t* chunk, Uint32 dirty_flags)
{
    if (!edit_trace_enabled())
    {
        return;
    }
    OCT_PROFILE_ZONE("world_edit_trace_light_upload");
    const Uint64 now_ticks = SDL_GetTicksNS();
    for (edit_trace_t& trace : g_edit_traces)
    {
        if (trace.light_logged || trace.light_dirty_flags == 0)
        {
            continue;
        }
        if (trace.chunk_world_x != world_chunk_world_x(chunk) || trace.chunk_world_z != world_chunk_world_z(chunk))
        {
            continue;
        }
        if ((dirty_flags & trace.light_dirty_flags) == 0)
        {
            continue;
        }
        if (!world_chunk_light_uploaded_at_least(chunk, trace.target_light_epoch))
        {
            continue;
        }
        trace.light_logged = true;
        const float latency_ms = static_cast<float>(now_ticks - trace.submitted_at_ticks) * 1e-6f;
        SDL_Log("Edit validation | lighting upload (%d, %d, %d) latency=%.2f ms flags=%u",
                trace.position[0],
                trace.position[1],
                trace.position[2],
                latency_ms,
                trace.light_dirty_flags);
    }
    prune_edit_traces(now_ticks);
}
