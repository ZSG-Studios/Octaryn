#include "render/postprocess/composite.h"

#include "render/scene/pass_shared.h"
#include "render/postprocess/composite_internal.h"
#include "core/profile.h"

namespace {

void dispatch_viewport_compute(SDL_GPUComputePass* compute_pass, const main_render_pass_context_t* context)
{
    int groups_x = (context->player->camera.viewport_size[0] + 8 - 1) / 8;
    int groups_y = (context->player->camera.viewport_size[1] + 8 - 1) / 8;
    SDL_DispatchGPUCompute(compute_pass, static_cast<Uint32>(groups_x), static_cast<Uint32>(groups_y), 1);
}

} // namespace

void main_render_postprocess_composite(SDL_GPUCommandBuffer* cbuf, const main_render_pass_context_t* context)
{
    OCT_PROFILE_ZONE("render_composite");

    SDL_GPUStorageTextureReadWriteBinding write_textures = {};
    write_textures.texture = context->resources->composite_texture;
    write_textures.cycle = true;
    SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(cbuf, &write_textures, 1, NULL, 0);
    if (!compute_pass)
    {
        SDL_Log("Failed to begin compute pass: %s", SDL_GetError());
        return;
    }

    SDL_GPUTextureSamplerBinding read_samplers[4] = {};
    read_samplers[0].texture = context->resources->color_texture;
    read_samplers[0].sampler = context->resources->nearest_sampler;
    read_samplers[1].texture = context->resources->position_texture;
    read_samplers[1].sampler = context->resources->nearest_sampler;
    read_samplers[2].texture = context->resources->voxel_texture;
    read_samplers[2].sampler = context->resources->nearest_sampler;
    read_samplers[3].texture = context->resources->material_texture;
    read_samplers[3].sampler = context->resources->nearest_sampler;

    struct
    {
        float sky_visibility_and_ambient_strength[4];
    } uniforms = {
        {
            context->visual_sky_visibility,
            context->ambient_strength,
            0.0f,
            0.0f,
        },
    };

    SDL_PushGPUDebugGroup(cbuf, "composite");
    SDL_BindGPUComputePipeline(compute_pass, context->pipelines->composite);
    SDL_BindGPUComputeSamplers(compute_pass, 0, read_samplers, 4);
    SDL_PushGPUComputeUniformData(cbuf, 0, &uniforms, sizeof(uniforms));
    dispatch_viewport_compute(compute_pass, context);
    SDL_EndGPUComputePass(compute_pass);
    SDL_PopGPUDebugGroup(cbuf);
}
