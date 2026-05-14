#include "app/runtime/render/probe.h"

#include <exception>
#include <filesystem>

#include "app/runtime/internal.h"

namespace {

enum class app_render_probe_kind_t
{
    VisibleFrame,
    Color,
    Composite,
};

struct app_render_probe_request_t
{
    app_render_probe_kind_t kind;
    const char* label;
    SDL_GPUTexture* texture;
    SDL_GPUTextureFormat texture_format;
    const char* output_path;
    SDL_GPUTransferBuffer* transfer_buffer;
    Uint32 row_pitch;
    Uint32 width;
    Uint32 height;
};

bool app_render_probe_validate_visible_frame(const app_render_probe_request_t& probe, Uint32 width, Uint32 height)
{
    void* pixels = SDL_MapGPUTransferBuffer(device, probe.transfer_buffer, false);
    if (!pixels)
    {
        SDL_Log("Failed to map %s probe transfer buffer: %s", probe.label, SDL_GetError());
        return false;
    }

    const Uint8* bytes = static_cast<const Uint8*>(pixels);
    const Uint32 center_x = width / 2;
    const Uint32 center_y = height / 2;
    const Uint8* center = bytes + (center_y * probe.row_pitch) + (center_x * 4u);
    const bool nonzero_center = center[0] != 0 || center[1] != 0 || center[2] != 0 || center[3] != 0;
    SDL_UnmapGPUTransferBuffer(device, probe.transfer_buffer);
    if (!nonzero_center)
    {
        SDL_Log("Visible-frame probe validation failed: center pixel remained zero");
    }
    return nonzero_center;
}

bool app_render_probe_save_rgba8_download(const app_render_probe_request_t& probe, Uint32 width, Uint32 height)
{
    void* pixels = SDL_MapGPUTransferBuffer(device, probe.transfer_buffer, false);
    if (!pixels)
    {
        SDL_Log("Failed to map %s probe transfer buffer: %s", probe.label, SDL_GetError());
        return false;
    }

    SDL_Surface* surface = SDL_CreateSurfaceFrom(static_cast<int>(width),
                                                 static_cast<int>(height),
                                                 SDL_PIXELFORMAT_RGBA32,
                                                 pixels,
                                                 static_cast<int>(probe.row_pitch));
    if (!surface)
    {
        SDL_Log("Failed to create %s probe surface: %s", probe.label, SDL_GetError());
        SDL_UnmapGPUTransferBuffer(device, probe.transfer_buffer);
        return false;
    }

    bool saved = false;
    try
    {
        std::filesystem::path output(probe.output_path);
        if (output.has_parent_path())
        {
            std::filesystem::create_directories(output.parent_path());
        }
        saved = SDL_SaveBMP(surface, probe.output_path);
        if (!saved)
        {
            SDL_Log("Failed to save %s probe '%s': %s", probe.label, probe.output_path, SDL_GetError());
        }
    }
    catch (const std::exception& e)
    {
        SDL_Log("Failed to prepare %s probe output path '%s': %s", probe.label, probe.output_path, e.what());
    }

    SDL_DestroySurface(surface);
    SDL_UnmapGPUTransferBuffer(device, probe.transfer_buffer);
    return saved;
}

bool app_render_probe_process_download(const app_render_probe_request_t& probe, Uint32 width, Uint32 height)
{
    if (probe.kind == app_render_probe_kind_t::VisibleFrame)
    {
        return app_render_probe_validate_visible_frame(probe, width, height);
    }
    return app_render_probe_save_rgba8_download(probe, width, height);
}

void app_render_probe_note_failure(app_runtime_probe_options_t* options, const app_render_probe_request_t& probe)
{
    switch (probe.kind)
    {
    case app_render_probe_kind_t::VisibleFrame:
        options->visible_frame_failed = true;
        return;
    case app_render_probe_kind_t::Color:
        options->color_failed = true;
        return;
    case app_render_probe_kind_t::Composite:
        options->composite_failed = true;
        return;
    }
}

void app_render_probe_note_success(app_runtime_probe_options_t* options, const app_render_probe_request_t& probe)
{
    switch (probe.kind)
    {
    case app_render_probe_kind_t::VisibleFrame:
        options->visible_frame_verified = true;
        options->visible_frame_failed = false;
        return;
    case app_render_probe_kind_t::Color:
        options->color_written = true;
        options->color_failed = false;
        return;
    case app_render_probe_kind_t::Composite:
        options->composite_written = true;
        options->composite_failed = false;
        return;
    }
}

bool app_render_probe_enqueue_download(SDL_GPUCopyPass* copy_pass,
                                       app_render_probe_request_t* probe,
                                       Uint32 width,
                                       Uint32 height)
{
    probe->row_pitch = width * SDL_GPUTextureFormatTexelBlockSize(probe->texture_format);
    const Uint32 transfer_size = probe->row_pitch * height;

    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transfer_info.size = transfer_size;
    probe->transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (!probe->transfer_buffer)
    {
        SDL_Log("Failed to create %s probe transfer buffer: %s", probe->label, SDL_GetError());
        return false;
    }

    SDL_GPUTextureRegion source = {};
    source.texture = probe->texture;
    source.w = width;
    source.h = height;
    source.d = 1;

    SDL_GPUTextureTransferInfo destination = {};
    destination.transfer_buffer = probe->transfer_buffer;
    destination.pixels_per_row = width;
    destination.rows_per_layer = height;
    SDL_DownloadFromGPUTexture(copy_pass, &source, &destination);
    return true;
}

void app_render_probe_release(app_render_probe_request_t* probes, int probe_count)
{
    for (int i = 0; i < probe_count; i++)
    {
        if (probes[i].transfer_buffer)
        {
            SDL_ReleaseGPUTransferBuffer(device, probes[i].transfer_buffer);
            probes[i].transfer_buffer = nullptr;
        }
    }
}

} // namespace

