#include "render/world/debug.h"

#include <atomic>

namespace {

std::atomic<Uint64> g_direct_candidate_chunks{0};
std::atomic<Uint64> g_direct_frustum_culled_chunks{0};
std::atomic<Uint64> g_direct_cube_draws{0};
std::atomic<Uint64> g_direct_cube_faces{0};
std::atomic<Uint64> g_sprite_draws{0};
std::atomic<Uint64> g_sprite_quads{0};
std::atomic<Uint64> g_sprite_vertices{0};

Uint64 take_counter(std::atomic<Uint64>& counter)
{
    return counter.exchange(0, std::memory_order_relaxed);
}

} // namespace

void render_world_debug_note_direct_candidate_chunk(void)
{
    g_direct_candidate_chunks.fetch_add(1, std::memory_order_relaxed);
}

void render_world_debug_note_direct_frustum_culled_chunk(void)
{
    g_direct_frustum_culled_chunks.fetch_add(1, std::memory_order_relaxed);
}

void render_world_debug_note_direct_cube_draw(Uint32 face_count)
{
    g_direct_cube_draws.fetch_add(1, std::memory_order_relaxed);
    g_direct_cube_faces.fetch_add(face_count, std::memory_order_relaxed);
}

void render_world_debug_note_sprite_draw(Uint32 vertex_count)
{
    g_sprite_draws.fetch_add(1, std::memory_order_relaxed);
    g_sprite_vertices.fetch_add(vertex_count, std::memory_order_relaxed);
    g_sprite_quads.fetch_add(vertex_count / 4u, std::memory_order_relaxed);
}

void render_world_debug_take_counters(render_world_debug_counters_t* out_counters)
{
    if (!out_counters)
    {
        return;
    }

    *out_counters = {};
    out_counters->direct_candidate_chunks = take_counter(g_direct_candidate_chunks);
    out_counters->direct_frustum_culled_chunks = take_counter(g_direct_frustum_culled_chunks);
    out_counters->direct_cube_draws = take_counter(g_direct_cube_draws);
    out_counters->direct_cube_faces = take_counter(g_direct_cube_faces);
    out_counters->sprite_draws = take_counter(g_sprite_draws);
    out_counters->sprite_quads = take_counter(g_sprite_quads);
    out_counters->sprite_vertices = take_counter(g_sprite_vertices);
}
