#include "app/runtime/render/present.h"

#include "app/runtime/diagnostics.h"
#include "app/runtime/internal.h"
#include "core/log.h"

void app_present_startup_frame(void)
{
    Uint64 step_start = app_profile_now();
    SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(device);
    if (!cbuf)
    {
        return;
    }
    oct_log_infof("Startup timing | startup present acquire command buffer took %.2f ms",
                  app_profile_elapsed_ms(step_start));
    SDL_GPUTexture* swapchain_texture = NULL;
    Uint32 width = 0;
    Uint32 height = 0;
    step_start = app_profile_now();
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cbuf, window, &swapchain_texture, &width, &height) ||
        !swapchain_texture || !width || !height)
    {
        SDL_Log("Startup present skipped: swapchain unavailable (%s)", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cbuf);
        return;
    }
    oct_log_infof("Startup timing | startup present acquire swapchain took %.2f ms",
                  app_profile_elapsed_ms(step_start));
    step_start = app_profile_now();
    SDL_GPUColorTargetInfo color_info = {};
    color_info.texture = swapchain_texture;
    color_info.load_op = SDL_GPU_LOADOP_CLEAR;
    color_info.store_op = SDL_GPU_STOREOP_STORE;
    color_info.clear_color.r = 0.02f;
    color_info.clear_color.g = 0.02f;
    color_info.clear_color.b = 0.025f;
    color_info.clear_color.a = 1.0f;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cbuf, &color_info, 1, NULL);
    if (pass)
    {
        SDL_EndGPURenderPass(pass);
    }
    oct_log_infof("Startup timing | startup present record pass took %.2f ms",
                  app_profile_elapsed_ms(step_start));
    step_start = app_profile_now();
    SDL_SubmitGPUCommandBuffer(cbuf);
    oct_log_infof("Startup timing | startup present submit took %.2f ms",
                  app_profile_elapsed_ms(step_start));
    SDL_Log("Startup present submitted (%ux%u)", width, height);
}

void app_render_swapchain(SDL_GPUCommandBuffer* cbuf,
                          SDL_GPUTexture* swapchain_texture,
                          SDL_GPUTexture* overlay_texture,
                          bool overlay_enabled)
{
    const Uint32 overlay_uniform = overlay_enabled ? 1u : 0u;
    SDL_GPUColorTargetInfo color_target = {};
    color_target.texture = swapchain_texture;
    color_target.load_op = SDL_GPU_LOADOP_DONT_CARE;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cbuf, &color_target, 1, NULL);
    if (!pass)
    {
        return;
    }
    SDL_GPUTextureSamplerBinding bindings[2] = {};
    bindings[0].texture = resources.composite_texture;
    bindings[0].sampler = resources.nearest_sampler;
    bindings[1].texture = overlay_texture;
    bindings[1].sampler = resources.nearest_sampler;
    SDL_BindGPUGraphicsPipeline(pass, pipelines.present);
    SDL_BindGPUFragmentSamplers(pass, 0, bindings, 2);
    SDL_PushGPUFragmentUniformData(cbuf, 0, &overlay_uniform, sizeof(overlay_uniform));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}
