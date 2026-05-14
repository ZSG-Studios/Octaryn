#pragma once

#include <SDL3/SDL.h>

typedef struct render_world_debug_counters
{
    Uint64 direct_candidate_chunks;
    Uint64 direct_frustum_culled_chunks;
    Uint64 direct_cube_draws;
    Uint64 direct_cube_faces;
    Uint64 sprite_draws;
    Uint64 sprite_quads;
    Uint64 sprite_vertices;
}
render_world_debug_counters_t;

void render_world_debug_note_direct_candidate_chunk(void);
void render_world_debug_note_direct_frustum_culled_chunk(void);
void render_world_debug_note_direct_cube_draw(Uint32 face_count);
void render_world_debug_note_sprite_draw(Uint32 vertex_count);
void render_world_debug_take_counters(render_world_debug_counters_t* out_counters);
