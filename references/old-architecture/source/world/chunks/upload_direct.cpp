#include "world/chunks/upload_direct.h"

#include "core/check.h"
#include "render/buffer/buffer.h"
#include "render/world/voxel.h"
#include "world/runtime/internal.h"

namespace {

bool validate_sprite_upload(chunk_t* chunk, const cpu_buffer_t* sprites)
{
    if (!sprites || sprites->size == 0)
    {
        return true;
    }
    if (!sprites->data || sprites->stride != sizeof(sprite_voxel_t))
    {
        SDL_Log("Unsupported direct sprite payload for chunk slot %u: stride=%u size=%u",
                chunk->slot_id,
                sprites ? sprites->stride : 0u,
                sprites ? sprites->size : 0u);
        return false;
    }
    return true;
}

bool validate_water_upload(chunk_t* chunk, const cpu_buffer_t* water)
{
    if (!water || water->size == 0)
    {
        return true;
    }
    if (!water->data || water->stride != sizeof(water_vertex_t))
    {
        SDL_Log("Unsupported direct water payload for chunk slot %u: stride=%u size=%u",
                chunk->slot_id,
                water ? water->stride : 0u,
                water ? water->size : 0u);
        return false;
    }
    return true;
}

gpu_buffer_t* first_non_empty_mesh_buffer(chunk_t* chunk, cpu_buffer_t face_uploads[MESH_TYPE_COUNT])
{
    for (int i = MESH_TYPE_OPAQUE; i <= MESH_TYPE_LAVA; ++i)
    {
        if (face_uploads[i].size > 0)
        {
            return &chunk->gpu_meshes[i];
        }
    }
    if (face_uploads[MESH_TYPE_SPRITE].size > 0)
    {
        return &chunk->gpu_meshes[MESH_TYPE_SPRITE];
    }
    return nullptr;
}

void update_chunk_mesh_counts(chunk_t* chunk, const Uint32 counts[MESH_TYPE_COUNT])
{
    for (int i = 0; i < MESH_TYPE_COUNT; ++i)
    {
        chunk->pooled_face_offsets[i] = 0;
        chunk->pooled_face_counts[i] = counts[i];
    }
}

} // namespace

bool world_chunk_prepare_direct_meshes(chunk_t* chunk,
                                       cpu_buffer_t face_uploads[MESH_TYPE_COUNT],
                                       Uint32 counts[MESH_TYPE_COUNT])
{
    CHECK(chunk);
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    CHECK(SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_RUNNING);

    if (!validate_water_upload(chunk, &face_uploads[MESH_TYPE_WATER]) ||
        !validate_water_upload(chunk, &face_uploads[MESH_TYPE_LAVA]) ||
        !validate_sprite_upload(chunk, &face_uploads[MESH_TYPE_SPRITE]))
    {
        return false;
    }

    counts[MESH_TYPE_OPAQUE] = face_uploads[MESH_TYPE_OPAQUE].size;
    counts[MESH_TYPE_TRANSPARENT] = face_uploads[MESH_TYPE_TRANSPARENT].size;
    counts[MESH_TYPE_WATER] = face_uploads[MESH_TYPE_WATER].size;
    counts[MESH_TYPE_LAVA] = face_uploads[MESH_TYPE_LAVA].size;
    counts[MESH_TYPE_SPRITE] = face_uploads[MESH_TYPE_SPRITE].size;

    if (face_uploads[MESH_TYPE_SPRITE].size > 0)
    {
        if (!world_gen_indices_internal(face_uploads[MESH_TYPE_SPRITE].size))
        {
            return false;
        }
    }
    return true;
}

gpu_buffer_t* world_chunk_first_direct_mesh_upload_buffer(chunk_t* chunk, cpu_buffer_t face_uploads[MESH_TYPE_COUNT])
{
    return first_non_empty_mesh_buffer(chunk, face_uploads);
}

bool world_chunk_stage_prepared_direct_meshes(chunk_t* chunk, cpu_buffer_t face_uploads[MESH_TYPE_COUNT])
{
    CHECK(chunk);
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    CHECK(SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_RUNNING);

    bool upload_ok = true;
    for (int i = MESH_TYPE_OPAQUE; i <= MESH_TYPE_LAVA; ++i)
    {
        if (face_uploads[i].size > 0)
        {
            upload_ok = gpu_buffer_upload(&chunk->gpu_meshes[i], &face_uploads[i]) && upload_ok;
        }
        else
        {
            gpu_buffer_clear(&chunk->gpu_meshes[i]);
        }
    }
    if (face_uploads[MESH_TYPE_SPRITE].size > 0)
    {
        upload_ok = gpu_buffer_upload(&chunk->gpu_meshes[MESH_TYPE_SPRITE], &face_uploads[MESH_TYPE_SPRITE]) &&
            upload_ok;
    }
    else
    {
        gpu_buffer_clear(&chunk->gpu_meshes[MESH_TYPE_SPRITE]);
    }
    return upload_ok;
}

void world_chunk_commit_direct_meshes(chunk_t* chunk, const Uint32 counts[MESH_TYPE_COUNT])
{
    update_chunk_mesh_counts(chunk, counts);
}

bool world_chunk_upload_direct_meshes(chunk_t* chunk, cpu_buffer_t face_uploads[MESH_TYPE_COUNT])
{
    Uint32 counts[MESH_TYPE_COUNT] = {};
    if (!world_chunk_prepare_direct_meshes(chunk, face_uploads, counts))
    {
        return false;
    }

    gpu_buffer_t* first_upload = world_chunk_first_direct_mesh_upload_buffer(chunk, face_uploads);
    const bool needs_submit = first_upload != nullptr;
    if (needs_submit && !gpu_buffer_begin_upload(first_upload))
    {
        return false;
    }

    if (!world_chunk_stage_prepared_direct_meshes(chunk, face_uploads))
    {
        if (needs_submit)
        {
            gpu_buffer_abort_upload(first_upload);
        }
        return false;
    }
    if (needs_submit && !gpu_buffer_end_upload(first_upload))
    {
        return false;
    }

    world_chunk_commit_direct_meshes(chunk, counts);
    return true;
}
