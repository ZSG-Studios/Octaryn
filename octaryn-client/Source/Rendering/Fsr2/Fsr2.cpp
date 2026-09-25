#include "Fsr2Internal.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace octaryn::client::rendering {
FfxFsr2Interface fsr_interface(Fsr2Context& context) {
    FfxFsr2Interface api{};
    api.scratchBuffer = &context;
    api.scratchBufferSize = sizeof(context);
    api.fpCreateBackendContext = [](FfxFsr2Interface*, FfxDevice) { return FFX_OK; };
    api.fpDestroyBackendContext = [](FfxFsr2Interface*) { return FFX_OK; };
    api.fpGetDeviceCapabilities = [](FfxFsr2Interface*, FfxDeviceCapabilities* caps, FfxDevice) {
        *caps = {};
        caps->minimumSupportedShaderModel = FFX_SHADER_MODEL_6_0;
        // Portable FP32 path uses group shared SPD; no fixed wave width assumed.
        caps->waveLaneCountMin = 32;
        caps->waveLaneCountMax = 64;
        return FFX_OK;
    };
    api.fpCreateResource = fsr_create_resource;
    api.fpRegisterResource = fsr_register_resource;
    api.fpUnregisterResources = fsr_unregister_resources;
    api.fpGetResourceDescription = fsr_resource_description;
    api.fpDestroyResource = fsr_destroy_resource;
    api.fpCreatePipeline = fsr_create_pipeline;
    api.fpDestroyPipeline = fsr_destroy_pipeline;
    api.fpScheduleGpuJob = fsr_schedule;
    api.fpExecuteGpuJobs = fsr_execute;
    return api;
}
Fsr2Context* create_fsr2(rhi::IDevice* device, const Fsr2CreateDesc& desc) {
    // SDK luminance is half-size and unconditionally binds its sixth mip.
    if (!device || desc.max_render_width < 2 || desc.max_render_height < 2 ||
        std::max(desc.max_render_width, desc.max_render_height) < 64 ||
        !desc.display_width || !desc.display_height ||
        desc.max_render_width > desc.display_width || desc.max_render_height > desc.display_height) return nullptr;
    auto context = std::make_unique<Fsr2Context>();
    context->device = device;
    context->description = desc;
    rhi::FormatSupport atomic_support{};
    if (SLANG_FAILED(device->getFormatSupport(rhi::Format::R32Uint, &atomic_support)) ||
        !rhi::is_set(atomic_support, rhi::FormatSupport::ShaderAtomic)) {
        std::fprintf(stderr, "FSR 2.2.1 requires native R32Uint texture atomics on this backend\n");
        return nullptr;
    }
    rhi::SamplerDesc sampler{};
    sampler.addressU = sampler.addressV = sampler.addressW = rhi::TextureAddressingMode::ClampToEdge;
    sampler.minFilter = sampler.magFilter = sampler.mipFilter = rhi::TextureFilteringMode::Point;
    context->point = device->createSampler(sampler);
    sampler.minFilter = sampler.magFilter = sampler.mipFilter = rhi::TextureFilteringMode::Linear;
    context->linear = device->createSampler(sampler);
    if (!context->point || !context->linear) return nullptr;
    FfxFsr2ContextDescription sdk{};
    sdk.device = device;
    sdk.callbacks = fsr_interface(*context);
    sdk.maxRenderSize = {desc.max_render_width, desc.max_render_height};
    sdk.displaySize = {desc.display_width, desc.display_height};
    sdk.flags = (desc.hdr ? FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE : 0) |
        (desc.depth_inverted ? FFX_FSR2_ENABLE_DEPTH_INVERTED : 0) |
        (desc.depth_infinite ? FFX_FSR2_ENABLE_DEPTH_INFINITE : 0) |
        (desc.dynamic_resolution ? FFX_FSR2_ENABLE_DYNAMIC_RESOLUTION : 0);
    const auto result = ffxFsr2ContextCreate(&context->sdk, &sdk);
    if (result != FFX_OK) {
        std::fprintf(stderr, "FSR 2.2.1 context failed: %d %s\n", result, context->error.c_str());
        return nullptr;
    }
    context->created = true;
    return context.release();
}
void destroy_fsr2(Fsr2Context* context) {
    if (!context) return;
    if (context->created) ffxFsr2ContextDestroy(&context->sdk);
    delete context;
}
bool dispatch_fsr2(Fsr2Context* c, rhi::ICommandEncoder* commands, const Fsr2Dispatch& d) {
    if (!c || !commands) return false;
    c->error.clear();
    if (!d.color || !d.depth || !d.motion_vectors || !d.output ||
        !d.render_width || !d.render_height || d.render_width > c->description.max_render_width ||
        d.render_height > c->description.max_render_height || !std::isfinite(d.delta_ms) ||
        d.delta_ms < 0 || d.pre_exposure <= 0 || !std::isfinite(d.pre_exposure)) {
        c->error = "Invalid FSR2 dispatch inputs";
        return false;
    }
    FfxFsr2DispatchDescription p{};
    p.commandList = commands;
    p.color = fsr_external(d.color);
    p.depth = fsr_external(d.depth);
    p.motionVectors = fsr_external(d.motion_vectors);
    p.reactive = fsr_external(d.reactive);
    p.transparencyAndComposition = fsr_external(d.transparency);
    p.exposure = fsr_external(d.exposure);
    p.output = fsr_external(d.output);
    p.renderSize = {d.render_width, d.render_height};
    p.jitterOffset = {d.jitter_x, d.jitter_y};
    p.motionVectorScale = {d.motion_scale_x, d.motion_scale_y};
    p.enableSharpening = d.sharpen;
    p.sharpness = std::clamp(d.sharpness, 0.0f, 1.0f);
    p.frameTimeDelta = d.delta_ms;
    p.preExposure = d.pre_exposure;
    p.reset = d.reset;
    p.cameraNear = d.near_plane;
    p.cameraFar = d.far_plane;
    p.cameraFovAngleVertical = d.vertical_fov;
    p.viewSpaceToMetersFactor = 1;
    std::memcpy(p.reprojectionMatrix, d.reprojection, sizeof(p.reprojectionMatrix));
    commands->globalBarrier();
    const auto result = ffxFsr2ContextDispatch(&c->sdk, &p);
    commands->globalBarrier();
    if (result != FFX_OK && c->error.empty()) c->error = "FSR2 SDK dispatch failed: " + std::to_string(result);
    return result == FFX_OK && c->error.empty();
}
const char* fsr2_error(const Fsr2Context* c) { return c ? c->error.c_str() : "FSR2 context unavailable"; }
uint64_t fsr2_gpu_bytes(const Fsr2Context* c) {
    uint64_t bytes = 0;
    if (c) for (const auto& resource : c->resources) if (!resource.external) bytes += resource.bytes;
    return bytes;
}
Fsr2Jitter fsr2_jitter(uint32_t frame, uint32_t render_width, uint32_t display_width) {
    Fsr2Jitter result;
    if (render_width && display_width) {
        const int phase = ffxFsr2GetJitterPhaseCount(static_cast<int32_t>(render_width), static_cast<int32_t>(display_width));
        if (phase <= 0) return result;
        ffxFsr2GetJitterOffset(&result.x, &result.y, static_cast<int32_t>(frame % static_cast<uint32_t>(phase)), phase);
    }
    return result;
}
}