bool app_render_probe_capture_requested(SDL_GPUCommandBuffer* cbuf, int width, int height, app_runtime_probe_options_t* options)
{
    app_render_probe_request_t probes[3] = {};
    int probe_count = 0;

    if (options->require_visible_frame && !options->visible_frame_verified)
    {
        probes[probe_count++] = {
            .kind = app_render_probe_kind_t::VisibleFrame,
            .label = "visible-frame",
            .texture = resources.composite_texture,
            .texture_format = color_format,
            .output_path = nullptr,
            .transfer_buffer = nullptr,
            .row_pitch = 0,
            .width = static_cast<Uint32>(width),
            .height = static_cast<Uint32>(height),
        };
    }

    const char* color_output_path = options->color_path.c_str();
    if (!options->color_path.empty())
    {
        probes[probe_count++] = {
            .kind = app_render_probe_kind_t::Color,
            .label = "color",
            .texture = resources.color_texture,
            .texture_format = color_format,
            .output_path = color_output_path,
            .transfer_buffer = nullptr,
            .row_pitch = 0,
            .width = resources.render_width,
            .height = resources.render_height,
        };
    }

    const char* composite_output_path = options->composite_path.c_str();
    if (!options->composite_path.empty())
    {
        probes[probe_count++] = {
            .kind = app_render_probe_kind_t::Composite,
            .label = "composite",
            .texture = resources.composite_texture,
            .texture_format = color_format,
            .output_path = composite_output_path,
            .transfer_buffer = nullptr,
            .row_pitch = 0,
            .width = resources.render_width,
            .height = resources.render_height,
        };
    }

    if (probe_count == 0)
    {
        return true;
    }

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cbuf);
    if (!copy_pass)
    {
        SDL_Log("Failed to begin render probe copy pass: %s", SDL_GetError());
        for (int i = 0; i < probe_count; i++)
        {
            app_render_probe_note_failure(options, probes[i]);
        }
        return false;
    }

    for (int i = 0; i < probe_count; i++)
    {
        if (!app_render_probe_enqueue_download(copy_pass, &probes[i], probes[i].width, probes[i].height))
        {
            SDL_EndGPUCopyPass(copy_pass);
            app_render_probe_release(probes, probe_count);
            for (int probe_index = 0; probe_index < probe_count; probe_index++)
            {
                app_render_probe_note_failure(options, probes[probe_index]);
            }
            return false;
        }
    }
    SDL_EndGPUCopyPass(copy_pass);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cbuf);
    if (!fence)
    {
        SDL_Log("Failed to submit render probe command buffer: %s", SDL_GetError());
        app_render_probe_release(probes, probe_count);
        for (int i = 0; i < probe_count; i++)
        {
            app_render_probe_note_failure(options, probes[i]);
        }
        return false;
    }

    SDL_GPUFence* fences[] = {fence};
    const bool waited = SDL_WaitForGPUFences(device, true, fences, 1);
    SDL_ReleaseGPUFence(device, fence);
    if (!waited)
    {
        SDL_Log("Failed waiting for render probe fence: %s", SDL_GetError());
        app_render_probe_release(probes, probe_count);
        for (int i = 0; i < probe_count; i++)
        {
            app_render_probe_note_failure(options, probes[i]);
        }
        return false;
    }

    bool all_saved = true;
    for (int i = 0; i < probe_count; i++)
    {
        const bool saved = app_render_probe_process_download(probes[i], probes[i].width, probes[i].height);
        if (!saved)
        {
            app_render_probe_note_failure(options, probes[i]);
            all_saved = false;
            continue;
        }
        if (probes[i].output_path && probes[i].output_path[0] != '\0')
        {
            SDL_Log("%s probe dump written to %s", probes[i].label, probes[i].output_path);
        }
        app_render_probe_note_success(options, probes[i]);
    }

    app_render_probe_release(probes, probe_count);
    return all_saved;
}
