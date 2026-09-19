#include "Probe.h"
#include "AssetPath.h"
#include "SlangShaderPath.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

namespace mesh_probe {
namespace {
using Pixel=std::array<float,4>;
constexpr unsigned Size=32,Count=Size*Size,Center=16*Size+16;
constexpr std::array<double,3> Surface{.125,1.1,-5};
struct Texture {Slang::ComPtr<rhi::ITexture> texture;Slang::ComPtr<rhi::ITextureView> view;};
Texture image(WorldRenderer& r,const std::vector<Pixel>& values) {
  rhi::TextureDesc d{};d.size={Size,Size,1};d.format=rhi::Format::RGBA32Float;
  d.usage=rhi::TextureUsage::ShaderResource;d.defaultState=rhi::ResourceState::ShaderResource;
  rhi::SubresourceData data{values.data(),Size*sizeof(Pixel),Count*sizeof(Pixel)};
  Texture result;checked(r.device->createTexture(d,&data,result.texture.writeRef()),"direct fixture texture");
  checked(result.texture->getDefaultView(result.view.writeRef()),"direct fixture view");return result;
}
float half(std::uint16_t bits) {
  const int exponent=(bits>>10)&31,sign=(bits&32768)?-1:1;const unsigned mantissa=bits&1023;
  require(exponent!=31,"nonfinite direct-light output");
  return float(sign)*std::ldexp(float(exponent?mantissa+1024:mantissa),exponent?exponent-25:-24);
}
std::array<double,3> oracle(const std::vector<WorldLocalLight>& lights) {
  std::array<double,3> value{};
  for(const auto& light:lights) {
    const unsigned samples=light.axis_v_type[3]==2?4:1;
    for(unsigned i=0;i<samples;++i) {
      std::array<double,3> delta{};
      for(unsigned a=0;a<3;++a) {
        double p=light.position_range[a];
        if(samples==4)p+=((i&1)? .5:-.5)*light.axis_u_inner[a]+((i&2)? .5:-.5)*light.axis_v_type[a];
        delta[a]=p-Surface[a];
      }
      const double distance=std::hypot(delta[0],delta[1],delta[2]);
      if(distance<.001 || distance>=light.position_range[3])continue;
      for(auto& a:delta)a/=distance;
      double attenuation=std::pow(std::max(1-std::pow(distance/light.position_range[3],4),0.),2)/std::max(distance*distance,.01);
      double facing=0;for(unsigned a=0;a<3;++a)facing-=delta[a]*light.direction_outer[a];
      if(light.axis_v_type[3]==1) {
        const double t=std::clamp((facing-light.direction_outer[3])/(light.axis_u_inner[3]-light.direction_outer[3]),0.,1.);
        attenuation*=t*t*(3-2*t);
      }
      if(samples==4) {
        const auto& u=light.axis_u_inner;const auto& v=light.axis_v_type;
        attenuation*=4*std::hypot(double(u[1]*v[2]-u[2]*v[1]),double(u[2]*v[0]-u[0]*v[2]),double(u[0]*v[1]-u[1]*v[0]))*std::max(facing,0.);
      }
      // Unit white diffuse receiver, metallic=F0=0, view aligned to normal.
      // Small noncollinear rectangle samples have a negligible Fresnel term.
      for(unsigned c=0;c<3;++c)value[c]+=light.color_intensity[c]*light.color_intensity[3]*attenuation*std::max(delta[2],0.)/(std::numbers::pi*samples);
    }
  }
  return value;
}
void compare(const Pixel& actual,const std::array<double,3>& expected,const char* label) {
  for(unsigned c=0;c<3;++c) {
    if(std::abs(actual[c]-expected[c])>std::max(.0002,std::abs(expected[c])*.002))
      std::fprintf(stderr,"direct_oracle case=%s channel=%u actual=%.8f expected=%.8f\n",label,c,actual[c],expected[c]);
    require(std::abs(actual[c]-expected[c])<=std::max(.0002,std::abs(expected[c])*.002),label);
  }
}
WorldLocalLight light(unsigned type=0) {
  WorldLocalLight l;l.position_range={.125f,1.1f,-1,8};l.color_intensity={1,.7f,.3f,20};
  l.direction_outer={0,0,-1,.7f};l.axis_u_inner={.2f,0,0,.9f};l.axis_v_type={0,.2f,0,float(type)};return l;
}
class DirectProbe {
  WorldRenderer& r;
  WorldFrames frames;
  std::array<Texture,4> gbuffer;
  std::array<Slang::ComPtr<rhi::ITexture>,2> retained;
  std::uint64_t serial{};
public:
  explicit DirectProbe(Fixture& f):r(f.renderer) {
    r.width=Size;r.height=Size;r.temporal.mode=0;r.ray_enabled=true;
    require(frames.initialize(r.device,2),"direct probe frame fences");
    const WorldCamera camera{float(Surface[0]),float(Surface[1]),0,0,0,1.05f};r.temporal.camera=camera;
    world_renderer_prepare_draw(r,camera);
    std::array<std::vector<Pixel>,4> values;for(auto& v:values)v.resize(Count);
    values[0][Center]={1,1,1,1};values[1][Center]={0,0,-5,5};values[3][Center]={1,0,0,0};
    for(unsigned i=0;i<4;++i) {
      gbuffer[i]=image(r,values[i]);
      for(auto& target:r.targets)target.hdr.views[i]=gbuffer[i].view;
    }
    require(world_ray_initialize(r) && world_ray_available(r),"direct-light real RT device");
    require(world_local_lighting_initialize(r),"production direct-light initialization");
    r.local_lighting.settings.tile_capacity=4;
    for(auto& output:retained) {
      rhi::TextureDesc d{};d.size={Size,Size,1};d.format=rhi::Format::RGBA16Float;
      d.usage=rhi::TextureUsage::CopyDestination|rhi::TextureUsage::CopySource;d.defaultState=rhi::ResourceState::CopyDestination;
      checked(r.device->createTexture(d,nullptr,output.writeRef()),"direct retained frame texture");
    }
  }
  ~DirectProbe(){frames.drain();r.queue->waitOnHost();}
  void lights(const std::vector<WorldLocalLight>& values) {
    require(open_world_renderer_set_lights(&r,values.data(),unsigned(values.size())),"direct public light update");
  }
  unsigned submit() {
    const unsigned slot=frames.slot(serial++);require(frames.wait(slot),"direct frame slot wait");r.active_frame=slot;r.frames=serial;
    world_block_lights_update(r);
    auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"direct frame encoder");
    require(prepare_player_shadows(r.player,commands,slot,r.player_pose,true),"direct player caster prepare");
    require(world_ray_prepare(r,commands,slot),"direct world ray prepare");
    require(world_local_lighting_prepare(r,commands),"shared direct/indirect light publication");
    require(world_local_lighting_update(r,commands),"production deterministic direct-light update");
    commands->copyTexture(retained[slot],{0,1,0,1},{},r.local_lighting.output,{0,1,0,1},{},{Size,Size,1});
    auto command=commands->finish();require(command!=nullptr,"direct frame finish");
    require(frames.submit(r.queue,command,slot),"direct frame submit");return slot;
  }
  Pixel read(unsigned slot) {
    require(frames.wait(slot),"direct read completion");
    Slang::ComPtr<ISlangBlob> bytes;rhi::SubresourceLayout layout{};
    checked(r.device->readTexture(retained[slot],0,0,bytes.writeRef(),&layout),"direct readback");
    require(bytes && layout.colPitch==8 && layout.rowPitch>=Size*8,"direct output layout");
    Pixel result{};
    for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x)for(unsigned c=0;c<4;++c) {
      std::uint16_t bits{};std::memcpy(&bits,static_cast<const char*>(bytes->getBufferPointer())+y*layout.rowPitch+x*8+c*2,2);
      const float value=half(bits);if(y*Size+x==Center)result[c]=value;
      else if(c<3)require(value==0,"invalid Gbuffer pixels received local light");
    }
    return result;
  }
  Pixel render(){return read(submit());}
  std::array<unsigned,4> counters() {
    std::array<unsigned,4> value{};checked(r.device->readBuffer(r.local_lighting.counters,0,sizeof(value),value.data()),"direct diagnostic counters");return value;
  }
  void settle(unsigned columns) {
    for(unsigned attempt=0;attempt<32;++attempt) {
      (void)render();const auto stats=world_ray_stats(r);
      if(stats.ready_columns==columns && stats.pending_columns==0 && stats.active_jobs==0)return;
    }
    require(false,"direct shadow geometry never settled");
  }
};
unsigned material(const Fixture& f,const char* name) {
  const std::string id=std::string("octaryn.basegame.block.")+name;
  for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].id==id)return i;
  require(false,"direct fixture material absent");return 0;
}
void no_loss_cases(DirectProbe& probe,WorldRenderer& r) {
  probe.lights({});compare(probe.render(),{},"zero lights must clear radiance");
  for(unsigned count:{1u,4u,5u,129u}) {
    std::vector<WorldLocalLight> lights(count,light());
    for(unsigned i=0;i<count;++i)lights[i].color_intensity[3]=1+float(i%7);
    probe.lights(lights);const auto result=probe.render();compare(result,oracle(lights),"sparse/dense direct sum loses a light");
    const auto counters=probe.counters();
    require(counters[1]==1 && counters[2]==count && counters[3]==(count>4?1u:0u),"direct sparse/dense counters disagree with exact contributions");
    for(unsigned frame=0;frame<3;++frame)require(probe.render()==result,"frame index or tile order changed deterministic direct output");
  }
  probe.lights({light()});const auto first=probe.submit();
  auto doubled=light();doubled.color_intensity[3]*=2;probe.lights({doubled});const auto second=probe.submit();
  require(first!=second,"retained direct frames reused one slot");
  compare(probe.read(first),oracle({light()}),"light replacement corrupted retained first frame");
  compare(probe.read(second),oracle({doubled}),"replacement light absent from next frame");
  probe.lights({});compare(probe.render(),{},"light removal left persistent radiance");
  require(!r.local_lighting.active,"empty local lights kept direct system active");
}
void predicted_torch_cases(DirectProbe& probe,Fixture& f) {
  auto& r=f.renderer;
  auto source=column(0,-1,0,32);source.revision=1;source.authoritative_revision=1;
  const auto torch=std::uint16_t(material(f,"white_torch"));
  const auto publish=[&] {
    source.blocks.compact();
    require(open_world_renderer_update(&r,source),"torch authority publication");
  };
  const auto check=[&](bool lit,const char* message) {
    const auto output=probe.render();
    require(r.block_lights.source_count==unsigned(lit) && r.block_lights.selected_count==unsigned(lit) &&
        r.local_lighting.lights.size()==unsigned(lit),message);
    require(r.local_lighting.uploaded_revision==r.local_lighting.light_revision,"GPU light revision lags CPU publication");
    if(lit)require(output[0]>.001f,message);
    else compare(output,{},message);
  };
  const auto predict=[&](unsigned command,std::uint16_t block) {
    require(open_world_renderer_apply_predicted_edit(&r,command,0,1,-2,block),"torch prediction rejected locally");
  };
  publish();check(false,"initial air retained lights");
  predict(1,torch);check(true,"predicted placement missing light");
  open_world_renderer_resolve_predicted_edit(&r,1,false,0);
  check(false,"rejected placement retained light");
  predict(2,torch);open_world_renderer_resolve_predicted_edit(&r,2,true,2);
  world_renderer_publish_column_metadata(r,source);
  check(true,"older authority watermark retired accepted placement");
  put(source,0,1,30,torch);source.revision=2;source.authoritative_revision=2;publish();probe.settle(1);
  check(true,"accepted placement lost light on authority rebase");
  require(r.predicted_edits.edits().empty() && r.prediction_bases.empty(),"accepted placement overlay not retired");
  predict(3,0);check(false,"predicted removal retained light");
  open_world_renderer_resolve_predicted_edit(&r,3,false,0);
  check(true,"rejected removal did not restore light");
  predict(4,0);open_world_renderer_resolve_predicted_edit(&r,4,true,3);
  world_renderer_publish_column_metadata(r,source);
  check(false,"older authority watermark resurrected removed torch");
  put(source,0,1,30,0);source.revision=3;source.authoritative_revision=3;publish();probe.settle(0);
  check(false,"accepted removal retained authoritative light");
  // A server snapshot can coalesce a placement/removal back to identical air.
  predict(5,torch);predict(6,0);
  open_world_renderer_resolve_predicted_edit(&r,5,true,4);
  open_world_renderer_resolve_predicted_edit(&r,6,true,5);
  source.authoritative_revision=5;
  require(world_renderer_same_authoritative_content(r,source),"same-content acknowledgement missed metadata path");
  world_renderer_publish_column_metadata(r,source);
  check(false,"coalesced metadata acknowledgements retained torch");
  require(r.predicted_edits.edits().empty() && r.prediction_bases.empty(),"same-content watermark retained predictions");
  predict(7,torch);check(true,"reset fixture missing prediction light");
  open_world_renderer_reset_predictions(&r);check(false,"prediction reset retained source");
  // Publication may arrive before its command acknowledgement.
  predict(8,torch);put(source,0,1,30,torch);source.revision=4;source.authoritative_revision=6;publish();
  open_world_renderer_resolve_predicted_edit(&r,8,true,6);check(true,"publication-before-ack lost torch");
  predict(9,0);put(source,0,1,30,0);source.revision=5;source.authoritative_revision=7;publish();
  open_world_renderer_resolve_predicted_edit(&r,9,true,7);check(false,"publication-before-ack retained removed torch");
  require(r.predicted_edits.edits().empty() && r.prediction_bases.empty(),"late acknowledgements retained predictions");
  open_world_renderer_set_center(&r,40,40,0);check(false,"torch lifecycle eviction retained light");
  open_world_renderer_set_center(&r,0,0,4);
  std::puts("predicted_torch_lights=passed add=1 remove=1 rejected_place=1 rejected_remove=1 accepted_revision=1 stale_watermark=1 same_content_watermark=1 publication_before_ack=1 reset=1 gpu_empty_output=zero");
}
void shadow_cases(DirectProbe& probe,Fixture& f) {
  auto& r=f.renderer;
  for(unsigned type:{0u,1u,2u}) {auto l=light(type);probe.lights({l});compare(probe.render(),oracle({l}),"point/spot/rectangle radiance oracle");}
  auto away=light(1);away.direction_outer={0,0,1,.7f};probe.lights({away});compare(probe.render(),{},"spotlight illuminates outside cone");
  auto source=column(0,-1,0,32);put(source,0,1,29,std::uint16_t(material(f,"stone")));
  require(open_world_renderer_update(&r,source),"direct blocker publication");probe.lights({light()});probe.settle(1);
  for(unsigned type:{0u,1u,2u}) {probe.lights({light(type)});compare(probe.render(),{},"voxel blocker failed direct point/spot/area shadow");}
  put(source,0,1,29,0);++source.revision;require(open_world_renderer_update(&r,source),"direct blocker removal");probe.settle(0);
  compare(probe.render(),oracle({light(2)}),"edited wall removal did not restore rectangle light");
  char asset[4096]{};require(bundle_path_build(asset,sizeof(asset),"Client/Assets/Player/octaryn_player_v1.gltf"),"direct player asset");
  const auto shader=resolve_slang_shader_path("octaryn-client/Shaders/Player/Player.slang");
  r.player=create_player_renderer(r.device,rhi::Format::RGBA16Float,rhi::Format::D32Float,asset,shader.c_str());require(r.player!=nullptr,"direct player create");
  r.player_pose={};r.player_pose.feet_z=-3.5f;r.player_pose.first_person=true;
  for(unsigned type:{0u,1u,2u}) {probe.lights({light(type)});compare(probe.render(),{},"first-person body failed direct point/spot/area shadow");}
  r.player_pose.feet_x=4;compare(probe.render(),oracle({light(2)}),"moving player left stale direct shadow");
  checked(r.queue->waitOnHost(),"direct player cleanup wait");destroy_player_renderer(r.player);r.player=nullptr;
  probe.lights({});put(source,0,1,30,std::uint16_t(material(f,"lava")));++source.revision;
  require(open_world_renderer_update(&r,source),"resident lava light publication");probe.settle(1);
  require(r.local_lighting.lights.size()==1 && r.local_lighting.lights[0].axis_v_type[3]==3,"voxel emitter did not enter production direct list");
  require(probe.render()[0]>.001f,"voxel emitter incorrectly shadows its own source voxel");
  put(source,0,1,29,std::uint16_t(material(f,"stone")));++source.revision;require(open_world_renderer_update(&r,source),"adjacent voxel source blocker");probe.settle(1);
  compare(probe.render(),{},"voxel source exclusion skipped adjacent opaque blocker");
  open_world_renderer_set_center(&r,40,40,0);probe.settle(0);compare(probe.render(),{},"unloaded resident emitter left direct light");
}
}
void direct_lighting_cases(Fixture& f) {
  DirectProbe probe(f);no_loss_cases(probe,f.renderer);predicted_torch_cases(probe,f);shadow_cases(probe,f);
  require(f.renderer.debug.errors.load()==0,"direct-light GPU validation errors");
  std::puts("direct_lighting=passed production_owner=1 production_shaders=1 sparse=1 exact_capacity=1 overflow=129 no_dropped_lights=1 repeat_frames=3 point=1 spot=1 rectangle_samples=4 block_source_shadow=1 player_shadow=1 retained_frames=2 removal=1 invalid_surface=1");
}
}
