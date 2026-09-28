#include "CloudRenderer.h"
#include "RhiShader.h"
#include "SlangShaderPath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <slang-rhi/shader-cursor.h>
namespace octaryn::client::rendering {
bool create_cloud_pipeline(rhi::IDevice* device,rhi::Format color_format,rhi::Format depth_format,
                           const char* path,Slang::ComPtr<rhi::IRenderPipeline>& pipeline) {
    const char* entries[]={"vertex_main","fragment_main"};
    const auto* option=std::getenv("OCTARYN_CLIENT_CLOUD_OCCLUSION");
    if(option && std::strcmp(option,"0") && std::strcmp(option,"1")) {
        std::fprintf(stderr,"invalid_cloud_occlusion_mode\n");return false;
    }
    bool qualified_default=false;
#ifdef _WIN32
    const auto backend=device->getInfo().deviceType;
    qualified_default=backend==rhi::DeviceType::D3D12 || backend==rhi::DeviceType::Vulkan;
#endif
    const bool occlusion=option?std::strcmp(option,"1")==0:qualified_default;
    const auto selected=occlusion?resolve_slang_shader_path("octaryn-client/Shaders/Sky/CloudsWithOcclusion.slang"):std::string(path);
    Slang::ComPtr<rhi::IShaderProgram> program;
    if (selected.empty() || !create_rhi_program(device,selected.c_str(),entries,2,program)) return false;
    rhi::ColorTargetDesc target{};
    target.format=color_format;target.enableBlend=true;
    target.color={rhi::BlendFactor::SrcAlpha,rhi::BlendFactor::InvSrcAlpha,rhi::BlendOp::Add};
    target.alpha={rhi::BlendFactor::SrcAlpha,rhi::BlendFactor::InvSrcAlpha,rhi::BlendOp::Add};
    rhi::RenderPipelineDesc state{};
    state.program=program;state.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
    state.targets=&target;state.targetCount=1;state.rasterizer.cullMode=rhi::CullMode::None;
    state.depthStencil.format=depth_format;
    state.depthStencil.depthTestEnable=true;state.depthStencil.depthWriteEnable=false;
    state.depthStencil.depthFunc=rhi::ComparisonFunc::LessEqual;
    if (SLANG_FAILED(device->createRenderPipeline(state,pipeline.writeRef()))) return false;
    std::printf("clouds occlusion=%u samples_unchanged=1 qualified_default=%u\n",
        occlusion?1u:0u,qualified_default?1u:0u);std::fflush(stdout);
    return true;
}
bool render_clouds(rhi::IRenderPassEncoder* encoder,rhi::IRenderPipeline* pipeline,const SkyUniforms& sky,
                   const float camera[3],float yaw,float pitch,float vertical_fov,
                   int width,int height,float max_distance,float near_plane,float far_plane,float jitter_x,float jitter_y,
                   rhi::ITextureView* opaque_positions) {
    if (!encoder || !pipeline || !camera || width<=0 || height<=0 ||
        !(max_distance>0) || !(near_plane>0) || !(far_plane>near_plane)) return false;
    const float sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    const float focal=1/std::tan(std::clamp(vertical_fov,0.2f,2.7f)/2);
    struct Uniforms { float light[4],time[4],camera[4],right[4],up[4],forward[4],projection[4]; };
    const Uniforms data{
        {sky.light_direction_sky[0],sky.light_direction_sky[1],sky.light_direction_sky[2],sky.light_direction_sky[3]},
        {sky.twilight_celestial_time[0],sky.twilight_celestial_time[1],sky.twilight_celestial_time[3],max_distance},
        {camera[0],camera[1],camera[2],192.33f},
        {cy,0,sy,jitter_x},{-sy*sp,cp,cy*sp,jitter_y},{sy*cp,sp,-cy*cp,0},
        {focal*static_cast<float>(height)/static_cast<float>(width),focal,
         far_plane/(far_plane-near_plane),far_plane*near_plane/(far_plane-near_plane)}};
    static_assert(sizeof(Uniforms)==112);
    auto* root=encoder->bindPipeline(pipeline);
    if (!root || SLANG_FAILED(root->setData({0,0,0},&data,sizeof(data)))) return false;
    auto positions=rhi::ShaderCursor(root)["cloudOpaquePositions"];
    if (positions.isValid() && (!opaque_positions || SLANG_FAILED(positions.setBinding(opaque_positions)))) return false;
    rhi::DrawArguments draw{};draw.vertexCount=3;
    encoder->draw(draw);
    return true;
}
}
