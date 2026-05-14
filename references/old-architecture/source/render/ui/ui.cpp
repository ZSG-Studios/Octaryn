#include "render/ui/ui.h"

namespace {

struct main_ui_dispatch_rect_t
{
    Sint32 x;
    Sint32 y;
    Sint32 w;
    Sint32 h;
};

struct main_ui_viewport_uniform_t
{
    Sint32 viewport[2];
    Sint32 offset[2];
};

Sint32 clamp_i32(Sint32 value, Sint32 minimum, Sint32 maximum)
{
    return SDL_min(SDL_max(value, minimum), maximum);
}

void dispatch_rect(SDL_GPUCommandBuffer* cbuf,
                   SDL_GPUComputePass* compute_pass,
                   const main_ui_context_t* context,
                   main_ui_dispatch_rect_t rect)
{
    const Sint32 viewport_width = context->viewport_size[0];
    const Sint32 viewport_height = context->viewport_size[1];
    const Sint32 x0 = clamp_i32(rect.x, 0, viewport_width);
    const Sint32 y0 = clamp_i32(rect.y, 0, viewport_height);
    const Sint32 x1 = clamp_i32(rect.x + rect.w, 0, viewport_width);
    const Sint32 y1 = clamp_i32(rect.y + rect.h, 0, viewport_height);
    if (x1 <= x0 || y1 <= y0)
    {
        return;
    }

    const main_ui_viewport_uniform_t viewport_uniform = {
        .viewport = {viewport_width, viewport_height},
        .offset = {x0, y0},
    };
    const Uint32 groups_x = (Uint32) ((x1 - x0 + 8 - 1) / 8);
    const Uint32 groups_y = (Uint32) ((y1 - y0 + 8 - 1) / 8);
    SDL_PushGPUComputeUniformData(cbuf, 0, &viewport_uniform, sizeof(viewport_uniform));
    SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1);
}

void dispatch_active_regions(SDL_GPUCommandBuffer* cbuf, SDL_GPUComputePass* compute_pass, const main_ui_context_t* context)
{
    const Sint32 viewport_width = context->viewport_size[0];
    const Sint32 viewport_height = context->viewport_size[1];
    if (viewport_width <= 0 || viewport_height <= 0)
    {
        return;
    }
    if (context->data.menu_enabled != 0u)
    {
        dispatch_rect(cbuf, compute_pass, context, {0, 0, viewport_width, viewport_height});
        return;
    }

    const float base_scale = SDL_max((float) viewport_width / 1280.0f, (float) viewport_height / 720.0f);
    const float scale = base_scale * 2.0f;
    const Sint32 block_start = (Sint32) SDL_floorf(10.0f * scale) - 2;
    const Sint32 block_end = (Sint32) SDL_ceilf(60.0f * scale) + 2;
    dispatch_rect(cbuf,
                  compute_pass,
                  context,
                  {block_start, viewport_height - block_end, block_end - block_start, block_end - block_start});

    const Sint32 cross_width = (Sint32) SDL_ceilf(8.0f * base_scale) + 2;
    const Sint32 cross_thickness = (Sint32) SDL_ceilf(2.0f * base_scale) + 2;
    const Sint32 center_x = viewport_width / 2;
    const Sint32 center_y = viewport_height / 2;
    dispatch_rect(cbuf,
                  compute_pass,
                  context,
                  {center_x - cross_width, center_y - cross_thickness, cross_width * 2, cross_thickness * 2});
    dispatch_rect(cbuf,
                  compute_pass,
                  context,
                  {center_x - cross_thickness, center_y - cross_width, cross_thickness * 2, cross_width * 2});

    if (context->data.show_debug == 0u)
    {
        return;
    }

    const Uint32 font_scale = SDL_max(1u, (Uint32) (scale + 0.5f));
    const Sint32 padding = 4 * (Sint32) font_scale;
    const Sint32 margin = 6 * (Sint32) font_scale;
    const Sint32 content_width = 30 * 4 * (Sint32) font_scale - (Sint32) font_scale;
    const Sint32 content_height = 16 * 6 * (Sint32) font_scale - (Sint32) font_scale;
    const Sint32 panel_width = content_width + padding * 2;
    const Sint32 panel_height = content_height + padding * 2;
    dispatch_rect(cbuf,
                  compute_pass,
                  context,
                  {margin, margin, panel_width, panel_height});
}

} // namespace

void main_ui_render(SDL_GPUCommandBuffer* cbuf, const main_ui_context_t* context)
{
    SDL_GPUStorageTextureReadWriteBinding write_textures[1] = {};
    write_textures[0].texture = context->composite_texture;
    SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(cbuf, write_textures, 1, NULL, 0);
    if (!compute_pass)
    {
        SDL_Log("Failed to begin compute pass: %s", SDL_GetError());
        return;
    }

    SDL_GPUTextureSamplerBinding read_textures[1] = {};
    read_textures[0].texture = context->atlas_texture;
    read_textures[0].sampler = context->nearest_sampler;

    SDL_PushGPUDebugGroup(cbuf, "ui");
    SDL_BindGPUComputePipeline(compute_pass, context->pipeline);
    SDL_BindGPUComputeSamplers(compute_pass, 0, read_textures, 1);
    SDL_PushGPUComputeUniformData(cbuf, 1, &context->data, sizeof(context->data));
    dispatch_active_regions(cbuf, compute_pass, context);
    SDL_EndGPUComputePass(compute_pass);
    SDL_PopGPUDebugGroup(cbuf);
}
