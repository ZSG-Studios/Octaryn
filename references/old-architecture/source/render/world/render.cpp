#include <SDL3/SDL.h>

#include "core/profile.h"
#include "core/camera/camera.h"
#include "render/world/debug.h"
#include "world/chunks/state.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"

namespace {

constexpr Uint32 kDrawFlagUseFaceBuffer = 0x2u;

typedef struct chunk_uniforms
{
    Sint32 chunk_position[2];
    Uint32 chunk_descriptor_index;
    Uint32 draw_flags;
}
chunk_uniforms_t;

mesh_type_t get_mesh_for_flags(world_flags_t flags)
{
    if (flags & WORLD_FLAGS_SPRITE)
    {
        return MESH_TYPE_SPRITE;
    }
    if (flags & WORLD_FLAGS_WATER)
    {
        return MESH_TYPE_WATER;
    }
    if (flags & WORLD_FLAGS_LAVA)
    {
        return MESH_TYPE_LAVA;
    }
    if (flags & WORLD_FLAGS_OPAQUE)
    {
        return MESH_TYPE_OPAQUE;
    }
    return MESH_TYPE_TRANSPARENT;
}

bool use_global_cube_path(world_flags_t flags)
{
    return (flags & (WORLD_FLAGS_SPRITE | WORLD_FLAGS_WATER | WORLD_FLAGS_LAVA)) == 0;
}

bool chunk_is_near_camera_for_culling(const camera_t* camera, const chunk_t* chunk)
{
    if (!camera || !chunk)
    {
        return false;
    }

    const float margin = static_cast<float>(CHUNK_WIDTH);
    const float min_x = static_cast<float>(chunk->position[0]) - margin;
    const float max_x = static_cast<float>(chunk->position[0] + CHUNK_WIDTH) + margin;
    const float min_z = static_cast<float>(chunk->position[1]) - margin;
    const float max_z = static_cast<float>(chunk->position[1] + CHUNK_WIDTH) + margin;
    return camera->position[0] >= min_x &&
        camera->position[0] <= max_x &&
        camera->position[2] >= min_z &&
        camera->position[2] <= max_z;
}

bool bind_direct_cube_face_buffer(world_flags_t flags,
                                  chunk_t* chunk,
                                  SDL_GPUCommandBuffer* cbuf,
                                  SDL_GPURenderPass* pass,
                                  Uint32 draw_flags)
{
    const gpu_buffer_t* gpu_cube_faces = &chunk->gpu_meshes[get_mesh_for_flags(flags)];
    const gpu_buffer_t* gpu_descriptors = world_gpu_chunk_descriptors_internal();
    if (!gpu_cube_faces->buffer || gpu_cube_faces->size == 0)
    {
        return false;
    }
    if (!gpu_descriptors->buffer)
    {
        return false;
    }

    SDL_GPUBuffer* storage_buffers[2] = {
        gpu_cube_faces->buffer,
        gpu_descriptors->buffer,
    };
    SDL_BindGPUVertexStorageBuffers(pass, 0, storage_buffers, 2);

    chunk_uniforms_t uniforms = {};
    uniforms.chunk_position[0] = chunk->position[0];
    uniforms.chunk_position[1] = chunk->position[1];
    uniforms.chunk_descriptor_index = world_chunk_descriptor_index_internal(chunk);
    uniforms.draw_flags = draw_flags;
    SDL_PushGPUVertexUniformData(cbuf, 2, &uniforms, sizeof(uniforms));
    return true;
}

void render_direct_cube_chunk(chunk_t* chunk, SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, world_flags_t flags)
{
    const mesh_type_t mesh_type = get_mesh_for_flags(flags);
    const Uint32 face_count = chunk->gpu_meshes[mesh_type].size;
    if (face_count == 0)
    {
        return;
    }
    const Uint32 draw_flags = kDrawFlagUseFaceBuffer;
    if (!bind_direct_cube_face_buffer(flags, chunk, cbuf, pass, draw_flags))
    {
        return;
    }

    SDL_DrawGPUPrimitives(pass, face_count * 6u, 1, 0, 0);
    render_world_debug_note_direct_cube_draw(face_count);
}

void render_direct_sprite_chunk(chunk_t* chunk, SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass)
{
    gpu_buffer_t* gpu_mesh = &chunk->gpu_meshes[MESH_TYPE_SPRITE];
    if (gpu_mesh->size == 0)
    {
        return;
    }

    const gpu_buffer_t* gpu_indices = world_gpu_indices_internal();
    const gpu_buffer_t* gpu_descriptors = world_gpu_chunk_descriptors_internal();
    if (!gpu_indices->buffer || gpu_indices->size == 0 || !gpu_descriptors->buffer)
    {
        return;
    }
    SDL_GPUBuffer* storage_buffers[2] = {
        gpu_mesh->buffer,
        const_cast<SDL_GPUBuffer*>(gpu_descriptors->buffer),
    };
    SDL_GPUBufferBinding index_binding = {};
    index_binding.buffer = gpu_indices->buffer;

    chunk_uniforms_t uniforms = {};
    uniforms.chunk_position[0] = chunk->position[0];
    uniforms.chunk_position[1] = chunk->position[1];
    uniforms.chunk_descriptor_index = world_chunk_descriptor_index_internal(chunk);
    uniforms.draw_flags = 0u;

    SDL_PushGPUVertexUniformData(cbuf, 2, &uniforms, sizeof(uniforms));
    SDL_BindGPUVertexStorageBuffers(pass, 0, storage_buffers, 2);
    SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_DrawGPUIndexedPrimitives(pass, (gpu_mesh->size * 3u) / 2u, 1, 0, 0, 0);
    render_world_debug_note_sprite_draw(gpu_mesh->size);
}

void render_direct_fluid_chunk(chunk_t* chunk, SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, mesh_type_t mesh_type)
{
    gpu_buffer_t* gpu_mesh = &chunk->gpu_meshes[mesh_type];
    if (gpu_mesh->size == 0)
    {
        return;
    }

    const gpu_buffer_t* gpu_descriptors = world_gpu_chunk_descriptors_internal();
    if (!gpu_mesh->buffer || !gpu_descriptors->buffer)
    {
        return;
    }
    SDL_GPUBuffer* storage_buffers[2] = {
        gpu_mesh->buffer,
        const_cast<SDL_GPUBuffer*>(gpu_descriptors->buffer),
    };
    chunk_uniforms_t uniforms = {};
    uniforms.chunk_position[0] = chunk->position[0];
    uniforms.chunk_position[1] = chunk->position[1];
    uniforms.chunk_descriptor_index = world_chunk_descriptor_index_internal(chunk);
    uniforms.draw_flags = 0u;

    SDL_PushGPUVertexUniformData(cbuf, 2, &uniforms, sizeof(uniforms));
    SDL_BindGPUVertexStorageBuffers(pass, 0, storage_buffers, 2);
    SDL_DrawGPUPrimitives(pass, gpu_mesh->size, 1, 0, 0);
    render_world_debug_note_direct_cube_draw(gpu_mesh->size / 6u);
}

void render_visible_chunk(chunk_t* chunk, SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, world_flags_t flags)
{
    if (use_global_cube_path(flags))
    {
        render_direct_cube_chunk(chunk, cbuf, pass, flags);
        return;
    }

    if (flags & WORLD_FLAGS_WATER)
    {
        render_direct_fluid_chunk(chunk, cbuf, pass, MESH_TYPE_WATER);
        return;
    }
    if (flags & WORLD_FLAGS_LAVA)
    {
        render_direct_fluid_chunk(chunk, cbuf, pass, MESH_TYPE_LAVA);
        return;
    }

    render_direct_sprite_chunk(chunk, cbuf, pass);
}

} // namespace

