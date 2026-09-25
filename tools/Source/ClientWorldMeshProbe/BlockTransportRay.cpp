#include "BlockTransportSetup.h"
#include "BlockTransportWork.h"
#include "AtlasInternal.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <thread>

namespace mesh_probe {
void block_transport_plant_cases(Fixture&);
namespace {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
constexpr unsigned Slots=16,Capacity=32,Epoch=41,Cycle=7;
constexpr BlockSurfaceKey Receiver{16,0,16,3};
struct RaySample {Pixel origin,direction;};
struct Expected {BlockSurfaceKey key{};float distance{1e30f};bool hit{};};
struct MaterialExpected {unsigned material;bool valid,back_face;BlockSurfaceKey key;float distance;};
struct MaterialResult {Words identity;BlockSurfaceKey key;Pixel properties;Words admission;};
static_assert(sizeof(MaterialResult)==64);
Expected intersect_room(const RaySample& ray,bool roof) {
  Expected result;
  for(int z=8;z<=24;++z)for(int y=0;y<=16;++y)for(int x=8;x<=24;++x) {
    if(!(y==0 || (roof && y==16) || x==8 || x==24 || z==8 || z==24))continue;
    const int low[3]={x,y,z};double enter=0,leave=1e30;unsigned face=6;
    for(unsigned axis=0;axis<3;++axis) {
      const double origin=ray.origin[axis],direction=ray.direction[axis];
      if(std::abs(direction)<1e-10) {
        if(origin<double(low[axis]) || origin>double(low[axis]+1)){leave=-1;break;}
        continue;
      }
      double first=(double(low[axis])-origin)/direction;
      double last=(double(low[axis]+1)-origin)/direction;
      const unsigned incoming=axis*2+(direction<0?1u:0u);
      if(first>last)std::swap(first,last);
      if(first>enter){enter=first;face=incoming;}
      leave=std::min(leave,last);
    }
    if(face<6 && enter>.0001 && leave>=enter && enter<result.distance) {
      result={BlockSurfaceKey{x,y,z,face},float(enter),true};
    }
  }
  return result;
}
class RayProbe {
  WorldRenderer& r;
  Slang::ComPtr<rhi::IComputePipeline> trace,sample;
  std::ofstream heartbeat{"frame-timing.csv",std::ios::app};
  std::chrono::steady_clock::time_point previous=std::chrono::steady_clock::now();
public:
  explicit RayProbe(WorldRenderer& renderer):r(renderer) {
    r.ray_enabled=true;r.gi_mode=GiMode::BlockTransport;
    require(world_ray_available(r),"BTGI prepared actual RT initialization");
    require(prepare_world_atlas_plant_masks(r.atlas),"BTGI standalone trace plant masks");
    require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Trace.slang",
        "main",trace),"BTGI production transport pipeline");
    const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportRay.slang").generic_string();
    require(block_transport_pipeline(r.device,path.c_str(),"main",sample),"BTGI sample readback pipeline");
    previous=std::chrono::steady_clock::now();
  }
  void cap() {
    std::this_thread::sleep_until(previous+std::chrono::milliseconds(34));
    const auto now=std::chrono::steady_clock::now();
    heartbeat<<r.frames++<<','<<std::chrono::duration<double,std::milli>(now-previous).count()<<'\n';
    heartbeat.flush();require(bool(heartbeat),"BTGI RT heartbeat write");previous=now;
  }
  void settle() {
    for(unsigned attempt=0;attempt<64;++attempt) {
      r.active_frame=r.frame_queue.slot(r.frames);
      require(r.frame_queue.wait(r.active_frame,2000),"BTGI RT frame reuse fence");
      auto commands=r.queue->createCommandEncoder();require(bool(commands),"BTGI RT scene commands");
      require(world_ray_prepare(r,commands,r.active_frame),"BTGI RT scene preparation");
      auto submission=commands->finish();require(bool(submission),"BTGI RT scene finish");
      require(r.frame_queue.submit(r.queue,submission,r.active_frame) &&
          r.frame_queue.wait(r.active_frame,2000),"BTGI RT scene bounded fence");
      const bool ready=world_ray_coverage_complete(r);cap();if(ready)return;
    }
    require(false,"BTGI RT geometry did not converge under bounded attempts");
  }
  void run(bool roof,bool coverage) {
    std::array<BlockTransportSurface,Capacity> initial{};
    initial[0]={Receiver,{.5f,.5f,.5f,0},{Epoch,Cycle,UINT32_MAX,0},{}};
    std::array<BlockTransportLink,Capacity*Slots> no_links{};
    std::array<BlockTransportCandidate,Slots> no_candidates{};
    std::array<unsigned,12> zero{};std::array<RaySample,Slots> ray_samples{};
    const auto rw=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess;
    const auto surfaces=buffer(r,initial.data(),sizeof(initial),sizeof(BlockTransportSurface),rw);
    const auto links=buffer(r,no_links.data(),sizeof(no_links),sizeof(BlockTransportLink),rw);
    const auto candidates=buffer(r,no_candidates.data(),sizeof(no_candidates),sizeof(BlockTransportCandidate),rw);
    const auto counters=buffer(r,zero.data(),sizeof(zero),sizeof(unsigned),rw);
    const auto samples=buffer(r,ray_samples.data(),sizeof(ray_samples),sizeof(Pixel),rw);
    std::array<Pixel,Capacity> no_radiance{};
    const auto radiance=buffer(r,no_radiance.data(),sizeof(no_radiance),sizeof(Pixel),rhi::BufferUsage::ShaderResource);
    BlockTransportWork selection(r,Capacity,1);
    const Words frame{Epoch,Cycle,Capacity,Slots},work{0,1,Epoch,0};
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BTGI transport commands");
    selection.encode(commands,surfaces,radiance,radiance,radiance,frame,Epoch);
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"BTGI sample pass");
    auto* root=pass->bindPipeline(sample);require(root!=nullptr,"BTGI sample pipeline bind");
    require(bind_world_atlas(r.atlas,root),"BTGI sample plant mask bindings");
    rhi::ShaderCursor cursor(root);const Words sample_info{Cycle,0,0,0};
    checked(cursor["probeKey"].setData(&Receiver,sizeof(Receiver)),"BTGI sample receiver key");
    checked(cursor["probeSampleInfo"].setData(sample_info.data(),sizeof(sample_info)),"BTGI sample epoch");
    checked(cursor["probeSamples"].setBinding(samples),"BTGI sample output");
    pass->dispatchCompute(1,1,1);pass->end();
    pass=commands->beginComputePass();require(pass!=nullptr,"BTGI transport pass");
    root=pass->bindPipeline(trace);require(root!=nullptr,"BTGI transport pipeline bind");
    require(world_ray_bind(r,root) && bind_world_atlas(r.atlas,root),"BTGI exact scene and atlas bindings");
    cursor=rhi::ShaderCursor(root);
    selection.bind(root);
    const auto bind=[&](const char* field,rhi::IBuffer* value) {
      checked(cursor[field].setBinding(value),field);
    };
    bind("btSurfaces",surfaces);bind("btLinks",links);bind("btCandidates",candidates);bind("btCounters",counters);
    const Pixel minimum{0,0,0,coverage?1.f:0.f},maximum{32,32,32,0},ray_settings{1,64,.002f,0};
    checked(cursor["btFrameInfo"].setData(frame.data(),sizeof(frame)),"BTGI transport frame");
    checked(cursor["btWork"].setData(work.data(),sizeof(work)),"BTGI transport budget");
    const unsigned player_revision=0;
    if(cursor["btPlayerRevision"].isValid())checked(cursor["btPlayerRevision"].setData(&player_revision,sizeof(player_revision)),"BTGI trace player revision");
    checked(cursor["btCoverageMin"].setData(minimum.data(),sizeof(minimum)),"BTGI certified bounds minimum");
    checked(cursor["btCoverageMax"].setData(maximum.data(),sizeof(maximum)),"BTGI certified bounds maximum");
    checked(cursor["raySettings"].setData(ray_settings.data(),sizeof(ray_settings)),"BTGI controlled ray range");
    pass->dispatchCompute(1,1,1);pass->end();
    auto submission=commands->finish();require(bool(submission),"BTGI transport finish");
    require(r.frame_queue.submit(r.queue,submission,r.active_frame) &&
        r.frame_queue.wait(r.active_frame,2000),"BTGI transport bounded fence");
    checked(r.device->readBuffer(samples,0,sizeof(ray_samples),ray_samples.data()),"BTGI sample readback");
    checked(r.device->readBuffer(links,0,sizeof(no_links),no_links.data()),"BTGI link readback");
    checked(r.device->readBuffer(candidates,0,sizeof(no_candidates),no_candidates.data()),"BTGI contributor readback");
    checked(r.device->readBuffer(counters,0,sizeof(zero),zero.data()),"BTGI ray-budget readback");
    const auto header=selection.header();
    require(header[2]==1 && header[3]==1 && header[4]==0,"BTGI trace did not receive the unique uninitialized occupied row");
    unsigned hit_count=0,sky_count=0,unknown_count=0;
    for(unsigned slot=0;slot<Slots;++slot) {
      const auto& ray=ray_samples[slot];
      const auto expected=intersect_room(ray,roof);
      require(ray.direction[1]>0 && std::abs(ray.direction[0]*ray.direction[0]+
          ray.direction[1]*ray.direction[1]+ray.direction[2]*ray.direction[2]-1)<1e-5f,
          "BTGI samples left receiver hemisphere");
      if(!coverage) {
        require(no_links[slot].terminal==0 && no_candidates[slot].key.direction==UINT32_MAX,
            "BTGI uncertified receiver generated transport or sky");
        ++unknown_count;continue;
      }
      if(expected.hit && expected.distance<=32) {
        require(no_links[slot].terminal==3 && no_links[slot].target_slot==slot,
            "BTGI exact first blocker was lost before contributor admission");
        require(no_candidates[slot].key==expected.key,"BTGI hit key differs from independent unit-block ray oracle");
        ++hit_count;
      } else {
        const float distance=(32-ray.origin[1])/ray.direction[1];
        const float x=ray.origin[0]+ray.direction[0]*distance,z=ray.origin[2]+ray.direction[2]*distance;
        const bool sky=coverage && distance>0 && distance+.002f<=64 && x>=0 && x<=32 && z>=0 && z<=32;
        require(no_links[slot].terminal==(sky?2u:0u),"BTGI finite or uncovered miss was incorrectly classified as sky");
        if(sky)++sky_count;else ++unknown_count;
      }
    }
    require(zero[6]==(coverage?Slots:0u),"BTGI trace exceeded or underused explicit ray-slot budget");
    if(roof)require(hit_count==Slots && sky_count==0,"BTGI sealed room did not block every hemisphere sample");
    else if(coverage)require(sky_count>0,"BTGI roof removal fixture did not expose certified sky");
    else require(sky_count==0 && unknown_count>0,"BTGI unknown coverage fixture lacked a conservative miss");
    std::printf("block_transport_rt_case roof=%u coverage=%u hits=%u sky=%u unknown=%u production_trace=1 status=passed\n",
        roof?1u:0u,coverage?1u:0u,hit_count,sky_count,unknown_count);
    cap();
  }

  void materials(const std::vector<RaySample>& rays,const std::vector<MaterialExpected>& expected,unsigned leaves) {
    require(rays.size()==expected.size(),"BTGI material oracle size");
    Slang::ComPtr<rhi::IComputePipeline> pipeline;
    const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportRay.slang").generic_string();
    require(block_transport_pipeline(r.device,path.c_str(),"material_main",pipeline),"BTGI material ray pipeline");
    const auto rw=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess;
    std::array<BlockTransportSurface,Capacity> empty{};std::array<unsigned,12> zero{};
    const auto surfaces=buffer(r,empty.data(),sizeof(empty),sizeof(BlockTransportSurface),rw);
    const auto counters=buffer(r,zero.data(),sizeof(zero),sizeof(unsigned),rw);
    const auto inputs=buffer(r,rays.data(),rays.size()*sizeof(RaySample),sizeof(RaySample),rhi::BufferUsage::ShaderResource);
    std::vector<MaterialResult> actual(rays.size());
    const auto results=buffer(r,actual.data(),actual.size()*sizeof(MaterialResult),sizeof(MaterialResult),rw);
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BTGI material commands");
    // Two distinct keys may need separate claim rounds; read both after publication.
    for(unsigned repeat=0;repeat<3;++repeat) {
      auto* pass=commands->beginComputePass();require(pass!=nullptr,"BTGI material pass");
      auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BTGI material pipeline bind");
      require(world_ray_bind(r,root) && bind_world_atlas(r.atlas,root),"BTGI material scene and atlas");
      const rhi::ShaderCursor cursor(root);
      checked(cursor["probeMaterialRays"].setBinding(inputs),"BTGI material ray input");
      checked(cursor["probeMaterialResults"].setBinding(results),"BTGI material ray results");
      checked(cursor["btSurfaces"].setBinding(surfaces),"BTGI material surface cache");
      checked(cursor["btCounters"].setBinding(counters),"BTGI material admission counters");
      const Words frame{Epoch,Cycle,Capacity,Slots},info{static_cast<unsigned>(rays.size()),0,0,0};
      const Pixel settings{1,16,.002f,0};
      checked(cursor["btFrameInfo"].setData(frame.data(),sizeof(frame)),"BTGI material geometry epoch");
      checked(cursor["probeMaterialInfo"].setData(info.data(),sizeof(info)),"BTGI material ray count");
      checked(cursor["raySettings"].setData(settings.data(),sizeof(settings)),"BTGI material alpha mip");
      pass->dispatchCompute((static_cast<unsigned>(rays.size())+63)/64,1,1);pass->end();commands->globalBarrier();
    }
    auto submission=commands->finish();require(bool(submission),"BTGI material finish");
    require(r.frame_queue.submit(r.queue,submission,r.active_frame) &&
        r.frame_queue.wait(r.active_frame,2000),"BTGI material bounded completion");
    checked(r.device->readBuffer(results,0,actual.size()*sizeof(MaterialResult),actual.data()),"BTGI material readback");
    unsigned fronts=0,holes=0,backs=0,transmission=0;
    for(std::size_t i=0;i<actual.size();++i) {
      const auto& a=actual[i];const auto& e=expected[i];
      require(a.identity[0]==1 && a.identity[1]==e.material && a.identity[3]==(e.valid?1u:0u),
          "BTGI leaf alpha or transmission first-hit classification differs from PNG oracle");
      require(std::abs(a.properties[0]-e.distance)<1e-5f && (a.properties[2]!=0)==e.back_face,
          "BTGI material fixture hit the wrong face plane or sidedness");
      if(e.valid)require(a.key==e.key && a.admission[0]!=UINT32_MAX && a.admission[1]==a.admission[0],
          "BTGI supported cutout cube failed exact-key admission");
      else require(a.key.direction==UINT32_MAX && a.admission[0]==UINT32_MAX,
          "BTGI unsupported transmission or back face entered the diffuse cache");
      if(e.material==leaves) {
        require(a.identity[2]==(16u|256u|1024u) && a.properties[1]>=.35f,
            "BTGI leaf fixture lost non-occluding opaque catalog flags or alpha coverage");
        if(e.back_face)++backs;else ++fronts;
      } else if(e.valid)++holes;
      else {require((a.identity[2]&(1u<<30))!=0,"BTGI glass/water became an unclassified miss");++transmission;}
    }
    require(fronts>0 && holes>0 && transmission==2,"BTGI material fixture lacked leaf coverage, holes or blockers");
    std::printf("block_transport_leaves_rt=passed png_oracle=1 opaque_fronts=%u transparent_holes=%u unsupported_back_faces=%u glass_water_blockers=%u exact_key_admission=1\n",
        fronts,holes,backs,transmission);
    cap();
  }
};
}
void block_transport_ray_cases(Fixture& fixture) {
  auto& r=fixture.renderer;RayProbe probe(r);open_world_renderer_set_center(&r,0,0,0);
  unsigned stone=0;
  for(unsigned i=1;i<fixture.catalog.size();++i)if(fixture.catalog[i].id=="octaryn.basegame.block.stone")stone=i;
  require(stone!=0,"BTGI RT fixture stone material");
  auto room=column();
  for(int z=8;z<=24;++z)for(int y=0;y<=16;++y)for(int x=8;x<=24;++x)
    if(y==0 || y==16 || x==8 || x==24 || z==8 || z==24)
      put(room,x,y,z,static_cast<std::uint16_t>(stone));
  const auto publish=[&] {
    room.blocks.compact();++room.revision;
    require(open_world_renderer_update(&r,room),"BTGI RT fixture publication");probe.settle();
  };
  publish();probe.run(true,true);
  for(int z=9;z<24;++z)for(int x=9;x<24;++x)put(room,x,16,z,0);
  publish();probe.run(false,true);probe.run(false,false);
  for(int z=9;z<24;++z)for(int x=9;x<24;++x)put(room,x,16,z,static_cast<std::uint16_t>(stone));
  publish();probe.run(true,true);

  unsigned leaves=0,glass=0,water=0;
  for(unsigned i=1;i<fixture.catalog.size();++i) {
    const auto& id=fixture.catalog[i].id;
    if(id=="octaryn.basegame.block.leaves")leaves=i;
    if(id=="octaryn.basegame.block.glass")glass=i;
    if(id=="octaryn.basegame.block.water")water=i;
  }
  require(leaves && glass && water,"BTGI material fixtures missing catalog blocks");
  require(r.atlas->material_flags[leaves]==(16u|256u|1024u),"BTGI leaves catalog eligibility changed");
  std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> atlas(load_atlas_rgba("Atlases/basegame-color.png"),SDL_DestroySurface);
  require(atlas && atlas->w==32*29 && atlas->h==32,"BTGI independent leaf alpha atlas missing");
  const unsigned layer=world_atlas_preview_layer(r.atlas,leaves);
  auto materials=column();materials.revision=room.revision+1;
  for(const auto pair:{std::pair{8,leaves},std::pair{12,glass},std::pair{16,water}}) {
    put(materials,pair.first,8,8,static_cast<std::uint16_t>(pair.second));
    put(materials,pair.first,8,6,static_cast<std::uint16_t>(stone));
  }
  std::vector<RaySample> rays;std::vector<MaterialExpected> expected;
  for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x) {
    const auto* row=static_cast<const Uint8*>(atlas->pixels)+y*static_cast<unsigned>(atlas->pitch);
    const bool front=row[(layer*32+x)*4+3]>=.35f*255;
    const bool back=row[(layer*32+31-x)*4+3]>=.35f*255;
    rays.push_back({{8+(float(x)+.5f)/32,9-(float(y)+.5f)/32,12,10},{0,0,-1,0}});
    if(front)expected.push_back({leaves,true,false,{8,8,8,5},3});
    else if(back)expected.push_back({leaves,false,true,{0,0,0,UINT32_MAX},4});
    else expected.push_back({stone,true,false,{8,8,6,5},5});
  }
  for(const auto pair:{std::pair{12,glass},std::pair{16,water}}) {
    rays.push_back({{float(pair.first)+.5f,8.125f,12,10},{0,0,-1,0}});
    expected.push_back({pair.second,false,false,{0,0,0,UINT32_MAX},3});
  }
  materials.blocks.compact();require(open_world_renderer_update(&r,materials),"BTGI material fixture publication");
  probe.settle();probe.materials(rays,expected,leaves);
  require(r.debug.errors.load()==0,"BTGI transport graphics validation errors");
  std::puts("block_transport_rt=passed hardware=1 production_trace=1 exact_block_oracle=1 roof_edits=1 sealed_dark=1 unknown_not_sky=1 validation_errors=0");
}
}
