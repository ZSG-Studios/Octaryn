#pragma once

#include <vector>

#include "render/world/voxel.h"
#include "world/chunks/snapshot.h"

void world_chunk_build_meshes(const neighborhood_snapshot_t& snapshot, cpu_buffer_t meshes[MESH_TYPE_COUNT]);
void world_chunk_build_mesh_vectors(const neighborhood_snapshot_t& snapshot, std::vector<voxel_t> meshes[MESH_TYPE_COUNT]);
