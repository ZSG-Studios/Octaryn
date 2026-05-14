#include "render/scene/passes.h"

#include "app/runtime/frame_profile.h"
#include "render/scene/hidden_blocks.h"
#include "core/profile.h"
#include "world/runtime/world.h"

namespace {

constexpr Uint32 kMaterialFlagPbr = 1u << 0u;
constexpr Uint32 kMaterialFlagPom = 1u << 1u;

void render_sky(SDL_GPUCommandBuffer* cbuf, const main_render_pass_context_t* context)
{
    SDL_GPUColorTargetInfo color_info = {};
    color_info.load_op = SDL_GPU_LOADOP_DONT_CARE;
    color_info.texture = context->resources->color_texture;
    color_info.cycle = true;
    color_info.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cbuf, &color_info, 1, NULL);
    if (!pass)
    {
        SDL_Log("Failed to begin sky render pass: %s", SDL_GetError());
        return;
    }

    struct
    {
        float light_direction_and_sky_visibility[4];
        float twilight_celestial_cloud_time[4];
        float camera_position_and_cloud_height[4];
        float celestial_toggles[4];
    } fragment_uniforms = {};
    SDL_PushGPUDebugGroup(cbuf, "sky");
    SDL_BindGPUGraphicsPipeline(pass, context->pipelines->sky);
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
    fragment_uniforms.twilight_celestial_cloud_time[2] = context->sky_gradient_enabled ? 1.0f : 0.0f;
    fragment_uniforms.twilight_celestial_cloud_time[3] = context->cloud_time_seconds;
    fragment_uniforms.camera_position_and_cloud_height[0] = context->player->camera.position[0];
    fragment_uniforms.camera_position_and_cloud_height[1] = context->player->camera.position[1];
    fragment_uniforms.camera_position_and_cloud_height[2] = context->player->camera.position[2];
    fragment_uniforms.camera_position_and_cloud_height[3] = 192.33f;
    fragment_uniforms.celestial_toggles[0] = context->stars_enabled ? 1.0f : 0.0f;
    fragment_uniforms.celestial_toggles[1] = context->sun_enabled ? 1.0f : 0.0f;
    fragment_uniforms.celestial_toggles[2] = context->moon_enabled ? 1.0f : 0.0f;
    fragment_uniforms.celestial_toggles[3] = 0.0f;
    SDL_PushGPUFragmentUniformData(cbuf, 0, &fragment_uniforms, sizeof(fragment_uniforms));
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_PopGPUDebugGroup(cbuf);
    SDL_EndGPURenderPass(pass);
}

void render_opaque(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, const main_render_pass_context_t* context)
{
    SDL_GPUTextureSamplerBinding atlas_bindings[3] = {};
    struct
    {
        float skylight_floor;
        float cloud_time_seconds;
        float sky_visibility;
        float twilight_strength;
        float celestial_visibility;
        Uint32 material_flags;
        Uint32 pad0;
        Uint32 pad1;
        float camera_position[4];
    } fragment_uniforms = {};
    const Uint32 material_flags = (context->pbr_enabled ? kMaterialFlagPbr : 0u) |
                                  (context->pom_enabled && context->pbr_enabled ? kMaterialFlagPom : 0u);
    fragment_uniforms.skylight_floor = context->skylight_floor;
    fragment_uniforms.cloud_time_seconds = context->cloud_time_seconds;
    fragment_uniforms.sky_visibility = context->visual_sky_visibility;
    fragment_uniforms.twilight_strength = context->twilight_strength;
    fragment_uniforms.celestial_visibility = context->celestial_visibility;
    fragment_uniforms.material_flags = material_flags;
    fragment_uniforms.camera_position[0] = context->player->camera.position[0];
    fragment_uniforms.camera_position[1] = context->player->camera.position[1];
    fragment_uniforms.camera_position[2] = context->player->camera.position[2];
    fragment_uniforms.camera_position[3] = 1.0f;
    atlas_bindings[0].texture = context->resources->atlas_texture;
    atlas_bindings[0].sampler = context->resources->cutout_sampler;
    atlas_bindings[1].texture = context->resources->atlas_normal_texture;
    atlas_bindings[1].sampler = context->resources->cutout_sampler;
    atlas_bindings[2].texture = context->resources->atlas_specular_texture;
    atlas_bindings[2].sampler = context->resources->cutout_sampler;
    main_render_hidden_block_uniforms_t hidden_uniforms = {};
    main_render_fill_hidden_block_uniforms(context, &hidden_uniforms);
    SDL_PushGPUDebugGroup(cbuf, "opaque");
    SDL_BindGPUGraphicsPipeline(pass, context->pipelines->opaque);
    SDL_PushGPUFragmentUniformData(cbuf, 0, &fragment_uniforms, sizeof(fragment_uniforms));
    SDL_PushGPUFragmentUniformData(cbuf, 1, &hidden_uniforms, sizeof(hidden_uniforms));
    SDL_BindGPUFragmentSamplers(pass, 0, atlas_bindings, 3);
    world_render(&context->player->camera, cbuf, pass, WORLD_FLAGS_OPAQUE);
    SDL_PopGPUDebugGroup(cbuf);
}

