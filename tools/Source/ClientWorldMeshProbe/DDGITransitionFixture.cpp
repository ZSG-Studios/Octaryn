#include "DDGITransitionFixture.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <thread>

namespace mesh_probe::transition {
using Pixel=std::array<float,4>;
unsigned material(const mesh_probe::Fixture& f,const char* name) {
  const std::string wanted=std::string("octaryn.basegame.block.")+name;
  for(unsigned i=0;i<f.catalog.size();++i)if(f.catalog[i].id==wanted)return i;
  require(false,"DDGI transition material missing");return 0;
}
Fixture::Fixture(mesh_probe::Fixture& fixture):f(fixture),r(f.renderer) {
  directory="logs/client/ddgi-transition";
  if(const char* path=SDL_getenv("OCTARYN_DDGI_TRANSITION_OUTPUT"))directory=path;
  std::filesystem::create_directories(directory);
  backend=r.device->getDeviceType()==rhi::DeviceType::D3D12?"d3d12":"vulkan";
  csv.open(directory/(backend+"-samples.csv"));receiver_csv.open(directory/(backend+"-receivers.csv"));
  require(bool(csv)&&bool(receiver_csv),"DDGI transition evidence files");
  csv<<"phase,elapsed_seconds,total_seconds,frame,slot,day,source,hierarchy_energy,fine_receiver_energy,coarse_receiver_energy";
  for(const char* volume:{"fine","coarse"})for(const char* name:{"raw_energy","published_energy","valid","refreshed","control_mismatches","pending_removal","minimum_trace","oldest_trace_seconds","pending","oldest_seconds","updated"})csv<<','<<volume<<'_'<<name;
  csv<<'\n';receiver_csv<<"phase,elapsed_seconds,total_seconds,day,source,volume,receiver,red,green,blue,coverage\n";
  scene(.5);
  require(world_ray_initialize(r)&&world_ray_available(r),"DDGI transition production ray scene required");
  require(world_local_lighting_initialize(r)&&world_ddgi_initialize(r),"DDGI transition production lighting initialization");
  require(open_world_renderer_set_ddgi_range(&r,16,128),"DDGI transition radii");
  require(r.ddgi.available&&r.ddgi.fine_volume&&r.ddgi.fine_volume->available,"DDGI transition both volumes required");
  require(create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGITransitionProbe.slang","main",pipeline),"DDGI transition production sampling shader");
  std::array<Pixel,Receivers> p{},n{};
  for(unsigned i=0;i<4;++i) {
    const float x=13.f+float(i)*2;
    p[i]={x,2.01f,16,0};n[i]={0,1,0,0};
    p[i+4]={11.01f,4.f,x,0};n[i+4]={1,0,0,0};
    p[i+8]={20.99f,4.f,x,0};n[i+8]={-1,0,0,0};
  }
  positions=buffer(r,p.data(),sizeof(p),sizeof(Pixel),rhi::BufferUsage::ShaderResource);
  normals=buffer(r,n.data(),sizeof(n),sizeof(Pixel),rhi::BufferUsage::ShaderResource);
  const std::array<Pixel,Receivers*3> zero{};
  results=buffer(r,zero.data(),sizeof(zero),sizeof(Pixel),rhi::BufferUsage::UnorderedAccess);
}
void Fixture::scene(double day) {
  auto settings=lighting_settings_default_value();settings.sun_strength=.75f;settings.ambient_strength=.65f;
  open_world_renderer_set_lighting(&r,settings);
  WorldSceneSettings scene;scene.day_fraction=day;scene.clouds=false;scene.fog=false;
  open_world_renderer_set_scene(&r,scene);
}
void Fixture::publish(StreamColumn& source) {
  source.blocks.compact();++source.revision;
  require(open_world_renderer_update(&r,source),"DDGI transition production geometry publication");
}
std::array<std::uint64_t,2> Fixture::frames() const {return {r.ddgi.fine_volume->frame,r.ddgi.frame};}
void Fixture::frame() {
  const auto start=Clock::now();const unsigned slot=r.frame_queue.slot(serial++);
  require(r.frame_queue.wait(slot,10000),"DDGI transition frame wait");r.active_frame=slot;r.frames=serial;
  const WorldCamera camera{16,5,16,0,0,1.05f};r.temporal.camera=camera;
  world_renderer_prepare_draw(r,camera);world_block_lights_update(r);
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"DDGI transition encoder");
  require(world_ray_prepare(r,commands,slot),"DDGI transition production acceleration structures");
  require(world_local_lighting_prepare(r,commands),"DDGI transition production light upload");
  require(world_ddgi_update(r,commands),"DDGI transition production scheduling");
  auto submission=commands->finish();require(submission!=nullptr,"DDGI transition command finish");
  require(r.frame_queue.submit(r.queue,submission,slot),"DDGI transition submit");
  require(r.frame_queue.wait(slot,10000),"DDGI transition GPU completion");
  const auto current=frames();
  for(unsigned i=0;i<2;++i) {completion[i].resize(current[i]+1);completion[i][current[i]]=seconds(epoch);}
  // Pacing is a ceiling; every age, step and phase uses the monotonic clock.
  std::this_thread::sleep_until(start+std::chrono::microseconds(16667));
}
Cohort Fixture::read_cohort(DDGISystem& s,unsigned volume,std::uint64_t phase_frame) {
  const auto inside=[&](const DDGIControl& c) {
    if(volume==0)return c.cell[0]>=14&&c.cell[0]<18&&c.cell[1]>=3&&c.cell[1]<6&&c.cell[2]>=14&&c.cell[2]<18;
    return c.cell[0]>=0&&c.cell[0]<4&&c.cell[1]>=1&&c.cell[1]<4&&c.cell[2]>=0&&c.cell[2]<4;
  };
  unsigned begin=unsigned(s.control_data.size()),end=0;
  for(unsigned i=0;i<s.control_data.size();++i)if(inside(s.control_data[i])) {begin=std::min(begin,i);end=std::max(end,i+1);}
  require(begin<end,"DDGI transition fixed cohort absent");
  std::vector<DDGIControl> controls(end-begin);std::vector<DDGIProbe> probes(end-begin);
  const unsigned texels=s.config.irradiance_resolution*s.config.irradiance_resolution;
  std::vector<Pixel> irradiance(std::size_t(end-begin)*texels);
  checked(r.device->readBuffer(s.controls,std::size_t(begin)*sizeof(DDGIControl),controls.size()*sizeof(DDGIControl),controls.data()),"DDGI transition GPU controls");
  checked(r.device->readBuffer(s.probes,std::size_t(begin)*sizeof(DDGIProbe),probes.size()*sizeof(DDGIProbe),probes.data()),"DDGI transition GPU probes");
  checked(r.device->readBuffer(s.irradiance,std::size_t(begin)*texels*sizeof(Pixel),irradiance.size()*sizeof(Pixel),irradiance.data()),"DDGI transition GPU irradiance");
  Cohort out;out.minimum_trace=std::numeric_limits<unsigned>::max();unsigned count=0;
  for(unsigned i=0;i<probes.size();++i)if(inside(s.control_data[begin+i])) {
    ++count;const auto& c=controls[i];const auto& p=probes[i];
    out.control_mismatches+=std::memcmp(&c,&s.control_data[begin+i],sizeof(c))!=0;
    const bool valid=p.metadata[0]==c.version&&p.metadata[3]!=0&&p.offset[3]!=1&&c.padding[0]==0&&c.padding[1]==0;
    out.valid+=valid;out.refreshed+=valid&&p.metadata[1]>phase_frame;
    out.pending_removal+=(c.padding[2]&3u)==2&&c.refresh_frame>p.metadata[1];
    require(p.metadata[1]<completion[volume].size(),"DDGI transition future GPU trace frame");
    out.minimum_trace=std::min(out.minimum_trace,p.metadata[1]);
    out.oldest_trace_seconds=std::max(out.oldest_trace_seconds,seconds(epoch)-completion[volume][p.metadata[1]]);
    for(unsigned t=0;t<texels;++t)for(unsigned channel=0;channel<3;++channel) {
      const float value=irradiance[i*texels+t][channel];require(std::isfinite(value)&&value>=0,"DDGI transition invalid GPU irradiance");
      out.raw[channel]+=value/double(CohortSize*texels);
      if(valid)out.published[channel]+=value/double(CohortSize*texels);
    }
  }
  require(count==CohortSize,"DDGI transition cohort changed");return out;
}
Sample Fixture::read(const std::string& name,double elapsed,double day,bool source,const std::array<std::uint64_t,2>& phase_frames) {
  Sample out;out.elapsed=elapsed;out.total_seconds=seconds(epoch);out.day=day;out.source=source;
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"DDGI transition sample encoder");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"DDGI transition sample pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"DDGI transition sample pipeline");
  require(world_ddgi_bind(r,root),"DDGI transition production resource bindings");
  rhi::ShaderCursor cursor(root);
  checked(cursor["positions"].setBinding(rhi::Binding(positions)),"DDGI transition positions");
  checked(cursor["normals"].setBinding(rhi::Binding(normals)),"DDGI transition normals");
  checked(cursor["results"].setBinding(rhi::Binding(results)),"DDGI transition results");
  pass->dispatchCompute(Receivers,1,1);pass->end();auto submission=commands->finish();
  checked(r.queue->submit(submission),"DDGI transition sample submit");checked(r.queue->waitOnHost(),"DDGI transition sample completion");
  std::array<Pixel,Receivers*3> pixels{};
  checked(r.device->readBuffer(results,0,sizeof(pixels),pixels.data()),"DDGI transition production receiver readback");
  for(unsigned v=0;v<3;++v)for(unsigned i=0;i<Receivers;++i) {
    const auto& pixel=pixels[v*Receivers+i];
    for(float c:pixel)require(std::isfinite(c)&&c>=0,"DDGI transition invalid GPU receiver");
    out.minimum_coverage[v]=std::min(out.minimum_coverage[v],double(pixel[3]));
    receiver_csv<<name<<','<<elapsed<<','<<out.total_seconds<<','<<day<<','<<source<<','<<v<<','<<i;
    for(unsigned c=0;c<3;++c) {out.pixels[v][i][c]=pixel[c];out.receiver[v][c]+=pixel[c]/double(Receivers);receiver_csv<<','<<pixel[c];}
    receiver_csv<<','<<pixel[3]<<'\n';
  }
  csv<<name<<','<<elapsed<<','<<out.total_seconds<<','<<serial<<','<<r.active_frame<<','<<day<<','<<source;
  for(const auto& rgb:out.receiver)csv<<','<<energy(rgb);
  const std::array<DDGISystem*,2> volumes{r.ddgi.fine_volume.get(),&r.ddgi};
  for(unsigned v=0;v<2;++v) {
    const auto& c=out.cohort[v]=read_cohort(*volumes[v],v,phase_frames[v]);const auto& s=volumes[v]->stats;
    csv<<','<<energy(c.raw)<<','<<energy(c.published)<<','<<c.valid<<','<<c.refreshed<<','<<c.control_mismatches<<','<<c.pending_removal<<','<<c.minimum_trace<<','<<c.oldest_trace_seconds<<','<<s.pending_probes<<','<<s.oldest_update_seconds<<','<<s.updated_probes;
  }
  csv<<'\n';csv.flush();receiver_csv.flush();return out;
}
void Fixture::check() const {
  const auto rays=world_ray_stats(r);
  require(rays.ready_columns==1&&rays.pending_columns==0&&rays.active_jobs==0,"DDGI transition unfinished production geometry");
  require(r.debug.errors.load()==0,"DDGI transition graphics validation errors");
}
}
