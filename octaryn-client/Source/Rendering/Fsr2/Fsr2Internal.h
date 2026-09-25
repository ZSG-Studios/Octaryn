#pragma once
#include "Fsr2.h"
#include <ffx_fsr2.h>
#include <slang-com-ptr.h>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace octaryn::client::rendering {
struct Fsr2Resource {
    Slang::ComPtr<rhi::ITexture> texture;
    Slang::ComPtr<rhi::ITextureView> srv;
    std::vector<Slang::ComPtr<rhi::ITextureView>> mips;
    FfxResourceDescription description{};
    uint64_t bytes{};
    bool external{};
};
struct Fsr2Pipeline {
    Slang::ComPtr<rhi::IComputePipeline> pipeline;
    std::vector<std::string> srv, uav, constants;
};
struct Fsr2Context {
    Slang::ComPtr<rhi::IDevice> device;
    Slang::ComPtr<rhi::ISampler> point, linear;
    Fsr2CreateDesc description;
    FfxFsr2Context sdk{};
    std::array<Fsr2Resource, 128> resources;
    std::vector<FfxGpuJobDescription> jobs;
    std::vector<std::unique_ptr<Fsr2Pipeline>> pipelines;
    std::string error;
    bool created{};
};
inline Fsr2Context& fsr_backend(FfxFsr2Interface* api) {
    return *static_cast<Fsr2Context*>(api->scratchBuffer);
}
FfxFsr2Interface fsr_interface(Fsr2Context&);
FfxResource fsr_external(rhi::ITexture*);
FfxErrorCode fsr_create_resource(FfxFsr2Interface*, const FfxCreateResourceDescription*, FfxResourceInternal*);
FfxErrorCode fsr_register_resource(FfxFsr2Interface*, const FfxResource*, FfxResourceInternal*);
FfxErrorCode fsr_unregister_resources(FfxFsr2Interface*);
FfxErrorCode fsr_destroy_resource(FfxFsr2Interface*, FfxResourceInternal);
FfxResourceDescription fsr_resource_description(FfxFsr2Interface*, FfxResourceInternal);
FfxErrorCode fsr_create_pipeline(FfxFsr2Interface*, FfxFsr2Pass, const FfxPipelineDescription*, FfxPipelineState*);
FfxErrorCode fsr_destroy_pipeline(FfxFsr2Interface*, FfxPipelineState*);
FfxErrorCode fsr_schedule(FfxFsr2Interface*, const FfxGpuJobDescription*);
FfxErrorCode fsr_execute(FfxFsr2Interface*, FfxCommandList);
}
