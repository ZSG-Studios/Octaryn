#include "Probe.h"
#include "ResourceProbePacing.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace mesh_probe {
namespace {
struct DrawReference {
  Slang::ComPtr<rhi::IBuffer> arguments;
  std::array<const WorldColumnGpu*,2> columns{};
};
struct DrawImage {
  std::array<std::vector<unsigned char>,world_gbuffer_formats.size()> colors;
  std::vector<unsigned char> depth;
};
unsigned material(const Fixture& f,const char* name) {
  const auto wanted=std::string("octaryn.basegame.block.")+name;
  for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].id==wanted)return i;
  throw std::runtime_error("counter draw material missing");
}
StreamColumn scene(Fixture& f,int column_x) {
  auto source=column(column_x,0);
  const auto stone=static_cast<std::uint16_t>(material(f,"stone"));
  for(int z=5;z<25;++z)for(int x=3;x<29;++x)put(source,x,1,z,stone);
  const std::array<const char*,4> names{"grass","glass","water","lava"};
  for(unsigned p=0;p<names.size();++p) {
    auto id=material(f,names[p]);
    if(p==0) {
      id=0;for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].sprite){id=i;break;}
      require(id!=0,"counter draw needs sprite material");
    }
    const int x=5+int(p)*6;
    for(int z=10;z<14;++z)for(int dx=0;dx<3;++dx)
      put(source,x+dx,2,z,static_cast<std::uint16_t>(id));
  }
  return source;
}
WorldCamera camera(unsigned view) {
  const float x=view?49.f:15.f,y=23,z=view?-43.f:75.f;
  const float dx=32-x,dy=3-y,dz=15-z,length=std::sqrt(dx*dx+dy*dy+dz*dz);
  return {x,y,z,std::atan2(dx,-dz),std::asin(dy/length),1.05f};
}
// Reference executes the actual completed emit counters through the previous
// individual indirect path. Production supplies its own direct draw records.
void indirect(WorldRenderer& r,rhi::IRenderPassEncoder* encoder,const DrawReference& reference,bool forward,int omitted) {
  constexpr std::array<unsigned,5> order{0,1,2,4,3};
  for(const auto pass:order) {
    if((pass>=2)!=forward)continue;
    if(int(pass)==omitted)continue;
    rhi::IShaderObject* root=nullptr;
    for(std::size_t i=0;i<r.draw_list.visible.size();++i) {
      const auto& mesh=*world_draw_item(r.draw_list,i,forward).column;
      if(!mesh.pass_counts[pass])continue;
      if(!root) {
        root=encoder->bindPipeline(pass==0?r.raster_pipeline:pass==1?r.sprite_pipeline:
            pass==4?r.lava_pipeline:r.transparent_pipeline);
        require(root && bind_world_atlas(r.atlas,root),"reference indirect pipeline/atlas");
      }
      auto uniforms=r.draw_uniforms;
      uniforms[28]=float(mesh.pass_counts[0]+mesh.pass_counts[1]+mesh.pass_counts[2]);
      checked(root->setBinding({0,0,0},rhi::Binding(mesh.faces)),"reference faces");
      checked(root->setBinding({0,7,0},rhi::Binding(mesh.fluids)),"reference fluids");
      checked(root->setBinding({0,8,0},rhi::Binding(mesh.patches)),"reference patches");
      checked(root->setData({0,0,0},uniforms.data(),sizeof(uniforms)),"reference uniforms");
      const auto found=std::find(reference.columns.begin(),reference.columns.end(),&mesh);
      require(found!=reference.columns.end(),"reference column identity");
      const auto index=std::size_t(found-reference.columns.begin());
      encoder->drawIndirect(1,{reference.arguments,index*160+80+pass*16});
    }
  }
}
std::vector<unsigned char> read(Fixture& f,rhi::ITexture* texture) {
  Slang::ComPtr<ISlangBlob> pixels;rhi::SubresourceLayout layout{};
  checked(f.renderer.device->readTexture(texture,0,0,pixels.writeRef(),&layout),"counter draw readback");
  const auto width=Fixture::Size*layout.colPitch;
  require(pixels && layout.rowPitch>=width && pixels->getBufferSize()>=layout.rowPitch*Fixture::Size,
      "counter draw readback layout");
  std::vector<unsigned char> bytes(width*Fixture::Size);
  for(unsigned y=0;y<Fixture::Size;++y)std::memcpy(bytes.data()+y*width,
      static_cast<const unsigned char*>(pixels->getBufferPointer())+y*layout.rowPitch,width);
  return bytes;
}
DrawImage render(Fixture& f,const DrawReference& reference,const WorldCamera& eye,bool direct,bool pbr,bool pom,int omitted=-1) {
  resource_probe::Frame paced(direct?"counter_direct_draw":"counter_indirect_reference");
  auto& r=f.renderer;r.pbr=pbr;r.pom=pom;world_renderer_prepare_draw(r,eye);
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"counter draw encoder");
  const auto count=world_gbuffer_target_count(false);
  std::array<rhi::RenderPassColorAttachment,world_gbuffer_formats.size()> colors{};
  for(unsigned i=0;i<count;++i) {colors[i].view=f.views[i];colors[i].loadOp=rhi::LoadOp::Clear;colors[i].storeOp=rhi::StoreOp::Store;}
  rhi::RenderPassDepthStencilAttachment depth{};depth.view=r.targets[0].depth_view;
  depth.depthClearValue=1;depth.depthLoadOp=rhi::LoadOp::Clear;depth.depthStoreOp=rhi::StoreOp::Store;
  depth.stencilLoadOp=rhi::LoadOp::DontCare;depth.stencilStoreOp=rhi::StoreOp::DontCare;
  rhi::RenderPassDesc pass{};pass.colorAttachments=colors.data();pass.colorAttachmentCount=count;pass.depthStencilAttachment=&depth;
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(Fixture::Size),float(Fixture::Size));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(Fixture::Size,Fixture::Size);
  auto* encoder=commands->beginRenderPass(pass);require(encoder!=nullptr,"counter opaque pass");
  encoder->setRenderState(state);
  if(direct)require(world_renderer_draw(r,encoder,false),"production individual opaque draw");
  else indirect(r,encoder,reference,false,omitted);
  encoder->end();
  colors[0].loadOp=rhi::LoadOp::Load;depth.depthLoadOp=rhi::LoadOp::Load;pass.colorAttachmentCount=1;
  encoder=commands->beginRenderPass(pass);require(encoder!=nullptr,"counter forward pass");
  encoder->setRenderState(state);
  if(direct)require(world_renderer_draw(r,encoder,true),"production individual forward draw");
  else indirect(r,encoder,reference,true,omitted);
  encoder->end();
  auto submission=commands->finish();require(bool(submission),"counter draw finish");
  require(r.frame_queue.submit(r.queue,submission,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
      "counter draw bounded completion");
  DrawImage result;
  for(unsigned i=0;i<count;++i)result.colors[i]=read(f,f.targets[i]);
  result.depth=read(f,r.targets[0].depth);return result;
}
}
void mesh_draw_counter_parity(Fixture& f) {
  auto& r=f.renderer;r.columns.clear();r.sources.clear();
  if(r.batch){r.batch->enabled=false;r.batch->prepared=false;}
  r.ray_enabled=false;r.lighting.sun_strength=.75f;r.lighting.ambient_strength=1;
  r.sky={};r.sky.light_direction_sky[1]=1;r.sky.light_direction_sky[3]=1;
  std::array<std::uint32_t,80> counters{};
  DrawReference reference;
  for(unsigned i=0;i<2;++i) {
    const auto source=scene(f,int(i));const auto mesh=f.mesh(source);
    require(mesh.counters_read,"counter draw needs actual emit readback");
    for(unsigned pass=0;pass<5;++pass)require(mesh.gpu.pass_counts[pass]>0 && mesh.gpu.patch_counts[pass]>0,
        "counter draw must exercise every material pass");
    for(unsigned pass=1;pass<5;++pass)require(mesh.counters[23+pass*4]>0,"counter draw lacks nonzero base instance");
    std::copy(mesh.counters.begin(),mesh.counters.end(),counters.begin()+i*40);
    const auto inserted=r.columns.emplace(std::make_pair(int(i),0),mesh.gpu);
    require(inserted.second,"counter draw duplicate column");reference.columns[i]=&inserted.first->second;
  }
  reference.arguments=buffer(r,counters.data(),sizeof(counters),4,rhi::BufferUsage::IndirectArgument);
  // Lazy native pipeline compilation is completed before timed frame admission.
  render(f,reference,camera(0),false,true,true);render(f,reference,camera(0),true,true,true);
  resource_probe::start();
  for(unsigned view=0;view<2;++view)for(unsigned mode=0;mode<3;++mode) {
    const auto expected=render(f,reference,camera(view),false,mode!=0,mode==2);
    const auto actual=render(f,reference,camera(view),true,mode!=0,mode==2);
    require(actual.depth==expected.depth,"direct mesh draw changed exact depth");
    require(actual.colors==expected.colors,"direct mesh draw changed exact color/material bytes");
    if(view==0 && mode==0)for(int pass=0;pass<5;++pass) {
      const auto missing=render(f,reference,camera(view),false,false,false,pass);
      require(missing.depth!=expected.depth || missing.colors!=expected.colors,
          "counter draw fixture contains an invisible material pass");
    }
    unsigned covered{};
    for(std::size_t offset=0;offset<actual.depth.size();offset+=sizeof(float)) {
      float value{};std::memcpy(&value,actual.depth.data()+offset,sizeof(value));covered+=value<1;
    }
    require(covered>100,"counter draw comparison contains no useful geometry");
    std::printf("world_mesh_counter_draw view=%u mode=%u passes=5 columns=2 covered=%u color=byte_identical depth=identical\n",view,mode,covered);
  }
  require(r.debug.errors.load()==0,"counter draw graphics validation errors");
  r.columns.clear();r.sources.clear();
  std::puts("world_mesh_draw_counters=passed passes=5 visible_passes=5 nonzero_instance=1 reversed_view=1 gpu_emit_reference=1");
}
}
