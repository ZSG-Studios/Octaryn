#include "Probe.h"
#include "ShadowFallbackSystem.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace mesh_probe {
namespace {
constexpr unsigned Resolution=256;
using Depth=std::array<std::vector<float>,3>;
unsigned material(const Fixture& f,const char* name) {
  const std::string id=std::string("octaryn.basegame.block.")+name;
  for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].id==id)return i;
  require(false,"shadow batch material absent");return 0;
}
class ShadowProbe {
  WorldRenderer& r;
  std::array<std::array<Slang::ComPtr<rhi::ITexture>,3>,2> captured;
public:
  explicit ShadowProbe(WorldRenderer& renderer):r(renderer) {
    r.lighting_settings.shadow_resolution=Resolution;
    require(initialize_shadow_fallback(r),"shadow batch pipeline initialization");
    require(r.shadow_fallback.batch.available,"shadow fixture requires actual indirect/bindless capabilities");
    rhi::TextureDesc desc{};desc.size={Fixture::Size,Fixture::Size,1};desc.format=rhi::Format::RGBA32Float;
    desc.usage=rhi::TextureUsage::ShaderResource;desc.defaultState=rhi::ResourceState::ShaderResource;
    std::vector<std::array<float,4>> zero(Fixture::Size*Fixture::Size);
    rhi::SubresourceData data{zero.data(),Fixture::Size*16,zero.size()*16};
    Slang::ComPtr<rhi::ITexture> positions;Slang::ComPtr<rhi::ITextureView> view;
    checked(r.device->createTexture(desc,&data,positions.writeRef()),"shadow empty receiver creation");
    checked(positions->getDefaultView(view.writeRef()),"shadow empty receiver view");
    for(auto& target:r.targets) {target.hdr.views[1]=view;target.hdr.views[2]=view;}
    desc.format=rhi::Format::R32Float;desc.defaultState=rhi::ResourceState::ShaderResource;
    desc.usage=rhi::TextureUsage::UnorderedAccess|rhi::TextureUsage::ShaderResource;
    for(auto& target:r.targets) {
      checked(r.device->createTexture(desc,nullptr,target.hdr.sun_visibility.writeRef()),"shadow resolved visibility");
      checked(target.hdr.sun_visibility->getDefaultView(target.hdr.sun_visibility_view.writeRef()),"shadow visibility view");
    }
    desc.size={Resolution,Resolution,1};desc.format=rhi::Format::D32Float;
    desc.usage=rhi::TextureUsage::CopyDestination|rhi::TextureUsage::CopySource|rhi::TextureUsage::DepthStencil;
    desc.defaultState=rhi::ResourceState::CopyDestination;
    for(auto& frame:captured)for(auto& depth:frame)
      checked(r.device->createTexture(desc,nullptr,depth.writeRef()),"shadow retained depth capture");
    r.draw_uniforms[0]=32;r.draw_uniforms[1]=20;r.draw_uniforms[2]=16;
    const float direction[4]={.25f,-.95f,.18f,1};std::copy_n(direction,4,r.sky.light_direction_sky);
  }
  ~ShadowProbe() {r.frame_queue.drain();r.queue->waitOnHost();}
  unsigned submit(bool enabled,unsigned limit) {
    const auto slot=r.frame_queue.slot(r.frames++);require(r.frame_queue.wait(slot),"shadow frame fence");
    r.active_frame=slot;auto& batch=r.shadow_fallback.batch;batch.enabled=enabled;batch.max_draws=limit;
    require(shadow_batch_begin_frame(batch,slot),"shadow completed frame release");
    auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"shadow command encoder");
    require(update_shadow_fallback(r,commands),"production sun fallback update");
    for(unsigned level=0;level<3;++level)commands->copyTexture(captured[slot][level],{0,1,0,1},{},
        r.shadow_fallback.depth[level],{0,1,0,1},{},{Resolution,Resolution,1});
    auto command=commands->finish();require(command!=nullptr,"shadow command finish");
    require(r.frame_queue.submit(r.queue,command,slot),"shadow frame submission");return slot;
  }
  Depth read(unsigned slot) {
    require(r.frame_queue.wait(slot),"shadow capture fence");Depth result;
    for(unsigned level=0;level<3;++level) {
      Slang::ComPtr<ISlangBlob> bytes;rhi::SubresourceLayout layout{};
      checked(r.device->readTexture(captured[slot][level],0,0,bytes.writeRef(),&layout),"shadow depth readback");
      require(bytes && layout.colPitch==sizeof(float) && layout.rowPitch>=Resolution*sizeof(float),"shadow depth layout");
      auto& depth=result[level];depth.resize(Resolution*Resolution);
      for(unsigned y=0;y<Resolution;++y)
        std::memcpy(depth.data()+y*Resolution,static_cast<const char*>(bytes->getBufferPointer())+y*layout.rowPitch,Resolution*sizeof(float));
      for(float value:depth)require(std::isfinite(value) && value>=0 && value<=1,"invalid shadow depth");
    }
    return result;
  }
};
void same(const Depth& expected,const Depth& actual) {
  for(unsigned level=0;level<3;++level)require(expected[level]==actual[level],"indirect sun shadows changed exact depth/alpha/fluid geometry");
}
}
void shadow_batch_cases(Fixture& f) {
  auto& r=f.renderer;r.columns.clear();r.sources.clear();r.culling_enabled=true;
  const auto stone=material(f,"stone"),rose=material(f,"rose"),water=material(f,"water"),lava=material(f,"lava_4"),glass=material(f,"glass");
  for(int x:{0,1,12}) {
    auto source=column(x,0);
    for(int z=4;z<13;++z)for(int y=3;y<7;++y)for(int bx=4;bx<13;++bx)put(source,bx,y,z,static_cast<std::uint16_t>(stone));
    put(source,23,7,20,static_cast<std::uint16_t>(rose));
    put(source,17,6,10,static_cast<std::uint16_t>(water));put(source,21,6,10,static_cast<std::uint16_t>(lava));
    put(source,26,6,10,static_cast<std::uint16_t>(glass));
    const auto mesh=f.mesh(source);
    require(mesh.gpu.pass_counts[0] && mesh.gpu.pass_counts[1] && mesh.gpu.pass_counts[2] && mesh.gpu.pass_counts[3] && mesh.gpu.pass_counts[4],
        "shadow fixture must exercise opaque, sprite, glass, water and lava ranges");
    world_renderer_store_column(r,{x,0},mesh.gpu);
  }
  // All geometry is outside the camera view, but remains eligible as a caster.
  world_renderer_prepare_draw(r,{2000,20,2000,0,0,1.05f});require(!r.drawn_columns,"shadow fixture casters are not off-camera");
  ShadowProbe probe(r);auto& batch=r.shadow_fallback.batch;const auto limit=batch.max_draws;
  const auto reference=probe.read(probe.submit(false,limit));const auto direct_draws=batch.submitted_draws;
  require(direct_draws>3 && batch.submitted_commands==direct_draws,"direct shadow reference lacked separate caster draws");
  for(unsigned level=0;level<3;++level)
    require(std::count_if(reference[level].begin(),reference[level].end(),[](float value){return value<1;})>0,
        "shadow reference clipmap contains no caster samples");
  same(reference,probe.read(probe.submit(true,limit)));
  require(batch.submitted_commands==3 && batch.submitted_draws==direct_draws && batch.count[2]>batch.count[0],
      "shadow batching lost casters, ignored clipmap culling or did not reduce submissions");
  same(reference,probe.read(probe.submit(true,1)));
  require(batch.submitted_commands==direct_draws,"shadow indirect limit splitting lost record indices");
  const auto first=probe.submit(true,limit);
  std::vector<unsigned char> records(batch.records.size()*sizeof(ShadowBatchRecord));
  std::memcpy(records.data(),batch.frames[first].mapped_records,records.size());
  open_world_renderer_set_center(&r,100,100,0);
  require(r.columns.empty() && r.sources.empty(),"shadow retained frame fixture did not evict live owners");
  const auto second=probe.submit(true,limit);require(first!=second,"shadow retained frames share a slot");
  require(!batch.frames[first].retained.empty() && batch.frames[second].retained.empty() &&
      std::memcmp(records.data(),batch.frames[first].mapped_records,records.size())==0,
      "shadow frame eviction rewrote retained indirect records or released in-flight mesh owners");
  same(reference,probe.read(first));const auto cleared=probe.read(second);
  for(const auto& level:cleared)require(std::all_of(level.begin(),level.end(),[](float value){return value==1;}),
      "evicted shadow casters survived the next frame");
  require(shadow_batch_begin_frame(batch,first) && batch.frames[first].retained.empty(),"completed shadow owners were not released");
  // Retire real retained shadow references through the bounded production path.
  auto source=column();put(source,4,4,4,static_cast<std::uint16_t>(stone));const auto mesh=f.mesh(source);
  world_renderer_store_column(r,{0,0},mesh.gpu);(void)probe.read(probe.submit(true,limit));
  require(open_world_renderer_begin_retirement(&r),"shadow retirement queue synchronization");
  for(unsigned step=0;open_world_renderer_retire_step(&r,1);++step) {
    require(step<8,"shadow retained retirement failed to converge");
    require(open_world_renderer_retirement_frame(&r),"shadow real retirement maintenance frame");
  }
  for(const auto& frame:batch.frames)require(frame.retained.empty(),"shadow retirement omitted secondary owners");
  require(r.debug.errors.load()==0,"shadow batch graphics validation errors");
  std::printf("world_shadow_batch=passed levels=3 depth=byte_identical opaque=1 sprite_alpha=1 lava=1 water_glass_transmit=1 "
      "off_camera=1 clipmap_culling=1 indirect_limit=1 retained_frames=2 eviction=1 retirement=1 direct_draws=%u batch_commands=3 validation_errors=0\n",direct_draws);
}
}
