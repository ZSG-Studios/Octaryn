#include "WorldRendererInternal.h"
#include "WorldRasterPipeline.h"
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
using namespace octaryn::client::rendering;
constexpr unsigned Size=64;
using Vector=std::array<float,3>;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void checked(SlangResult value,const char* message) {require(SLANG_SUCCEEDED(value),message);}

Slang::ComPtr<rhi::IBuffer> buffer(WorldRenderer& renderer,const void* data,
    std::size_t size,unsigned stride,rhi::BufferUsage usage) {
  rhi::BufferDesc desc{};desc.size=size;desc.elementSize=stride;
  desc.usage=usage;desc.defaultState=usage==rhi::BufferUsage::IndirectArgument?
      rhi::ResourceState::IndirectArgument:rhi::ResourceState::ShaderResource;
  Slang::ComPtr<rhi::IBuffer> result;
  checked(renderer.device->createBuffer(desc,data,result.writeRef()),"fixture buffer creation");
  return result;
}

struct Fixture {
  WorldRenderer renderer;
  std::array<Slang::ComPtr<rhi::ITexture>,4> targets;
  std::array<Slang::ComPtr<rhi::ITextureView>,4> views;

  Fixture() {
    auto& r=renderer;
    const rhi::Feature required[]={rhi::Feature::Rasterization};
    rhi::DeviceDesc desc{};desc.deviceType=rhi::DeviceType::Vulkan;
    desc.requiredFeatures=required;desc.requiredFeatureCount=1;
    desc.slang.targetFlags=SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
    desc.slang.targetProfile="spirv_1_3";desc.enableValidation=true;desc.debugCallback=&r.debug;
    checked(rhi::getRHI()->createDevice(desc,r.device.writeRef()),"headless Vulkan device creation");
    checked(r.device->getQueue(rhi::QueueType::Graphics,r.queue.writeRef()),"graphics queue creation");
    const auto& info=r.device->getInfo();
    std::printf("raster_culling_device backend=slang_rhi api=%s adapter=%s\n",info.apiName,info.adapterName);
    require(create_world_raster_pipelines(r.device,r.raster_pipeline,r.sprite_pipeline,
        r.lava_pipeline,r.transparent_pipeline),"production raster pipeline creation");
    r.atlas=create_world_atlas(r.device);require(r.atlas!=nullptr,"production atlas creation");
    r.width=Size;r.height=Size;r.culling_enabled=false;r.pbr=false;r.pom=false;
    rhi::TextureDesc texture{};texture.size={Size,Size,1};
    texture.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::CopySource;
    texture.defaultState=rhi::ResourceState::RenderTarget;
    for(unsigned i=0;i<4;++i) {
      texture.format=world_gbuffer_formats[i];
      checked(r.device->createTexture(texture,nullptr,targets[i].writeRef()),"G-buffer target creation");
      checked(targets[i]->getDefaultView(views[i].writeRef()),"G-buffer view creation");
    }
    texture.format=rhi::Format::D32Float;
    texture.usage=rhi::TextureUsage::DepthStencil|rhi::TextureUsage::CopySource;
    texture.defaultState=rhi::ResourceState::DepthWrite;
    checked(r.device->createTexture(texture,nullptr,r.depth.writeRef()),"depth texture creation");
    checked(r.depth->getDefaultView(r.depth_view.writeRef()),"depth view creation");
  }

  void face(unsigned direction,unsigned block,bool sprite) {
    auto& r=renderer;
    // The production GPU mesher emits precisely world xyz + block/direction uint4.
    const std::array<std::uint32_t,4> record{0,0,0,block|(direction<<16)};
    const std::array<float,8> unused_fluid{};
    std::array<std::uint32_t,40> arguments{};
    const unsigned pass=sprite?1u:0u;
    arguments[pass*4]=6;arguments[pass*4+1]=1;
    arguments[20+pass*4]=6;arguments[20+pass*4+1]=1;
    // A unit face has one patch: face index zero, patch ordinal zero.
    const std::uint32_t patch=0;
    WorldColumnGpu column;column.face_count=1;column.pass_counts[pass]=1;column.patch_counts[pass]=1;
    column.min_y=0;column.height=1;
    column.faces=buffer(r,record.data(),sizeof(record),16,rhi::BufferUsage::ShaderResource);
    column.fluids=buffer(r,unused_fluid.data(),sizeof(unused_fluid),32,rhi::BufferUsage::ShaderResource);
    column.patches=buffer(r,&patch,sizeof(patch),4,rhi::BufferUsage::ShaderResource);
    column.arguments=buffer(r,arguments.data(),sizeof(arguments),0,rhi::BufferUsage::IndirectArgument);
    r.columns.insert_or_assign({0,0},std::move(column));
  }

