#include "render/atlas/upload/upload.h"

#include "render/atlas/upload/data.h"
#include "render/atlas/upload/transfer.h"
#include "core/profile.h"

auto main_render_atlas_upload_texture(const SDL_Surface* surface,
                                      SDL_GPUTexture* texture,
                                      SDL_GPUDevice* device,
                                      Uint32 layer_count,
                                      Uint32 mip_levels,
                                      main_render_atlas_mip_kind_t kind,
                                      const char* label) -> bool
{
    const Uint64 build_start = oct_profile_now_ticks();

    SDL_GPUTransferBufferCreateInfo buffer_info = {};
    buffer_info.size = main_render_atlas_upload_size(layer_count, mip_levels);
    buffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &buffer_info);
    if (!buffer)
    {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return false;
    }

    if (!main_render_atlas_build_upload_data(surface, device, buffer, layer_count, mip_levels, kind))
    {
        SDL_ReleaseGPUTransferBuffer(device, buffer);
        return false;
    }
    char build_label[96] = {0};
    SDL_snprintf(build_label, sizeof(build_label), "create_atlas | build %s mip upload data", label);
    oct_profile_log_duration("Startup timing", build_label, build_start);

    const Uint64 upload_start = oct_profile_now_ticks();
    const bool upload_ok =
        main_render_atlas_transfer_to_gpu(device, texture, buffer, layer_count, mip_levels);
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    if (!upload_ok)
    {
        return false;
    }

    char upload_label[96] = {0};
    SDL_snprintf(upload_label, sizeof(upload_label), "create_atlas | %s gpu upload", label);
    oct_profile_log_duration("Startup timing", upload_label, upload_start);
    return true;
}

auto main_render_atlas_upload(main_render_resources_t* resources,
                              SDL_GPUDevice* device,
                              Uint32 layer_count,
                              Uint32 mip_levels) -> bool
{
    return main_render_atlas_upload_texture(resources->atlas_surface,
                                           resources->atlas_texture,
                                           device,
                                           layer_count,
                                           mip_levels,
                                           MAIN_RENDER_ATLAS_MIP_ALBEDO,
                                           "albedo");
}
