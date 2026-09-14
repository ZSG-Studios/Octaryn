#include "WorldTemporal.h"
#include "RhiShader.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
using namespace octaryn::client::rendering;
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
}

void qualify_jitter_raster(rhi::IDevice* device,rhi::ICommandQueue* queue,unsigned display_width) {
    constexpr unsigned width=64,height=48;
    WorldTemporal temporal;
    temporal.mode=display_width==width?1u:2u;
    temporal.width=width;temporal.height=height;temporal.display_width=display_width;
    temporal.display_height=display_width*height/width;
    WorldCamera eye{};eye.vertical_fov=1.570796327f;
    Slang::ComPtr<rhi::IShaderProgram> program;
    const char* entries[]={"vertex_main","fragment_main"};
    require(create_rhi_program(device,"octaryn-client/Shaders/Sky/JitterRaster.slang",entries,2,program),
        "production jitter raster shader creation");
    rhi::ColorTargetDesc target{};target.format=rhi::Format::RGBA32Float;
    rhi::RenderPipelineDesc desc{};desc.program=program;desc.targets=&target;desc.targetCount=1;
    desc.rasterizer.cullMode=rhi::CullMode::None;
    Slang::ComPtr<rhi::IRenderPipeline> pipeline;
    require(SLANG_SUCCEEDED(device->createRenderPipeline(desc,pipeline.writeRef())),"jitter raster pipeline creation");
    rhi::TextureDesc texture{};texture.size={width,height,1};texture.format=target.format;
    texture.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::CopySource;
    texture.defaultState=rhi::ResourceState::RenderTarget;
    auto color=device->createTexture(texture);
    require(color!=nullptr,"jitter raster color creation");
    auto view=color->createView({});require(view!=nullptr,"jitter raster color view");
    float worst=0;
    for(unsigned frame=0;frame<18;++frame) {
        const auto camera=begin_temporal(temporal,eye,frame);
        auto basis=temporal_view(camera,width,height);
        basis.right[3]=camera.jitter_x;basis.up[3]=camera.jitter_y;
        const std::array<std::array<float,4>,4> uniforms{basis.right,basis.up,basis.forward,basis.projection};
        auto commands=queue->createCommandEncoder();require(commands!=nullptr,"jitter raster encoder");
        rhi::RenderPassColorAttachment attachment{};attachment.view=view;attachment.loadOp=rhi::LoadOp::Clear;
        rhi::RenderPassDesc pass_desc{};pass_desc.colorAttachments=&attachment;pass_desc.colorAttachmentCount=1;
        auto* pass=commands->beginRenderPass(pass_desc);require(pass!=nullptr,"jitter raster pass");
        rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
        state.viewports[0]=rhi::Viewport::fromSize(float(width),float(height));
        state.scissorRects[0]=rhi::ScissorRect::fromSize(width,height);pass->setRenderState(state);
        auto* root=pass->bindPipeline(pipeline);
        require(root && SLANG_SUCCEEDED(root->setData({0,0,0},uniforms.data(),sizeof(uniforms))),"jitter raster uniforms");
        rhi::DrawArguments draw{};draw.vertexCount=3;pass->draw(draw);pass->end();
        auto submission=commands->finish();
        require(submission && SLANG_SUCCEEDED(queue->submit(submission)),"jitter raster submission");
        commit_temporal(temporal);
        require(SLANG_SUCCEEDED(queue->waitOnHost()),"jitter raster completion");
        Slang::ComPtr<ISlangBlob> pixels;rhi::SubresourceLayout layout{};
        require(SLANG_SUCCEEDED(device->readTexture(color,0,0,pixels.writeRef(),&layout)) &&
            layout.colPitch==16 && layout.rowPitch>=width*16,"jitter raster readback");
        float error=0;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const auto* actual=reinterpret_cast<const float*>(static_cast<const char*>(pixels->getBufferPointer())+
                y*layout.rowPitch+x*layout.colPitch);
            // Independent AMD 2.2.1 sample location: (pixel + .5) - SDK Jitter().
            // The actual pixels above use production begin_temporal and SkyRay.slang.
            const float sample_x=(float(x)+.5f-temporal.jitter.x)/float(width);
            const float sample_y=(float(y)+.5f-temporal.jitter.y)/float(height);
            const float expected[]={.5f+(sample_x*2-1)*float(width)/float(height)*.2f,
                .5f+(1-sample_y*2)*.2f,.3f,1};
            for(unsigned channel=0;channel<4;++channel) {
                require(std::isfinite(actual[channel]),"nonfinite jitter raster pixel");
                error=std::max(error,std::abs(actual[channel]-expected[channel]));
            }
        }
        std::printf("fsr2_jitter_raster mode=%u frame=%u render=%ux%u display=%ux%u max_error=%g jitter=(%g,%g)\n",
            temporal.mode,frame,width,height,temporal.display_width,temporal.display_height,error,temporal.jitter.x,temporal.jitter.y);
        require(error<.00001f,"Production raster jitter disagrees with FSR2 sample positions");
        worst=std::max(worst,error);
    }
    std::printf("fsr2_jitter_raster=passed mode=%u frames=18 pixels=%u max_error=%g\n",temporal.mode,width*height*18,worst);
}
