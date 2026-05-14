#pragma once

#include "world/runtime/private.h"

bool world_chunk_runtime_upload_meshes(chunk_t* chunk, cpu_buffer_t meshes[MESH_TYPE_COUNT]);
bool world_chunk_runtime_prepare_meshes(chunk_t* chunk,
                                        cpu_buffer_t meshes[MESH_TYPE_COUNT],
                                        Uint32 counts[MESH_TYPE_COUNT]);
gpu_buffer_t* world_chunk_runtime_first_mesh_upload_buffer(chunk_t* chunk, cpu_buffer_t meshes[MESH_TYPE_COUNT]);
bool world_chunk_runtime_stage_prepared_meshes(chunk_t* chunk, cpu_buffer_t meshes[MESH_TYPE_COUNT]);
void world_chunk_runtime_commit_meshes(chunk_t* chunk, const Uint32 counts[MESH_TYPE_COUNT]);
