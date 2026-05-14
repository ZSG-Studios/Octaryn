#pragma once

#include <SDL3/SDL.h>

#include "world/block/block.h"
#include "render/buffer/buffer.h"
#include "world/runtime/world.h"

typedef enum job_type
{
    JOB_TYPE_NONE,
    JOB_TYPE_QUIT,
    JOB_TYPE_BLOCKS,
    JOB_TYPE_MESHES,
    JOB_TYPE_LIGHTS,
}
job_type_t;

typedef struct job
{
    job_type_t type;
    int x;
    int z;
}
job_t;

typedef enum job_state
{
    JOB_STATE_REQUESTED,
    JOB_STATE_RUNNING,
    JOB_STATE_COMPLETED,
}
job_state_t;

typedef enum mesh_type
{
    MESH_TYPE_OPAQUE,
    MESH_TYPE_TRANSPARENT,
    MESH_TYPE_WATER,
    MESH_TYPE_LAVA,
    MESH_TYPE_SPRITE,
    MESH_TYPE_COUNT,
}
mesh_type_t;

typedef enum light_dirty_flags
{
    LIGHT_DIRTY_BLOCK = 0x1,
    LIGHT_DIRTY_SKYLIGHT = 0x2,
    LIGHT_DIRTY_ALL = LIGHT_DIRTY_BLOCK | LIGHT_DIRTY_SKYLIGHT,
}
light_dirty_flags_t;

typedef Uint64 packed_face_t;

typedef enum chunk_draw_descriptor_flags
{
    CHUNK_DRAW_DESCRIPTOR_HAS_OPAQUE      = 0x01,
    CHUNK_DRAW_DESCRIPTOR_HAS_TRANSPARENT = 0x02,
    CHUNK_DRAW_DESCRIPTOR_HAS_SKYLIGHT    = 0x04,
    CHUNK_DRAW_DESCRIPTOR_RENDER_READY    = 0x08,
}
chunk_draw_descriptor_flags_t;

typedef struct chunk_draw_descriptor
{
    Sint32 chunk_position[2];
    Uint32 slot_id;
    Uint32 flags;
    Uint32 face_offsets[MESH_TYPE_COUNT];
    Uint32 face_counts[MESH_TYPE_COUNT];
    Uint32 skylight_offset;
    Uint32 skylight_count;
}
chunk_draw_descriptor_t;

typedef struct chunk
{
    SDL_AtomicInt block_state;
    SDL_AtomicInt mesh_state;
    SDL_AtomicInt light_state;
    SDL_AtomicInt mesh_reschedule_pending;
    SDL_AtomicInt light_reschedule_pending;
    SDL_AtomicInt light_dirty_flags;
    SDL_AtomicInt mesh_epoch;
    SDL_AtomicInt last_uploaded_mesh_epoch;
    SDL_AtomicInt light_epoch;
    SDL_AtomicInt last_uploaded_light_epoch;
    SDL_AtomicInt render_mesh_valid;
    SDL_AtomicInt urgent_priority;
    SDL_Mutex* data_mutex;
    Sint32 position[2];
    Uint32 slot_id;
    Uint32 pooled_face_offsets[MESH_TYPE_COUNT];
    Uint32 pooled_face_counts[MESH_TYPE_COUNT];
    Uint32 pooled_skylight_offset;
    Uint32 pooled_skylight_count;
    block_t* blocks;
    gpu_buffer_t gpu_meshes[MESH_TYPE_COUNT];
    gpu_buffer_t gpu_skylight;
}
chunk_t;

const gpu_buffer_t* world_gpu_chunk_descriptors_internal(void);
Uint32 world_chunk_face_offset_internal(const chunk_t* chunk, mesh_type_t mesh_type);
Uint32 world_chunk_face_count_internal(const chunk_t* chunk, mesh_type_t mesh_type);
Uint32 world_chunk_descriptor_index_internal(const chunk_t* chunk);
Uint32 world_chunk_skylight_offset_internal(const chunk_t* chunk);
Uint32 world_chunk_skylight_count_internal(const chunk_t* chunk);
Uint32 world_chunk_descriptor_capacity_internal(void);
void world_runtime_render_pools_init_internal(SDL_GPUDevice* device);
void world_runtime_render_pools_free_internal(void);
void world_runtime_flush_chunk_descriptors_internal(void);
Uint32 world_runtime_dirty_pooled_chunk_slot_count_internal(void);
bool world_runtime_upload_chunk_lighting_data_internal(chunk_t* chunk,
                                                       const cpu_buffer_t* skylight,
                                                       Uint32 dirty_flags);
bool world_runtime_commit_chunk_lighting_data_internal(chunk_t* chunk,
                                                       Uint32 skylight_byte_count,
                                                       Uint32 dirty_flags);
void world_runtime_begin_pooled_chunk_slot_reframe_internal(void);
void world_runtime_move_pooled_chunk_slot_internal(Uint32 source_slot, Uint32 destination_slot);
void world_runtime_clear_pooled_chunk_slot_internal(Uint32 slot);
void world_runtime_drop_pooled_chunk_slot_internal(Uint32 slot);
void world_runtime_commit_pooled_chunk_slot_reframe_internal(void);
void world_runtime_sync_chunk_descriptor_internal(chunk_t* chunk);