  unsigned render(const Vector& center,const Vector& normal,bool outside,bool retain_depth=false) {
    auto& r=renderer;
    const float side=outside?1.f:-1.f;
    const Vector forward{-normal[0]*side,-normal[1]*side,-normal[2]*side};
    WorldCamera camera{};
    camera.x=center[0]+normal[0]*side*2;camera.y=center[1]+normal[1]*side*2;
    camera.z=center[2]+normal[2]*side*2;
    camera.yaw=std::atan2(forward[0],-forward[2]);camera.pitch=std::asin(forward[1]);
    camera.vertical_fov=1.0471975512f;
    world_renderer_prepare_draw(r,camera);
    require(r.draw_list.visible.size()==1,"fixture column missing from production draw list");
    auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"command encoder creation");
    std::array<rhi::RenderPassColorAttachment,4> colors{};
    for(unsigned i=0;i<4;++i) {
      colors[i].view=views[i];colors[i].loadOp=rhi::LoadOp::Clear;colors[i].storeOp=rhi::StoreOp::Store;
    }
    rhi::RenderPassDepthStencilAttachment depth{};depth.view=r.depth_view;
    depth.depthClearValue=1;depth.depthLoadOp=retain_depth?rhi::LoadOp::Load:rhi::LoadOp::Clear;
    depth.depthStoreOp=rhi::StoreOp::Store;
    depth.stencilLoadOp=rhi::LoadOp::DontCare;depth.stencilStoreOp=rhi::StoreOp::DontCare;
    rhi::RenderPassDesc pass{};pass.colorAttachments=colors.data();pass.colorAttachmentCount=4;
    pass.depthStencilAttachment=&depth;
    auto* encoder=commands->beginRenderPass(pass);require(encoder!=nullptr,"G-buffer render pass");
    rhi::RenderState state{};state.viewportCount=1;state.scissorRectCount=1;
    state.viewports[0]=rhi::Viewport::fromSize(static_cast<float>(Size),static_cast<float>(Size));
    state.scissorRects[0]=rhi::ScissorRect::fromSize(Size,Size);encoder->setRenderState(state);
    const bool drawn=world_renderer_draw(r,encoder,false);encoder->end();
    require(drawn,"production opaque/sprite draw");
    auto submission=commands->finish();require(submission!=nullptr,"command finish");
    checked(r.queue->submit(submission),"graphics submit");checked(r.queue->waitOnHost(),"graphics completion");
    Slang::ComPtr<ISlangBlob> pixels;rhi::SubresourceLayout layout{};
    checked(r.device->readTexture(r.depth,0,0,pixels.writeRef(),&layout),"depth readback");
    require(pixels && layout.colPitch==sizeof(float) && layout.rowPitch>=Size*sizeof(float) &&
        pixels->getBufferSize()>=layout.rowPitch*Size,"depth readback layout");
    unsigned depth_covered=0;
    const auto* bytes=static_cast<const unsigned char*>(pixels->getBufferPointer());
    for(unsigned y=0;y<Size;++y) for(unsigned x=0;x<Size;++x) {
      float value;std::memcpy(&value,bytes+y*layout.rowPitch+x*sizeof(float),sizeof(value));
      require(std::isfinite(value) && value>=0 && value<=1,"invalid depth pixel");
      if(value<1) ++depth_covered;
    }
    // The encoded voxel attachment is cleared on every draw, including a redraw
    // that retains depth. Nonzero pixels prove fragments passed the depth test.
    checked(r.device->readTexture(targets[2],0,0,pixels.writeRef(),&layout),"voxel G-buffer readback");
    require(pixels && layout.colPitch==4 && layout.rowPitch>=Size*4 &&
        pixels->getBufferSize()>=layout.rowPitch*Size,"voxel readback layout");
    bytes=static_cast<const unsigned char*>(pixels->getBufferPointer());
    unsigned covered=0;
    for(unsigned y=0;y<Size;++y) for(unsigned x=0;x<Size;++x) {
      const auto* pixel=bytes+y*layout.rowPitch+x*4;
      if(pixel[0] || pixel[1] || pixel[2] || pixel[3]) ++covered;
    }
    require(retain_depth || covered==depth_covered,"voxel/depth coverage disagreement");
    require(r.debug.errors.load()==0,"RHI or Vulkan validation errors");
    return covered;
  }
};
}

int main(int argc,char**) {
  try {
    require(argc==1,"usage: client_raster_culling_probe (staged assets beside executable)");
    Fixture fixture;
    // WorldFaces direction order: -X,+X,-Y,+Y,-Z,+Z. Stone is catalog block 5.
    const std::array<Vector,6> normals{{{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}}};
    for(unsigned face=0;face<6;++face) {
      const auto& n=normals[face];const Vector center{.5f+n[0]*.5f,.5f+n[1]*.5f,.5f+n[2]*.5f};
      fixture.face(face,5,false);
      const auto outside=fixture.render(center,n,true),equal=fixture.render(center,n,true,true);
      const auto inside=fixture.render(center,n,false);
      std::printf("raster_culling opaque_direction=%u outside_pixels=%u inside_pixels=%u equal_depth_pixels=%u\n",face,outside,inside,equal);
      require(outside>100 && inside==0,"opaque exterior must render and interior must be culled");
      require(equal==outside,"opaque equal-depth redraw must preserve every visible fragment");
    }
    // Real bluebell block 10 uses the production crossed-plane/LOD0-alpha sprite shader.
    // Faces 6/7 share one diagonal, 8/9 the other; test each authored face both ways.
    constexpr float diagonal=.7071067812f;
    for(unsigned face=6;face<10;++face) {
      const Vector n=face<8?Vector{-diagonal,0,diagonal}:Vector{diagonal,0,diagonal};
      fixture.face(face,10,true);
      const auto front=fixture.render({.5f,.5f,.5f},n,true);
      const auto equal=fixture.render({.5f,.5f,.5f},n,true,true);
      const auto back=fixture.render({.5f,.5f,.5f},n,false);
      std::printf("raster_culling sprite_direction=%u front_pixels=%u back_pixels=%u equal_depth_pixels=%u\n",face,front,back,equal);
      require(front>8 && back>8,"production cutout sprite must render from both sides");
      require(equal==front,"sprite equal-depth redraw must preserve every visible fragment");
    }
    std::printf("client_raster_culling_probe=passed opaque_views=12 sprite_views=8 equal_depth_redraws=10 validation_errors=0\n");
    return 0;
  } catch(const std::exception& error) {
    std::fprintf(stderr,"client_raster_culling_probe=failed reason=%s\n",error.what());return 1;
  }
}
