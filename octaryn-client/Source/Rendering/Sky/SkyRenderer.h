#pragma once
#include "SkyData.h"
#include <slang-rhi.h>
#include <slang-com-ptr.h>
namespace octaryn::client::rendering {
bool create_sky_pipeline(rhi::IDevice*,rhi::Format color_format,rhi::Format depth_format,const char* shader_path,Slang::ComPtr<rhi::IRenderPipeline>&);
bool render_sky(rhi::IRenderPassEncoder*,rhi::IRenderPipeline*,const SkyUniforms&,
                float yaw,float pitch,float vertical_fov,int width,int height,float jitter_x=0,float jitter_y=0);
}
