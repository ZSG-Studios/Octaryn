#include <SDL3/SDL.h>

#include "core/check.h"
#include "render/world/voxel.h"
#include "world/chunks/upload_direct.h"
#include "world/chunks/upload_mesh.h"
#include "world/runtime/private.h"

namespace {

bool validate_cube_payload(chunk_t* chunk, const cpu_buffer_t* mesh, mesh_type_t mesh_type)
{
    if (mesh->size == 0)
    {
        return true;
    }
    if (!mesh->data ||
        mesh->payload != CPU_BUFFER_PAYLOAD_PACKED_FACE ||
        mesh->stride != sizeof(packed_face_t))
    {
        SDL_Log("Unsupported packed cube mesh payload for chunk slot %u: mesh=%d stride=%u size=%u",
                chunk->slot_id,
                static_cast<int>(mesh_type),
                mesh->stride,
                mesh->size);
        return false;
    }
    return true;
}

bool validate_sprite_payload(chunk_t* chunk, const cpu_buffer_t* mesh)
{
    if (mesh->size == 0)
    {
        return true;
    }
    if (!mesh->data ||
        mesh->payload != CPU_BUFFER_PAYLOAD_SPRITE_VOXEL ||
        mesh->stride != sizeof(sprite_voxel_t))
    {
        SDL_Log("Unsupported sprite mesh payload for chunk slot %u: stride=%u size=%u",
                chunk->slot_id,
                mesh->stride,
                mesh->size);
        return false;
    }
    return true;
}

bool validate_water_payload(chunk_t* chunk, const cpu_buffer_t* mesh)
{
    if (mesh->size == 0)
    {
        return true;
    }
    if (!mesh->data ||
        mesh->payload != CPU_BUFFER_PAYLOAD_WATER_VERTEX ||
        mesh->stride != sizeof(water_vertex_t))
    {
        SDL_Log("Unsupported water mesh payload for chunk slot %u: stride=%u size=%u",
                chunk->slot_id,
                mesh->stride,
                mesh->size);
        return false;
    }
    return true;
}

} // namespace

bool world_chunk_runtime_upload_meshes(chunk_t* chunk, cpu_buffer_t meshes[MESH_TYPE_COUNT])
{
    Uint32 counts[MESH_TYPE_COUNT] = {};
    if (!world_chunk_runtime_prepare_meshes(chunk, meshes, counts))
    {
        return false;
    }

    gpu_buffer_t* first_upload = world_chunk_runtime_first_mesh_upload_buffer(chunk, meshes);
    const bool needs_submit = first_upload != nullptr;
    if (needs_submit && !gpu_buffer_begin_upload(first_upload))
    {
        return false;
    }
    if (!world_chunk_runtime_stage_prepared_meshes(chunk, meshes))
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

    world_chunk_runtime_commit_meshes(chunk, counts);
    return true;
}

bool world_chunk_runtime_prepare_meshes(chunk_t* chunk,
                                        cpu_buffer_t meshes[MESH_TYPE_COUNT],
                                        Uint32 counts[MESH_TYPE_COUNT])
{
    CHECK(SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED);
    CHECK(SDL_GetAtomicInt(&chunk->mesh_state) == JOB_STATE_RUNNING);

    if (!validate_cube_payload(chunk, &meshes[MESH_TYPE_OPAQUE], MESH_TYPE_OPAQUE) ||
        !validate_cube_payload(chunk, &meshes[MESH_TYPE_TRANSPARENT], MESH_TYPE_TRANSPARENT) ||
        !validate_water_payload(chunk, &meshes[MESH_TYPE_WATER]) ||
        !validate_water_payload(chunk, &meshes[MESH_TYPE_LAVA]) ||
        !validate_sprite_payload(chunk, &meshes[MESH_TYPE_SPRITE]))
    {
        return false;
    }

    return world_chunk_prepare_direct_meshes(chunk, meshes, counts);
}

gpu_buffer_t* world_chunk_runtime_first_mesh_upload_buffer(chunk_t* chunk, cpu_buffer_t meshes[MESH_TYPE_COUNT])
{
    return world_chunk_first_direct_mesh_upload_buffer(chunk, meshes);
}

bool world_chunk_runtime_stage_prepared_meshes(chunk_t* chunk, cpu_buffer_t meshes[MESH_TYPE_COUNT])
{
    return world_chunk_stage_prepared_direct_meshes(chunk, meshes);
}

void world_chunk_runtime_commit_meshes(chunk_t* chunk, const Uint32 counts[MESH_TYPE_COUNT])
{
    world_chunk_commit_direct_meshes(chunk, counts);
}
