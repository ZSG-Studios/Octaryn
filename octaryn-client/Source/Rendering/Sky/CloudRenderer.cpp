#include "CloudRenderer.h"
#include "RhiShader.h"
#include <algorithm>
#include <cmath>
namespace octaryn::client::rendering {
bool create_cloud_pipeline(rhi::IDevice* device,rhi::Format color_format,rhi::Format depth_format,
                           const char* path,Slang::ComPtr<rhi::IRenderPipeline>& pipeline) {
    const char* entries[]={"vertex_main","fragment_main"};
    Slang::ComPtr<rhi::IShaderProgram> program;
    if (!create_rhi_program(device,path,entries,2,program)) return false;
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
    return SLANG_SUCCEEDED(device->createRenderPipeline(state,pipeline.writeRef()));
}
bool render_clouds(rhi::IRenderPassEncoder* encoder,rhi::IRenderPipeline* pipeline,const SkyUniforms& sky,
                   const float camera[3],float yaw,float pitch,float vertical_fov,
                   int width,int height,float max_distance,float near_plane,float far_plane,float jitter_x,float jitter_y) {
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
    rhi::DrawArguments draw{};draw.vertexCount=3;
    encoder->draw(draw);
    return true;
}
}