void render_sprites(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass, const main_render_pass_context_t* context)
{
    SDL_GPUTextureSamplerBinding atlas_bindings[3] = {};
    struct
    {
        float skylight_floor;
        float cloud_time_seconds;
        float sky_visibility;
        float twilight_strength;
        float celestial_visibility;
        Uint32 material_flags;
        Uint32 pad0;
        Uint32 pad1;
        float camera_position[4];
    } fragment_uniforms = {};
    const Uint32 material_flags = (context->pbr_enabled ? kMaterialFlagPbr : 0u) |
                                  (context->pom_enabled && context->pbr_enabled ? kMaterialFlagPom : 0u);
    fragment_uniforms.skylight_floor = context->skylight_floor;
    fragment_uniforms.cloud_time_seconds = context->cloud_time_seconds;
    fragment_uniforms.sky_visibility = context->visual_sky_visibility;
    fragment_uniforms.twilight_strength = context->twilight_strength;
    fragment_uniforms.celestial_visibility = context->celestial_visibility;
    fragment_uniforms.material_flags = material_flags;
    fragment_uniforms.camera_position[0] = context->player->camera.position[0];
    fragment_uniforms.camera_position[1] = context->player->camera.position[1];
    fragment_uniforms.camera_position[2] = context->player->camera.position[2];
    fragment_uniforms.camera_position[3] = 1.0f;
    atlas_bindings[0].texture = context->resources->atlas_texture;
    atlas_bindings[0].sampler = context->resources->cutout_sampler;
    atlas_bindings[1].texture = context->resources->atlas_normal_texture;
    atlas_bindings[1].sampler = context->resources->cutout_sampler;
    atlas_bindings[2].texture = context->resources->atlas_specular_texture;
    atlas_bindings[2].sampler = context->resources->cutout_sampler;
    main_render_hidden_block_uniforms_t hidden_uniforms = {};
    main_render_fill_hidden_block_uniforms(context, &hidden_uniforms);
    SDL_PushGPUDebugGroup(cbuf, "sprites");
    SDL_BindGPUGraphicsPipeline(pass, context->pipelines->opaque_sprite);
    SDL_PushGPUFragmentUniformData(cbuf, 0, &fragment_uniforms, sizeof(fragment_uniforms));
    SDL_PushGPUFragmentUniformData(cbuf, 1, &hidden_uniforms, sizeof(hidden_uniforms));
    SDL_BindGPUFragmentSamplers(pass, 0, atlas_bindings, 3);
    world_render(&context->player->camera, cbuf, pass, WORLD_FLAGS_SPRITE);
    SDL_PopGPUDebugGroup(cbuf);
}

} // namespace

void main_render_pass_gbuffer(SDL_GPUCommandBuffer* cbuf,
                              const main_render_pass_context_t* context,
                              main_frame_profile_sample_t* profile_sample)
{
    OCT_PROFILE_ZONE("render_gbuffer");
    Uint64 stage_start = oct_profile_now_ticks();
    render_sky(cbuf, context);
    if (profile_sample)
    {
        profile_sample->gbuffer_sky_ms = oct_profile_elapsed_ms(stage_start);
    }

    SDL_GPUColorTargetInfo color_info[4] = {};
    color_info[0].load_op = SDL_GPU_LOADOP_LOAD;
    color_info[0].texture = context->resources->color_texture;
    color_info[0].store_op = SDL_GPU_STOREOP_STORE;
    color_info[1].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[1].texture = context->resources->position_texture;
    color_info[1].cycle = true;
    color_info[1].store_op = SDL_GPU_STOREOP_STORE;
    color_info[2].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[2].texture = context->resources->voxel_texture;
    color_info[2].cycle = true;
    color_info[2].store_op = SDL_GPU_STOREOP_STORE;
    color_info[3].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[3].texture = context->resources->material_texture;
    color_info[3].cycle = true;
    color_info[3].store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth_info = {};
    depth_info.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = context->resources->depth_texture;
    depth_info.clear_depth = 1.0f;
    depth_info.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cbuf, color_info, 4, &depth_info);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    stage_start = oct_profile_now_ticks();
    render_opaque(cbuf, pass, context);
    if (profile_sample)
    {
        profile_sample->gbuffer_opaque_ms = oct_profile_elapsed_ms(stage_start);
    }

    stage_start = oct_profile_now_ticks();
    render_sprites(cbuf, pass, context);
    if (profile_sample)
    {
        profile_sample->gbuffer_sprite_ms = oct_profile_elapsed_ms(stage_start);
    }
    SDL_EndGPURenderPass(pass);
}
