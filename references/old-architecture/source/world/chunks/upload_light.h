#pragma once

#include "world/runtime/private.h"

bool world_chunk_runtime_upload_lights(chunk_t* chunk, cpu_buffer_t* skylight, Uint32 dirty_flags);
bool world_chunk_runtime_prepare_lights(chunk_t* chunk,
                                        const cpu_buffer_t* skylight,
                                        Uint32 dirty_flags,
                                        Uint32* skylight_byte_count);
gpu_buffer_t* world_chunk_runtime_first_light_upload_buffer(chunk_t* chunk,
                                                           const cpu_buffer_t* skylight,
                                                           Uint32 dirty_flags);
bool world_chunk_runtime_stage_prepared_lights(chunk_t* chunk,
                                               cpu_buffer_t* skylight,
                                               Uint32 dirty_flags);
bool world_chunk_runtime_commit_lights(chunk_t* chunk,
                                       Uint32 skylight_byte_count,
                                       Uint32 dirty_flags);
