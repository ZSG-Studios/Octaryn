#pragma once

#include <stdbool.h>

#include "world/runtime/world.h"
#include "world/chunks/state.h"

typedef struct chunk chunk_t;

bool world_try_get_loaded_chunk_at(const int position[3], int* chunk_x, int* chunk_z, chunk_t** chunk);
bool world_chunk_mesh_ready(chunk_t* chunk);
bool world_chunk_mesh_running(chunk_t* chunk);
int world_chunk_mesh_epoch(chunk_t* chunk);
int world_chunk_last_uploaded_mesh_epoch(chunk_t* chunk);
int world_chunk_light_epoch(chunk_t* chunk);
int world_chunk_last_uploaded_light_epoch(chunk_t* chunk);
bool world_chunk_is_local_index(int cx, int cz);
bool world_chunk_is_border_index(int cx, int cz);
int world_chunk_world_x(const chunk_t* chunk);
int world_chunk_world_z(const chunk_t* chunk);
void world_chunk_to_local(const chunk_t* chunk, int* bx, int* by, int* bz);
block_t world_chunk_get_block(chunk_t* chunk, int bx, int by, int bz);
block_t world_chunk_set_block(chunk_t* chunk, int bx, int by, int bz, block_t block);
void world_apply_pending_block_edits(void);
void world_note_edit_mesh_upload(chunk_t* chunk);
void world_note_edit_light_upload(chunk_t* chunk, Uint32 dirty_flags);
void world_regen_chunk_meshes(int x, int z);
void world_regen_all_chunk_meshes(void);
void world_mark_chunk_urgent(int x, int z);
#define WORLD_LIGHT_DIRTY_BLOCK 0x1
#define WORLD_LIGHT_DIRTY_SKYLIGHT 0x2
#define WORLD_LIGHT_DIRTY_ALL (WORLD_LIGHT_DIRTY_BLOCK | WORLD_LIGHT_DIRTY_SKYLIGHT)
void world_request_chunk_light_regen_flags(int chunk_x, int chunk_z, int flags);
void world_request_chunk_light_regen_neighborhood_flags(int chunk_x, int chunk_z, int flags);
void world_request_chunk_light_regen(int chunk_x, int chunk_z);
void world_request_chunk_light_regen_neighborhood(int chunk_x, int chunk_z);