chunk_t* world_get_chunk_internal(int cx, int cz);
chunk_t* world_get_chunk_slot_internal(int cx, int cz);
void world_set_chunk_internal(int cx, int cz, chunk_t* chunk);
void world_get_neighborhood_internal(int cx, int cz, chunk_t* neighborhood[3][3]);
void world_gen_chunk_blocks_internal(chunk_t* chunk);
void world_build_chunk_meshes_internal(chunk_t* neighborhood[3][3], cpu_buffer_t meshes[MESH_TYPE_COUNT]);
void world_upload_chunk_meshes_internal(chunk_t* chunk, cpu_buffer_t meshes[MESH_TYPE_COUNT]);
bool world_prepare_chunk_meshes_upload_internal(chunk_t* chunk,
                                                cpu_buffer_t meshes[MESH_TYPE_COUNT],
                                                Uint32 counts[MESH_TYPE_COUNT],
                                                Uint64* upload_bytes);
gpu_buffer_t* world_first_chunk_mesh_upload_buffer_internal(chunk_t* chunk, cpu_buffer_t meshes[MESH_TYPE_COUNT]);
bool world_stage_chunk_meshes_upload_internal(chunk_t* chunk, cpu_buffer_t meshes[MESH_TYPE_COUNT]);
void world_commit_chunk_meshes_upload_internal(chunk_t* chunk,
                                               const Uint32 counts[MESH_TYPE_COUNT],
                                               Uint64 upload_bytes);
void world_fail_chunk_meshes_upload_internal(chunk_t* chunk, Uint64 upload_bytes);
void world_build_chunk_lights_internal(chunk_t* neighborhood[3][3], cpu_buffer_t* skylight, Uint32 dirty_flags);
void world_upload_chunk_lights_internal(chunk_t* chunk, cpu_buffer_t* skylight, Uint32 dirty_flags);
bool world_prepare_chunk_lights_upload_internal(chunk_t* chunk,
                                                cpu_buffer_t* skylight,
                                                Uint32 dirty_flags,
                                                Uint32* skylight_byte_count,
                                                Uint64* upload_bytes);
gpu_buffer_t* world_first_chunk_light_upload_buffer_internal(chunk_t* chunk,
                                                            cpu_buffer_t* skylight,
                                                            Uint32 dirty_flags);
bool world_stage_chunk_lights_upload_internal(chunk_t* chunk,
                                              cpu_buffer_t* skylight,
                                              Uint32 dirty_flags);
void world_commit_chunk_lights_upload_internal(chunk_t* chunk,
                                               Uint32 skylight_byte_count,
                                               Uint32 dirty_flags,
                                               Uint64 upload_bytes);
void world_fail_chunk_lights_upload_internal(chunk_t* chunk, Uint32 dirty_flags, Uint64 upload_bytes);
SDL_GPUDevice* world_device_internal(void);
void world_runtime_buffers_init_internal(SDL_GPUDevice* device);
void world_runtime_gen_empty_skylight_internal(void);
void world_runtime_buffers_free_internal(void);
void world_runtime_indices_init_internal(void);
void world_runtime_indices_free_internal(void);
bool world_gen_indices_internal(Uint32 size);
int world_active_world_width_internal(void);
void world_set_active_world_width_internal(int world_width);
const int (*world_sorted_chunks_internal(void))[2];
int (*world_sorted_chunks_mutable_internal(void))[2];
const gpu_buffer_t* world_gpu_indices_internal(void);
const gpu_buffer_t* world_gpu_empty_skylight_internal(void);
int world_origin_x_internal(void);
int world_origin_z_internal(void);
void world_set_origin_internal(int origin_x, int origin_z);
bool world_is_moving_internal(void);
void world_set_is_moving_internal(bool moving);
block_t world_get_neighborhood_block_internal(chunk_t* neighborhood[3][3], int bx, int by, int bz, int dx, int dy, int dz);
void world_chunk_seed_block(chunk_t* chunk, int bx, int by, int bz, block_t block);
bool world_chunk_render_mesh_valid(const chunk_t* chunk);
bool world_chunk_ensure_blocks_allocated(chunk_t* chunk);
void world_chunk_clear_blocks(chunk_t* chunk);
block_t world_chunk_read_local_block(const chunk_t* chunk, int bx, int by, int bz);
void world_chunk_write_local_block(chunk_t* chunk, int bx, int by, int bz, block_t block);

void world_jobs_init(SDL_GPUDevice* device);
void world_jobs_free(void);
int world_jobs_running_count(void);
int world_jobs_cpu_running_count(void);
int world_jobs_get_worker_count(void);
int world_jobs_get_worker_limit(void);
void world_jobs_set_worker_count(int count);
void world_jobs_service_pending_uploads(void);
void world_jobs_update(const camera_t* camera, bool allow_submissions);
