#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>

namespace octaryn::client::rendering {
bool create_world_raster_pipelines(rhi::IDevice* device,
    Slang::ComPtr<rhi::IRenderPipeline>& opaque,
    Slang::ComPtr<rhi::IRenderPipeline>& sprite,
    Slang::ComPtr<rhi::IRenderPipeline>& lava,
    Slang::ComPtr<rhi::IRenderPipeline>& transparent);
}
