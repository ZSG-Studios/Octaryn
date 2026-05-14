#pragma once

#include <SDL3/SDL.h>

#include "world/block/block.h"

#define CHUNK_WIDTH 32
#define CHUNK_HEIGHT 256
#define MAX_WORLD_WIDTH 66
#define OCTARYN_WORLD_JOBS_MAX_WORKERS 16

typedef struct camera camera_t;

typedef enum world_flags
{
    WORLD_FLAGS_OPAQUE      = 0x01,
    WORLD_FLAGS_TRANSPARENT = 0x02,
    WORLD_FLAGS_WATER       = 0x04,
    WORLD_FLAGS_SPRITE      = 0x08,
    WORLD_FLAGS_LAVA        = 0x10,
}
world_flags_t;

typedef struct world_query
{
    block_t block;
    int current[3];
    int previous[3];
}
world_query_t;

typedef struct world_debug_stats
{
    int active_chunks;
    int loaded_chunks;
    int mesh_ready_chunks;
    int chunks_with_opaque_mesh;
    int chunks_with_transparent_mesh;
    int chunks_with_sprite_mesh;
    Uint32 opaque_vertices;
    Uint32 transparent_vertices;
    Uint32 sprite_vertices;
    Uint32 opaque_faces;
    Uint32 transparent_faces;
    Uint32 cube_faces;
    Uint32 dirty_pooled_chunk_slots;
    int running_jobs;
}
world_debug_stats_t;

typedef struct world_block_render_debug
{
    bool loaded;
    int chunk_x;
    int chunk_z;
    Uint32 slot_id;
    Uint32 opaque_faces;
    Uint32 transparent_faces;
    Uint32 sprite_faces;
    Uint32 skylight_count;
}
world_block_render_debug_t;

typedef struct world_edit_debug_stats
{
    Uint64 queued;
    Uint64 applied;
    Uint64 changed;
    Uint64 deferred_attempts;
    Uint32 pending;
}
world_edit_debug_stats_t;

void world_init(SDL_GPUDevice* device);
void world_free(void);
void world_set_render_distance(const camera_t* camera, int chunks);
void world_regen_all_chunk_meshes(void);
void world_reset_window(const camera_t* camera);
int world_get_render_distance(void);
bool world_has_pending_window_transition(void);
void world_update(const camera_t* camera);
void world_render(const camera_t* camera, SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, world_flags_t flags);
int world_count_visible_mesh_chunks(const camera_t* camera);
int world_count_visible_chunks_missing_skylight(const camera_t* camera);
int world_get_recent_hidden_blocks(int out_blocks[][4], int max_count);
int world_get_generation_worker_count(void);
int world_get_generation_worker_limit(void);
void world_set_generation_worker_count(int count);
block_t world_get_block(const int position[3]);
bool world_try_get_surface_height(int world_x, int world_z, int* surface_y, block_t* surface_block);
void world_get_debug_stats(world_debug_stats_t* out_stats);
void world_get_edit_debug_stats(world_edit_debug_stats_t* out_stats);
bool world_edit_recent_changes(Uint64 window_ns);
bool world_get_block_render_debug(const int position[3], world_block_render_debug_t* out_debug);
bool world_queue_block_edit(const int position[3], block_t block);
bool world_set_block(const int position[3], block_t block);
world_query_t world_raycast(const camera_t* camera, float length);
