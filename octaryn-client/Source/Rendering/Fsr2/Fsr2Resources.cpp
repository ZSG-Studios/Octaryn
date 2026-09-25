#include "Fsr2Internal.h"
#include <algorithm>
#include <stdexcept>

namespace octaryn::client::rendering {
namespace {
rhi::Format format(FfxSurfaceFormat value) {
    using rhi::Format;
    switch (value) {
    case FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS:
    case FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT: return Format::RGBA32Float;
    case FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT: return Format::RGBA16Float;
    case FFX_SURFACE_FORMAT_R16G16B16A16_UNORM: return Format::RGBA16Unorm;
    case FFX_SURFACE_FORMAT_R32G32_FLOAT: return Format::RG32Float;
    case FFX_SURFACE_FORMAT_R32_UINT: return Format::R32Uint;
    case FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS:
    case FFX_SURFACE_FORMAT_R8G8B8A8_UNORM: return Format::RGBA8Unorm;
    case FFX_SURFACE_FORMAT_R11G11B10_FLOAT: return Format::R11G11B10Float;
    case FFX_SURFACE_FORMAT_R16G16_FLOAT: return Format::RG16Float;
    case FFX_SURFACE_FORMAT_R16G16_UINT: return Format::RG16Uint;
    case FFX_SURFACE_FORMAT_R16_FLOAT: return Format::R16Float;
    case FFX_SURFACE_FORMAT_R16_UINT: return Format::R16Uint;
    case FFX_SURFACE_FORMAT_R16_UNORM: return Format::R16Unorm;
    case FFX_SURFACE_FORMAT_R16_SNORM: return Format::R16Snorm;
    case FFX_SURFACE_FORMAT_R8_UNORM: return Format::R8Unorm;
    case FFX_SURFACE_FORMAT_R8_UINT: return Format::R8Uint;
    case FFX_SURFACE_FORMAT_R8G8_UNORM: return Format::RG8Unorm;
    case FFX_SURFACE_FORMAT_R32_FLOAT: return Format::R32Float;
    default: throw std::runtime_error("Unsupported FSR2 resource format");
    }
}
FfxSurfaceFormat format(rhi::Format value) {
    if (value == rhi::Format::D32Float) return FFX_SURFACE_FORMAT_R32_FLOAT;
    for (int id = FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS; id <= FFX_SURFACE_FORMAT_R32_FLOAT; ++id)
        if (format(static_cast<FfxSurfaceFormat>(id)) == value) return static_cast<FfxSurfaceFormat>(id);
    return FFX_SURFACE_FORMAT_UNKNOWN;
}
void views(Fsr2Resource& resource) {
    resource.srv = resource.texture->createView({});
    if (!resource.srv) throw std::runtime_error("FSR2 SRV creation failed");
    const auto& desc = resource.texture->getDesc();
    if (!rhi::is_set(desc.usage, rhi::TextureUsage::UnorderedAccess)) return;
    for (uint32_t mip = 0; mip < desc.mipCount; ++mip) {
        rhi::TextureViewDesc view{};
        view.subresourceRange = {mip, 1, 0, 1};
        resource.mips.push_back(resource.texture->createView(view));
        if (!resource.mips.back()) throw std::runtime_error("FSR2 UAV mip creation failed");
    }
}
int free_index(Fsr2Context& context) {
    for (int i = 1; i < static_cast<int>(context.resources.size()); ++i)
        if (!context.resources[i].texture) return i;
    throw std::runtime_error("FSR2 resource capacity exceeded");
}
}
FfxResource fsr_external(rhi::ITexture* texture) {
    FfxResource result{};
    if (!texture) return result;
    const auto& desc = texture->getDesc();
    result.resource = texture;
    result.description = {FFX_RESOURCE_TYPE_TEXTURE2D, format(desc.format),
        desc.size.width, desc.size.height, desc.size.depth, desc.mipCount, FFX_RESOURCE_FLAGS_NONE};
    result.state = FFX_RESOURCE_STATE_COMPUTE_READ;
    result.isDepth = desc.format == rhi::Format::D32Float;
    return result;
}
FfxErrorCode fsr_create_resource(FfxFsr2Interface* api, const FfxCreateResourceDescription* input, FfxResourceInternal* out) {
    auto& context = fsr_backend(api);
    try {
        const auto& d = input->resourceDescription;
        if (d.type != FFX_RESOURCE_TYPE_TEXTURE2D && d.type != FFX_RESOURCE_TYPE_TEXTURE1D)
            throw std::runtime_error("FSR2 requested unsupported resource dimension");
        const int index = free_index(context);
        auto& resource = context.resources[index];
        rhi::TextureDesc desc{};
        // SDK flags deliberately select 2D LUTs; no backend-specific 1D path.
        desc.size = {d.width, std::max(1u, d.height), 1};
        desc.mipCount = d.mipCount ? d.mipCount : rhi::kAllMips;
        desc.format = format(d.format);
        desc.usage = rhi::TextureUsage::ShaderResource |
            rhi::TextureUsage::CopySource | rhi::TextureUsage::CopyDestination;
        if (input->usage & FFX_RESOURCE_USAGE_UAV) desc.usage |= rhi::TextureUsage::UnorderedAccess;
        desc.defaultState = rhi::ResourceState::ShaderResource;
        rhi::SubresourceData data{};
        if (input->initData) {
            if (d.mipCount != 1) throw std::runtime_error("FSR2 initialized texture must have one mip");
            data.data = input->initData;
            data.rowPitch = uint64_t(d.width) * rhi::getFormatInfo(desc.format).blockSizeInBytes;
            data.slicePitch = data.rowPitch * desc.size.height;
            if (data.slicePitch > input->initDataSize) throw std::runtime_error("FSR2 initialization data truncated");
        }
        resource.texture = context.device->createTexture(desc, input->initData ? &data : nullptr);
        if (!resource.texture) throw std::runtime_error("FSR2 texture allocation failed");
        resource.description = d;
        resource.description.mipCount = resource.texture->getDesc().mipCount;
        views(resource);
        uint32_t width = desc.size.width, height = desc.size.height;
        for (uint32_t mip = 0; mip < resource.description.mipCount; ++mip) {
            resource.bytes += uint64_t(width) * height * rhi::getFormatInfo(desc.format).blockSizeInBytes;
            width = std::max(1u, width / 2); height = std::max(1u, height / 2);
        }
        out->internalIndex = index;
        return FFX_OK;
    } catch (const std::exception& error) {
        context.error = error.what();
        return FFX_ERROR_BACKEND_API_ERROR;
    }
}
FfxErrorCode fsr_register_resource(FfxFsr2Interface* api, const FfxResource* input, FfxResourceInternal* out) {
    auto& context = fsr_backend(api);
    if (!input->resource) { out->internalIndex = 0; return FFX_OK; }
    try {
        const int index = free_index(context);
        auto& resource = context.resources[index];
        resource.texture = static_cast<rhi::ITexture*>(input->resource);
        resource.description = input->description;
        resource.external = true;
        views(resource);
        out->internalIndex = index;
        return FFX_OK;
    } catch (const std::exception& error) {
        context.error = error.what();
        return FFX_ERROR_BACKEND_API_ERROR;
    }
}
FfxErrorCode fsr_unregister_resources(FfxFsr2Interface* api) {
    for (auto& resource : fsr_backend(api).resources) if (resource.external) resource = {};
    return FFX_OK;
}
FfxErrorCode fsr_destroy_resource(FfxFsr2Interface* api, FfxResourceInternal resource) {
    if (resource.internalIndex > 0 && resource.internalIndex < 128)
        fsr_backend(api).resources[resource.internalIndex] = {};
    return FFX_OK;
}
FfxResourceDescription fsr_resource_description(FfxFsr2Interface* api, FfxResourceInternal resource) {
    return fsr_backend(api).resources.at(static_cast<size_t>(resource.internalIndex)).description;
}
}
