#pragma once

#include "world/runtime/private.h"

bool world_chunk_upload_direct_meshes(chunk_t* chunk, cpu_buffer_t face_uploads[MESH_TYPE_COUNT]);
bool world_chunk_prepare_direct_meshes(chunk_t* chunk,
                                       cpu_buffer_t face_uploads[MESH_TYPE_COUNT],
                                       Uint32 counts[MESH_TYPE_COUNT]);
gpu_buffer_t* world_chunk_first_direct_mesh_upload_buffer(chunk_t* chunk, cpu_buffer_t face_uploads[MESH_TYPE_COUNT]);
bool world_chunk_stage_prepared_direct_meshes(chunk_t* chunk, cpu_buffer_t face_uploads[MESH_TYPE_COUNT]);
void world_chunk_commit_direct_meshes(chunk_t* chunk, const Uint32 counts[MESH_TYPE_COUNT]);
