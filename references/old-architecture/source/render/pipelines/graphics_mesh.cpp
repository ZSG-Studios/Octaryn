#include "render/pipelines/graphics_internal.h"

bool main_pipelines_create_opaque(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat color_format,
                                  SDL_GPUTextureFormat depth_format)
{
    SDL_GPUColorTargetDescription color_targets[4] = {};
    color_targets[0].format = color_format;
    color_targets[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    color_targets[2].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    color_targets[3].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.target_info.num_color_targets = 4;
    info.target_info.color_target_descriptions = color_targets;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    return main_pipelines_graphics_create_pipeline(&pipelines->opaque, device, info, "opaque_packed.vert", "opaque.frag");
}

bool main_pipelines_create_opaque_sprite(main_pipelines_t* pipelines, SDL_GPUDevice* device,
                                         SDL_GPUTextureFormat color_format, SDL_GPUTextureFormat depth_format)
{
    SDL_GPUColorTargetDescription color_targets[4] = {};
    color_targets[0].format = color_format;
    color_targets[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    color_targets[2].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    color_targets[3].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.target_info.num_color_targets = 4;
    info.target_info.color_target_descriptions = color_targets;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    return main_pipelines_graphics_create_pipeline(&pipelines->opaque_sprite, device, info, "sprite_packed.vert", "opaque.frag");
}

bool main_pipelines_create_depth(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat depth_format)
{
    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    return main_pipelines_graphics_create_pipeline(&pipelines->depth, device, info, "depth_packed.vert", "depth.frag");
}

bool main_pipelines_create_transparent(main_pipelines_t* pipelines, SDL_GPUDevice* device,
                                       SDL_GPUTextureFormat color_format, SDL_GPUTextureFormat depth_format)
{
    SDL_GPUColorTargetDescription color_targets[1] = {};
    main_pipelines_graphics_init_alpha_blend_target(color_targets[0], color_format);

    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = color_targets;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = false;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    return main_pipelines_graphics_create_pipeline(&pipelines->transparent, device, info, "transparent_packed.vert",
                                                   "transparent.frag");
}

bool main_pipelines_create_water(main_pipelines_t* pipelines, SDL_GPUDevice* device,
                                 SDL_GPUTextureFormat color_format, SDL_GPUTextureFormat depth_format)
{
    SDL_GPUColorTargetDescription color_targets[1] = {};
    main_pipelines_graphics_init_alpha_blend_target(color_targets[0], color_format);

    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = color_targets;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = false;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    return main_pipelines_graphics_create_pipeline(&pipelines->water, device, info, "water.vert", "transparent.frag");
}

bool main_pipelines_create_lava(main_pipelines_t* pipelines, SDL_GPUDevice* device,
                                SDL_GPUTextureFormat color_format, SDL_GPUTextureFormat depth_format)
{
    SDL_GPUColorTargetDescription color_targets[1] = {};
    color_targets[0].format = color_format;

    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = color_targets;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    return main_pipelines_graphics_create_pipeline(&pipelines->lava, device, info, "water.vert", "transparent.frag");
}
