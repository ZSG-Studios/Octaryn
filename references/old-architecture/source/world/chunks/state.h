#pragma once

#include "world/runtime/world.h"

typedef struct chunk chunk_t;

bool world_chunk_blocks_ready(const chunk_t* chunk);
bool world_chunk_mesh_requested(const chunk_t* chunk);
bool world_chunk_light_requested(const chunk_t* chunk);
bool world_chunk_light_job_active(const chunk_t* chunk);
bool world_chunk_light_ready(const chunk_t* chunk);
bool world_chunk_has_uploaded_lighting(const chunk_t* chunk);
bool world_chunk_has_any_gpu_mesh(const chunk_t* chunk);
bool world_chunk_render_ready(const chunk_t* chunk);
bool world_chunk_scene_ready(const chunk_t* chunk);
bool world_chunk_neighborhood_blocks_ready(chunk_t* neighborhood[3][3]);
bool world_chunk_mesh_uploaded_at_least(const chunk_t* chunk, int epoch);
bool world_chunk_light_uploaded_at_least(const chunk_t* chunk, int epoch);
