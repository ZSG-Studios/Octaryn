#pragma once
#include "SkyData.h"
#include <slang-rhi.h>
#include <slang-com-ptr.h>
namespace octaryn::client::rendering {
bool create_cloud_pipeline(rhi::IDevice*,rhi::Format color_format,rhi::Format depth_format,
                           const char* shader_path,Slang::ComPtr<rhi::IRenderPipeline>&);
bool render_clouds(rhi::IRenderPassEncoder*,rhi::IRenderPipeline*,const SkyUniforms&,
                   const float camera_position[3],float yaw,float pitch,float vertical_fov,
                   int width,int height,float max_distance,float near_plane,float far_plane,float jitter_x=0,float jitter_y=0);
}