void world_render(const camera_t* camera, SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, world_flags_t flags)
{
    const float camera_position[4] = {
        camera->position[0],
        camera->position[1],
        camera->position[2],
        0.0f,
    };
    SDL_PushGPUVertexUniformData(cbuf, 0, camera->proj, 64);
    SDL_PushGPUVertexUniformData(cbuf, 1, camera->view, 64);
    SDL_PushGPUVertexUniformData(cbuf, 3, camera_position, sizeof(camera_position));

    const int total = world_active_world_width_internal() * world_active_world_width_internal();
    const int (*sorted_chunks)[2] = world_sorted_chunks_internal();
    const bool back_to_front = (flags & (WORLD_FLAGS_TRANSPARENT | WORLD_FLAGS_WATER)) != 0;
    const int begin = back_to_front ? total - 1 : 0;
    const int end = back_to_front ? -1 : total;
    const int step = back_to_front ? -1 : 1;
    for (int i = begin; i != end; i += step)
    {
        const int a = sorted_chunks[i][0];
        const int b = sorted_chunks[i][1];
        if (world_chunk_is_border_index(a, b))
        {
            continue;
        }
        chunk_t* chunk = world_get_chunk_internal(a, b);
        if (!world_chunk_scene_ready(chunk))
        {
            continue;
        }
        render_world_debug_note_direct_candidate_chunk();
        const bool use_view_frustum_culling = camera->type != CAMERA_TYPE_ORTHO;
        const bool near_camera = chunk_is_near_camera_for_culling(camera, chunk);
        if (!near_camera &&
            use_view_frustum_culling &&
            !camera_get_vis(camera,
                            static_cast<float>(chunk->position[0]),
                            0.0f,
                            static_cast<float>(chunk->position[1]),
                            static_cast<float>(CHUNK_WIDTH),
                            static_cast<float>(CHUNK_HEIGHT),
                            static_cast<float>(CHUNK_WIDTH)))
        {
            render_world_debug_note_direct_frustum_culled_chunk();
            continue;
        }
        render_visible_chunk(chunk, cbuf, pass, flags);
    }
}
