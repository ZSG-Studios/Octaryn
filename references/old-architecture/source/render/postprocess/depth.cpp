#include "render/postprocess/composite.h"

#include "core/profile.h"
#include "world/runtime/world.h"

void main_render_postprocess_depth(SDL_GPUCommandBuffer* cbuf, const main_render_pass_context_t* context)
{
    OCT_PROFILE_ZONE("render_depth");
    if (!cbuf || !context || !context->resources || !context->pipelines || !context->resources->depth_texture)
    {
        return;
    }

    SDL_GPUDepthStencilTargetInfo depth_info = {};
    depth_info.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = context->resources->depth_texture;
    depth_info.clear_depth = 1.0f;
    depth_info.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cbuf, nullptr, 0, &depth_info);
    if (!pass)
    {
        SDL_Log("Failed to begin depth prepass: %s", SDL_GetError());
        return;
    }

    SDL_PushGPUDebugGroup(cbuf, "depth_prepass");
    SDL_BindGPUGraphicsPipeline(pass, context->pipelines->depth);
    world_render(&context->player->camera, cbuf, pass, WORLD_FLAGS_OPAQUE);
    SDL_PopGPUDebugGroup(cbuf);
    SDL_EndGPURenderPass(pass);
}
