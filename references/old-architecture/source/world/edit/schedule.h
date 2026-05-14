#pragma once

#include "world/runtime/world.h"

void world_edit_request_urgent_mesh_regen_for_chunk(int chunk_x, int chunk_z);
void world_edit_request_urgent_mesh_regen_for_edit(int chunk_x, int chunk_z, int local_x, int local_z);
void world_edit_request_skylight_regen_for_chunk(int chunk_x, int chunk_z);
void world_edit_request_skylight_regen_for_chunks(const bool dirty_chunks[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH]);
void world_edit_request_skylight_regen_for_edit(int chunk_x, int chunk_z, int local_x, int local_z);
