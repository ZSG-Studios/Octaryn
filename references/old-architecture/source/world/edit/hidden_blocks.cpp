#include "world/edit/bookkeeping.h"

#include <SDL3/SDL.h>

#include <vector>

#include "world/runtime/internal.h"

namespace {

struct hidden_render_block_t
{
    int position[4];
    int chunk_world_x;
    int chunk_world_z;
    int target_mesh_epoch;
    Uint64 created_at_ticks;
};

std::vector<hidden_render_block_t> g_hidden_render_blocks;

constexpr Uint64 kHiddenRenderBlockWatchdogNs = 5ull * SDL_NS_PER_SECOND;

void prune_hidden_render_blocks(Uint64 now_ticks)
{
    for (size_t i = 0; i < g_hidden_render_blocks.size();)
    {
        const hidden_render_block_t& hidden = g_hidden_render_blocks[i];
        int query_position[3] = {hidden.position[0], hidden.position[1], hidden.position[2]};
        int chunk_x = 0;
        int chunk_z = 0;
        chunk_t* chunk = NULL;
        const bool loaded = world_try_get_loaded_chunk_at(query_position, &chunk_x, &chunk_z, &chunk);
        const bool uploaded = loaded && world_chunk_mesh_uploaded_at_least(chunk, hidden.target_mesh_epoch);
        const bool owner_moved = loaded && (world_chunk_world_x(chunk) != hidden.chunk_world_x || world_chunk_world_z(chunk) != hidden.chunk_world_z);
        const bool expired = (now_ticks - hidden.created_at_ticks) >= kHiddenRenderBlockWatchdogNs;
        if (uploaded || owner_moved || expired)
        {
            g_hidden_render_blocks[i] = g_hidden_render_blocks.back();
            g_hidden_render_blocks.pop_back();
            continue;
        }
        ++i;
    }
}

} // namespace

void world_edit_add_hidden_render_block(const int position[3], int chunk_world_x, int chunk_world_z, int target_mesh_epoch)
{
    const Uint64 now_ticks = SDL_GetTicksNS();
    prune_hidden_render_blocks(now_ticks);
    for (hidden_render_block_t& hidden : g_hidden_render_blocks)
    {
        if (hidden.position[0] == position[0] && hidden.position[1] == position[1] && hidden.position[2] == position[2])
        {
            hidden.chunk_world_x = chunk_world_x;
            hidden.chunk_world_z = chunk_world_z;
            hidden.target_mesh_epoch = target_mesh_epoch;
            hidden.created_at_ticks = now_ticks;
            return;
        }
    }
    g_hidden_render_blocks.push_back({{position[0], position[1], position[2], 0}, chunk_world_x, chunk_world_z, target_mesh_epoch, now_ticks});
}

void world_edit_remove_hidden_render_block(const int position[3])
{
    for (size_t i = 0; i < g_hidden_render_blocks.size();)
    {
        const hidden_render_block_t& hidden = g_hidden_render_blocks[i];
        if (hidden.position[0] == position[0] && hidden.position[1] == position[1] && hidden.position[2] == position[2])
        {
            g_hidden_render_blocks[i] = g_hidden_render_blocks.back();
            g_hidden_render_blocks.pop_back();
            continue;
        }
        ++i;
    }
}

int world_get_recent_hidden_blocks(int out_blocks[][4], int max_count)
{
    const Uint64 now_ticks = SDL_GetTicksNS();
    prune_hidden_render_blocks(now_ticks);
    const size_t count = static_cast<size_t>(SDL_min(max_count, static_cast<int>(g_hidden_render_blocks.size())));
    for (size_t i = 0; i < count; ++i)
    {
        out_blocks[i][0] = g_hidden_render_blocks[i].position[0];
        out_blocks[i][1] = g_hidden_render_blocks[i].position[1];
        out_blocks[i][2] = g_hidden_render_blocks[i].position[2];
        out_blocks[i][3] = 0;
    }
    return static_cast<int>(count);
}
