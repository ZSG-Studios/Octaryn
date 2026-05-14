#include "render/pipelines/graphics_internal.h"

#include "render/shader/shader.h"

void main_pipelines_graphics_init_mesh_vertex_input(SDL_GPUVertexAttribute (&vertex_attributes)[1],
                                                    SDL_GPUVertexBufferDescription (&vertex_buffers)[1])
{
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    vertex_buffers[0].pitch = 4;
}

void main_pipelines_graphics_init_alpha_blend_target(SDL_GPUColorTargetDescription& color_target,
                                                     SDL_GPUTextureFormat format)
{
    color_target.format = format;
    color_target.blend_state.enable_blend = true;
    color_target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color_target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
}

bool main_pipelines_graphics_create_pipeline(SDL_GPUGraphicsPipeline** pipeline, SDL_GPUDevice* device,
                                             SDL_GPUGraphicsPipelineCreateInfo& info, const char* vertex_path,
                                             const char* fragment_path)
{
    info.vertex_shader = static_cast<SDL_GPUShader*>(shader_load(device, vertex_path));
    info.fragment_shader = static_cast<SDL_GPUShader*>(shader_load(device, fragment_path));
    if (info.vertex_shader && info.fragment_shader)
    {
        *pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return *pipeline != NULL;
}
