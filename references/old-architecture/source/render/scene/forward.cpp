#include "render/scene/forward.h"

#include "render/scene/pass_shared.h"
#include "core/profile.h"
#include "world/runtime/world.h"

namespace {

void render_transparent(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, const main_render_pass_context_t* context)
{
    SDL_GPUTextureSamplerBinding atlas_bindings[3] = {};
    struct
    {
        float skylight_floor;
        float world_time_seconds;
        float sky_visibility;
        float twilight_strength;
        float celestial_visibility;
        Uint32 material_flags;
        Uint32 pad0;
        Uint32 pad1;
        float camera_position[4];
        float light_direction[4];
    } fragment_uniforms = {
        context->skylight_floor,
        context->world_time_seconds,
        context->visual_sky_visibility,
        context->twilight_strength,
        context->celestial_visibility,
        (context->pbr_enabled ? kMaterialFlagPbr : 0u) |
            (context->pom_enabled && context->pbr_enabled ? kMaterialFlagPom : 0u),
        0u,
        0u,
        {
            context->player->camera.position[0],
            context->player->camera.position[1],
            context->player->camera.position[2],
            1.0f,
        },
        {
            context->visual_light_direction[0],
            context->visual_light_direction[1],
            context->visual_light_direction[2],
            0.0f,
        },
    };
    atlas_bindings[0].texture = context->resources->atlas_texture;
    atlas_bindings[0].sampler = context->resources->atlas_sampler;
    atlas_bindings[1].texture = context->resources->atlas_normal_texture;
    atlas_bindings[1].sampler = context->resources->atlas_sampler;
    atlas_bindings[2].texture = context->resources->atlas_specular_texture;
    atlas_bindings[2].sampler = context->resources->atlas_sampler;

    SDL_PushGPUDebugGroup(cbuf, "transparent");
    SDL_BindGPUGraphicsPipeline(pass, context->pipelines->transparent);
    SDL_PushGPUFragmentUniformData(cbuf, 0, &fragment_uniforms, sizeof(fragment_uniforms));
    SDL_BindGPUFragmentSamplers(pass, 0, atlas_bindings, 3);
    world_render(&context->player->camera, cbuf, pass, WORLD_FLAGS_TRANSPARENT);
    SDL_PopGPUDebugGroup(cbuf);
}

void render_water(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, const main_render_pass_context_t* context)
{
    SDL_GPUTextureSamplerBinding atlas_bindings[3] = {};
    struct
    {
        float skylight_floor;
        float world_time_seconds;
        float sky_visibility;
        float twilight_strength;
        float celestial_visibility;
        Uint32 material_flags;
        Uint32 pad0;
        Uint32 pad1;
        float camera_position[4];
        float light_direction[4];
    } fragment_uniforms = {
        context->skylight_floor,
        context->world_time_seconds,
        context->visual_sky_visibility,
        context->twilight_strength,
        context->celestial_visibility,
        (context->pbr_enabled ? kMaterialFlagPbr : 0u) |
            (context->pom_enabled && context->pbr_enabled ? kMaterialFlagPom : 0u),
        0u,
        0u,
        {
            context->player->camera.position[0],
            context->player->camera.position[1],
            context->player->camera.position[2],
            1.0f,
        },
        {
            context->visual_light_direction[0],
            context->visual_light_direction[1],
            context->visual_light_direction[2],
            0.0f,
        },
    };
    atlas_bindings[0].texture = context->resources->atlas_texture;
    atlas_bindings[0].sampler = context->resources->atlas_sampler;
    atlas_bindings[1].texture = context->resources->atlas_normal_texture;
    atlas_bindings[1].sampler = context->resources->atlas_sampler;
    atlas_bindings[2].texture = context->resources->atlas_specular_texture;
    atlas_bindings[2].sampler = context->resources->atlas_sampler;

    SDL_PushGPUDebugGroup(cbuf, "water");
    SDL_BindGPUGraphicsPipeline(pass, context->pipelines->water);
    SDL_PushGPUFragmentUniformData(cbuf, 0, &fragment_uniforms, sizeof(fragment_uniforms));
    SDL_BindGPUFragmentSamplers(pass, 0, atlas_bindings, 3);
    world_render(&context->player->camera, cbuf, pass, WORLD_FLAGS_WATER);
    SDL_PopGPUDebugGroup(cbuf);
}

void render_lava(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, const main_render_pass_context_t* context)
{
    SDL_GPUTextureSamplerBinding atlas_bindings[3] = {};
    struct
    {
        float skylight_floor;
        float world_time_seconds;
        float sky_visibility;
        float twilight_strength;
        float celestial_visibility;
        Uint32 material_flags;
        Uint32 pad0;
        Uint32 pad1;
        float camera_position[4];
        float light_direction[4];
    } fragment_uniforms = {
        context->skylight_floor,
        context->world_time_seconds,
        context->visual_sky_visibility,
        context->twilight_strength,
        context->celestial_visibility,
        (context->pbr_enabled ? kMaterialFlagPbr : 0u) |
            (context->pom_enabled && context->pbr_enabled ? kMaterialFlagPom : 0u),
        0u,
        0u,
        {
            context->player->camera.position[0],
            context->player->camera.position[1],
            context->player->camera.position[2],
            1.0f,
        },
        {
            context->visual_light_direction[0],
            context->visual_light_direction[1],
            context->visual_light_direction[2],
            0.0f,
        },
    };
    atlas_bindings[0].texture = context->resources->atlas_texture;
    atlas_bindings[0].sampler = context->resources->atlas_sampler;
    atlas_bindings[1].texture = context->resources->atlas_normal_texture;
    atlas_bindings[1].sampler = context->resources->atlas_sampler;
    atlas_bindings[2].texture = context->resources->atlas_specular_texture;
    atlas_bindings[2].sampler = context->resources->atlas_sampler;

    SDL_PushGPUDebugGroup(cbuf, "lava");
    SDL_BindGPUGraphicsPipeline(pass, context->pipelines->lava);
    SDL_PushGPUFragmentUniformData(cbuf, 0, &fragment_uniforms, sizeof(fragment_uniforms));
    SDL_BindGPUFragmentSamplers(pass, 0, atlas_bindings, 3);
    world_render(&context->player->camera, cbuf, pass, WORLD_FLAGS_LAVA);
    SDL_PopGPUDebugGroup(cbuf);
}

void render_clouds(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, const main_render_pass_context_t* context)
{
    struct
    {
        float light_direction_and_sky_visibility[4];
        float twilight_celestial_cloud_time[4];
        float camera_position_and_cloud_height[4];
    } fragment_uniforms = {};
    SDL_PushGPUDebugGroup(cbuf, "clouds");
    SDL_BindGPUGraphicsPipeline(pass, context->pipelines->clouds);
    SDL_PushGPUVertexUniformData(cbuf, 0, context->player->camera.proj, 64);
    SDL_PushGPUVertexUniformData(cbuf, 1, context->player->camera.view, 64);
    SDL_PushGPUFragmentUniformData(cbuf, 1, context->player->camera.proj, 64);
    SDL_PushGPUFragmentUniformData(cbuf, 2, context->player->camera.view, 64);
    fragment_uniforms.light_direction_and_sky_visibility[0] = context->visual_light_direction[0];
    fragment_uniforms.light_direction_and_sky_visibility[1] = context->visual_light_direction[1];
    fragment_uniforms.light_direction_and_sky_visibility[2] = context->visual_light_direction[2];
    fragment_uniforms.light_direction_and_sky_visibility[3] = context->visual_sky_visibility;
    fragment_uniforms.twilight_celestial_cloud_time[0] = context->twilight_strength;
    fragment_uniforms.twilight_celestial_cloud_time[1] = context->celestial_visibility;
    fragment_uniforms.twilight_celestial_cloud_time[2] = context->cloud_time_seconds;
    fragment_uniforms.twilight_celestial_cloud_time[3] = context->cloud_max_distance;
    fragment_uniforms.camera_position_and_cloud_height[0] = context->player->camera.position[0];
    fragment_uniforms.camera_position_and_cloud_height[1] = context->player->camera.position[1];
    fragment_uniforms.camera_position_and_cloud_height[2] = context->player->camera.position[2];
    fragment_uniforms.camera_position_and_cloud_height[3] = 192.33f;
    SDL_PushGPUFragmentUniformData(cbuf, 0, &fragment_uniforms, sizeof(fragment_uniforms));
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_PopGPUDebugGroup(cbuf);
}

} // namespace

void main_render_pass_forward(SDL_GPUCommandBuffer* cbuf, const main_render_pass_context_t* context)
{
    OCT_PROFILE_ZONE("render_forward");
    SDL_GPUColorTargetInfo color_info = {};
    color_info.load_op = SDL_GPU_LOADOP_LOAD;
    color_info.texture = context->resources->composite_texture;
    color_info.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth_info = {};
    depth_info.load_op = SDL_GPU_LOADOP_LOAD;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = context->resources->depth_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cbuf, &color_info, 1, &depth_info);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }

    if (context->clouds_enabled)
    {
        render_clouds(cbuf, pass, context);
    }
    render_transparent(cbuf, pass, context);
    render_lava(cbuf, pass, context);
    render_water(cbuf, pass, context);
    SDL_EndGPURenderPass(pass);
}
