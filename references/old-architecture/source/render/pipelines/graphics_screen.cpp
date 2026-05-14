#include "render/pipelines/graphics_internal.h"

bool main_pipelines_create_sky(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat color_format,
                               SDL_GPUTextureFormat depth_format)
{
    (void) depth_format;
    SDL_GPUColorTargetDescription color_targets[1] = {};
    color_targets[0].format = color_format;

    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = color_targets;
    return main_pipelines_graphics_create_pipeline(&pipelines->sky, device, info, "sky.vert", "sky.frag");
}

bool main_pipelines_create_clouds(main_pipelines_t* pipelines, SDL_GPUDevice* device, SDL_GPUTextureFormat color_format,
                                  SDL_GPUTextureFormat depth_format)
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
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    return main_pipelines_graphics_create_pipeline(&pipelines->clouds, device, info, "sky.vert", "clouds.frag");
}

bool main_pipelines_create_selection(main_pipelines_t* pipelines, SDL_GPUDevice* device,
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
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
    return main_pipelines_graphics_create_pipeline(&pipelines->selection, device, info, "selection.vert",
                                                   "selection.frag");
}

bool main_pipelines_create_present(main_pipelines_t* pipelines, SDL_GPUDevice* device,
                                   SDL_GPUTextureFormat swapchain_format)
{
    SDL_GPUColorTargetDescription color_targets[1] = {};
    color_targets[0].format = swapchain_format;

    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = color_targets;
    return main_pipelines_graphics_create_pipeline(&pipelines->present, device, info, "present.vert", "present.frag");
}
