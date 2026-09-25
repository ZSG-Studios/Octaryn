#include "BlockTransportSetup.h"
#include "BlockTransportContributorAdmission.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <chrono>
#include <thread>

namespace mesh_probe {
namespace {
using Words=std::array<unsigned,4>;
constexpr unsigned Capacity=32,Epoch=31;
std::vector<Words> run(WorldRenderer& r,rhi::IComputePipeline* pipeline,
    const std::vector<BlockTransportCandidate>& candidates,rhi::IBuffer* surfaces,
    unsigned epoch,unsigned mode,unsigned frame_number=7,bool visible=false,rhi::IBuffer* shared_counters=nullptr,
    unsigned allocation_limit=UINT32_MAX) {
  const auto start=std::chrono::steady_clock::now();
  const auto inputs=buffer(r,candidates.data(),candidates.size()*sizeof(BlockTransportCandidate),
      sizeof(BlockTransportCandidate),rhi::BufferUsage::ShaderResource);
  std::vector<Words> result(candidates.size());
  const auto output=buffer(r,result.data(),result.size()*sizeof(Words),sizeof(Words),
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  std::array<unsigned,12> zero{};
  const auto counters=buffer(r,zero.data(),sizeof(zero),sizeof(unsigned),
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BTGI cache commands");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BTGI cache pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BTGI cache pipeline bind");
  const rhi::ShaderCursor cursor(root);
  checked(cursor["btSurfaces"].setBinding(surfaces),"BTGI cache surfaces");
  checked(cursor["btCounters"].setBinding(shared_counters?shared_counters:counters.get()),"BTGI cache counters");
  checked(cursor["probeCandidates"].setBinding(inputs),"BTGI cache candidates");
  checked(cursor["probeResults"].setBinding(output),"BTGI cache output");
  const Words frame{epoch,frame_number,Capacity,BlockTransportLinks};
  const Words options{static_cast<unsigned>(candidates.size()),mode,visible?1u:0u,allocation_limit};
  checked(cursor["btFrameInfo"].setData(frame.data(),sizeof(frame)),"BTGI cache epoch");
  checked(cursor["probeOptions"].setData(options.data(),sizeof(options)),"BTGI cache options");
  pass->dispatchCompute(mode==2?(static_cast<unsigned>(candidates.size())+63)/64:1,1,1);pass->end();
  auto submission=commands->finish();require(bool(submission),"BTGI cache finish");
  require(r.frame_queue.submit(r.queue,submission,r.active_frame) &&
      r.frame_queue.wait(r.active_frame,2000),"BTGI cache bounded completion");
  checked(r.device->readBuffer(output,0,result.size()*sizeof(Words),result.data()),"BTGI cache result readback");
  block_transport_complete(r,start);
  return result;
}

std::array<BlockTransportSurface,Capacity> read_surfaces(WorldRenderer& r,rhi::IBuffer* surfaces) {
  std::array<BlockTransportSurface,Capacity> result{};
  checked(r.device->readBuffer(surfaces,0,sizeof(result),result.data()),"BTGI eviction surface readback");
  return result;
}
void evict(WorldRenderer& r,rhi::IComputePipeline* pipeline,rhi::IBuffer* surfaces,
    rhi::IBuffer* counters,rhi::IBuffer* direct,rhi::IBuffer* environment,unsigned frame_number,unsigned pressure) {
  const auto start=std::chrono::steady_clock::now();
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BTGI eviction commands");
  checked(commands->uploadBufferData(counters,11*sizeof(unsigned),sizeof(pressure),&pressure),
      "BTGI eviction capacity pressure");
  commands->setBufferState(counters,rhi::ResourceState::UnorderedAccess);
  commands->setBufferState(surfaces,rhi::ResourceState::UnorderedAccess);
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BTGI eviction pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BTGI eviction bind");
  const rhi::ShaderCursor cursor(root);
  checked(cursor["btSurfaces"].setBinding(surfaces),"BTGI eviction surfaces");
  checked(cursor["btCounters"].setBinding(counters),"BTGI eviction counters");
  checked(cursor["btDirect"].setBinding(direct),"BTGI eviction direct history");
  checked(cursor["btEnvironment"].setBinding(environment),"BTGI eviction environment history");
  const Words frame{Epoch,frame_number,Capacity,BlockTransportLinks};
  checked(cursor["btFrameInfo"].setData(frame.data(),sizeof(frame)),"BTGI eviction frame");
  pass->dispatchCompute((Capacity+63)/64,1,1);pass->end();
  auto submission=commands->finish();require(bool(submission),"BTGI eviction finish");
  require(r.frame_queue.submit(r.queue,submission,r.active_frame) &&
      r.frame_queue.wait(r.active_frame,2000),"BTGI eviction bounded completion");
  block_transport_complete(r,start);
}
void eviction_cases(WorldRenderer& r,rhi::IComputePipeline* cache,
    const std::vector<BlockTransportCandidate>& candidates) {
  constexpr unsigned Frame=1000,Tombstone=UINT32_MAX-1,Generation=7;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Evict.slang",
      "main",pipeline),"BTGI production eviction pipeline");
  std::array<BlockTransportSurface,Capacity> initial{};
  const auto index=[](unsigned i){return (Capacity-3+i)%Capacity;};
  for(unsigned i=0;i<7;++i) {
    auto& surface=initial[index(i)];surface.key=candidates[i].key;surface.albedo=candidates[i].albedo;
    surface.state={Epoch,1,1,1};surface.extra={Generation,1,1,0};
  }
  initial[index(0)].extra[1]=Frame;
  initial[index(2)].extra={Generation,0,Frame-1,0};
  initial[index(3)].extra[1]=Frame-64;
  initial[index(4)].extra={Generation,Frame-65,Frame-65,0};
  initial[index(4)].state[2]=UINT32_MAX;
  initial[index(5)].extra={Generation,0,Frame-64,0};
  initial[index(6)].extra[1]=0;
  initial[index(6)].state[1]=Frame;
  const auto surfaces=buffer(r,initial.data(),sizeof(initial),sizeof(BlockTransportSurface),
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  std::array<unsigned,12> stats{};stats[8]=7;stats[9]=6;stats[10]=17;
  const auto counters=buffer(r,stats.data(),sizeof(stats),sizeof(unsigned),
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopyDestination);
  using Pixel=std::array<float,4>;
  const Pixel poison{9.f,8.f,7.f,6.f};
  std::array<Pixel,Capacity> poisoned;poisoned.fill(poison);
  const auto direct=buffer(r,poisoned.data(),sizeof(poisoned),sizeof(Pixel),
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  const auto environment=buffer(r,poisoned.data(),sizeof(poisoned),sizeof(Pixel),
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  evict(r,pipeline,surfaces,counters,direct,environment,Frame,0);
  auto actual=read_surfaces(r,surfaces);
  for(unsigned i=0;i<7;++i)require(actual[index(i)].state[0]==Epoch,
      "BTGI eviction ran without capacity pressure");
  evict(r,pipeline,surfaces,counters,direct,environment,Frame,1);
  actual=read_surfaces(r,surfaces);
  for(unsigned i=0;i<7;++i) {
    const bool expired=i==1 || i==4 || i==6;
    require(actual[index(i)].state[0]==(expired?Tombstone:Epoch),
        "BTGI eviction violated visible retention, allocation grace, or contributor expiry");
    require(actual[index(i)].extra[0]==Generation,"BTGI eviction destroyed slot generation");
  }
  for(auto* history:{direct.get(),environment.get()}) {
    std::array<Pixel,Capacity> values{};
    checked(r.device->readBuffer(history,0,sizeof(values),values.data()),"BTGI evicted radiance history");
    for(unsigned i=0;i<7;++i) {
      const Pixel expected=i==1 || i==4 || i==6?Pixel{}:poison;
      require(values[index(i)]==expected,"BTGI eviction failed to clear recycled direct or environment history");
    }
  }
  checked(r.device->readBuffer(counters,0,sizeof(stats),stats.data()),"BTGI eviction statistics");
  require(stats[8]==4 && stats[9]==4 && stats[10]==17 && stats[11]==1,
      "BTGI eviction live gauges or cumulative pressure changed incorrectly");
  const std::vector<BlockTransportCandidate> existing{candidates[2]};
  auto lookup=run(r,cache,existing,surfaces,Epoch,1,Frame,false,counters);
  require(lookup[0][1]==index(2),"BTGI lookup stopped at an evicted collision-chain hole");
  lookup=run(r,cache,existing,surfaces,Epoch,0,Frame,false,counters);
  require(lookup[0][0]==index(2),"BTGI admission duplicated a key beyond a tombstone");
  actual=read_surfaces(r,surfaces);
  require(actual[index(1)].state[0]==Tombstone && actual[index(2)].extra[1]==Frame &&
      actual[index(2)].extra[3]==0,
      "BTGI existing contributor lost last-use tracking or became pinned");
  const std::vector<BlockTransportCandidate> duplicates(256,candidates[2]);
  const auto parallel=run(r,cache,duplicates,surfaces,Epoch,2,Frame,false,counters);
  for(const auto& result:parallel)require(result[0]==index(2),
      "BTGI concurrent duplicate admission reclaimed an earlier collision-chain tombstone");
  actual=read_surfaces(r,surfaces);
  unsigned occupied=0;for(const auto& surface:actual)if(surface.state[0]==Epoch)++occupied;
  require(occupied==4 && actual[index(1)].state[0]==Tombstone,
      "BTGI concurrent duplicate admission after eviction allocated an alias");
  const std::vector<BlockTransportCandidate> replacement{candidates[7]};
  lookup=run(r,cache,replacement,surfaces,Epoch,0,Frame,false,counters);
  require(lookup[0][0]==index(1) && lookup[0][1]==index(1),
      "BTGI replacement did not reuse the earliest collision-chain tombstone");
  actual=read_surfaces(r,surfaces);
  const auto& fresh=actual[index(1)];
  require(fresh.extra[0]==Generation+1 && fresh.extra[1]==Frame && fresh.extra[2]==Frame &&
      fresh.state[2]==UINT32_MAX && fresh.state[3]==0,
      "BTGI recycled slot retained identity, visibility, or radiance history");
  evict(r,pipeline,surfaces,counters,direct,environment,Frame+1,2);
  actual=read_surfaces(r,surfaces);
  require(actual[index(1)].state[0]==Epoch,"BTGI late-frame contributor missed allocation grace");
  lookup=run(r,cache,replacement,surfaces,Epoch,0,Frame+2,true,counters);
  require(lookup[0][0]==index(1),"BTGI visible refresh changed the resident slot");
  actual=read_surfaces(r,surfaces);
  require(actual[index(1)].extra[1]==Frame+2 && actual[index(1)].extra[2]==Frame,
      "BTGI visible refresh did not preserve allocation age separately");
  std::puts("block_transport_eviction=passed pressure_gate=1 tombstone_lookup=1 duplicate_after_hole=1 concurrent_duplicates_after_hole=256 generation=1 visible_retention=1 allocation_grace=1 contributor_expiry=1 live_gauges=1");
}
void admission_safety_cases(WorldRenderer& r,rhi::IComputePipeline* pipeline,
    const std::vector<BlockTransportCandidate>& candidates) {
  constexpr unsigned First=Capacity-3,Tombstone=UINT32_MAX-1;
  const auto writable=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess;
  std::array<BlockTransportSurface,Capacity> retired{};
  retired[First].state[0]=Tombstone;retired[First].extra[0]=UINT32_MAX;
  const auto surfaces=buffer(r,retired.data(),sizeof(retired),sizeof(BlockTransportSurface),writable);
  const std::vector<BlockTransportCandidate> candidate{candidates[0]};
  auto result=run(r,pipeline,candidate,surfaces,Epoch,0,1000,true);
  require(result[0][0]==First+1 && result[0][1]==First+1,
      "BTGI exhausted generation wrapped or blocked the next reusable slot");
  auto actual=read_surfaces(r,surfaces);
  require(actual[First].state[0]==Tombstone && actual[First].extra[0]==UINT32_MAX &&
      actual[First+1].extra[0]==1,"BTGI exhausted generation was not retired until geometry reset");
  std::array<BlockTransportSurface,Capacity> pending{};pending[First].state[0]=UINT32_MAX;
  const auto pending_surfaces=buffer(r,pending.data(),sizeof(pending),sizeof(BlockTransportSurface),writable);
  std::array<unsigned,12> stats{};
  const auto pending_counters=buffer(r,stats.data(),sizeof(stats),sizeof(unsigned),writable);
  result=run(r,pipeline,candidate,pending_surfaces,Epoch,0,1000,true,pending_counters);
  require(result[0][0]==UINT32_MAX,"BTGI admission did not defer a potentially identical pending claim");
  checked(r.device->readBuffer(pending_counters,0,sizeof(stats),stats.data()),"BTGI pending claim counters");
  require(stats[1]==1 && stats[8]==0 && stats[11]==1,"BTGI mandatory pending deferral was not recorded");
  std::array<BlockTransportSurface,Capacity> full{};
  for(unsigned i=0;i<BlockTransportProbes;++i) {
    auto& surface=full[(First+i)%Capacity];surface.key=candidates[i].key;
    surface.state={Epoch,1000,1,1};surface.extra={1,1000,1,0};
  }
  const auto full_surfaces=buffer(r,full.data(),sizeof(full),sizeof(BlockTransportSurface),writable);
  stats={};stats[8]=BlockTransportProbes;
  const auto full_counters=buffer(r,stats.data(),sizeof(stats),sizeof(unsigned),writable);
  const std::vector<BlockTransportCandidate> overflow{candidates[16]};
  result=run(r,pipeline,overflow,full_surfaces,Epoch,0,1000,false,full_counters);
  require(result[0][0]==UINT32_MAX,"BTGI full probe range exceeded its admission bound");
  checked(r.device->readBuffer(full_counters,0,sizeof(stats),stats.data()),"BTGI contributor overflow counters");
  require(stats[11]==0,"BTGI contributor overflow requested visible-set eviction");
  result=run(r,pipeline,overflow,full_surfaces,Epoch,0,1000,true,full_counters);
  require(result[0][0]==UINT32_MAX,"BTGI visible overflow replaced a retained slot without eviction");
  checked(r.device->readBuffer(full_counters,0,sizeof(stats),stats.data()),"BTGI visible overflow counters");
  require(stats[11]==1 && stats[8]==BlockTransportProbes,
      "BTGI visible hard capacity failure did not request bounded eviction");
  std::puts("block_transport_admission_safety=passed generation_exhaustion=1 pending_deferral=1 visible_capacity_pressure=1");
}
void contributor_budget_cases(WorldRenderer& r,rhi::IComputePipeline* pipeline,
    const std::vector<BlockTransportCandidate>& candidates) {
  constexpr unsigned Limit=8,Frame=1000;
  const auto writable=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess;
  std::array<BlockTransportSurface,Capacity> empty{};
  std::array<unsigned,12> stats{};
  const auto surfaces=buffer(r,empty.data(),sizeof(empty),sizeof(empty[0]),writable);
  const auto counters=buffer(r,stats.data(),sizeof(stats),sizeof(unsigned),writable|rhi::BufferUsage::CopyDestination);
  std::vector<BlockTransportCandidate> parallel;
  for(unsigned i=0;i<256;++i)parallel.push_back(candidates[i%17]);
  run(r,pipeline,parallel,surfaces,Epoch,2,7,false,counters,Limit);
  checked(r.device->readBuffer(counters,0,sizeof(stats),stats.data()),"BT contributor concurrent budget");
  require(stats[8]>0 && stats[8]<=Limit && stats[11]==0,"BT concurrent contributor budget exceeded");
  run(r,pipeline,candidates,surfaces,Epoch,0,7,false,counters,Limit);
  checked(r.device->readBuffer(counters,0,sizeof(stats),stats.data()),"BT contributor full budget");
  require(stats[8]==Limit && stats[11]==0,"BT contributor budget failed to fill or became mandatory pressure");
  auto actual=read_surfaces(r,surfaces);unsigned retained=Capacity;
  for(unsigned i=0;i<Capacity;++i)if(actual[i].state[0]==Epoch) {retained=i;break;}
  require(retained<Capacity,"BT contributor budget retained no row");
  const std::vector<BlockTransportCandidate> used{{actual[retained].key,actual[retained].albedo}};
  const auto touched=run(r,pipeline,used,surfaces,Epoch,0,Frame,false,counters,0);
  require(touched[0][0]==retained,"BT zero new-admission budget blocked existing contributor reuse");
  actual=read_surfaces(r,surfaces);
  require(actual[retained].extra[1]==Frame && actual[retained].extra[2]==7 && actual[retained].extra[3]==0,
      "BT useful contributor lost last-use tracking or became pinned");
  Slang::ComPtr<rhi::IComputePipeline> eviction;
  require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Evict.slang","main",eviction),
      "BT contributor retention eviction pipeline");
  using Pixel=std::array<float,4>;std::array<Pixel,Capacity> zero{};
  const auto direct=buffer(r,zero.data(),sizeof(zero),sizeof(Pixel),writable);
  const auto environment=buffer(r,zero.data(),sizeof(zero),sizeof(Pixel),writable);
  evict(r,eviction,surfaces,counters,direct,environment,Frame+1,1);
  actual=read_surfaces(r,surfaces);unsigned occupied=0;
  for(const auto& surface:actual)if(surface.state[0]==Epoch)++occupied;
  require(occupied==1 && actual[retained].state[0]==Epoch,
      "BT pressure evicted a reused contributor or retained unused contributors");
  checked(r.device->readBuffer(counters,0,sizeof(stats),stats.data()),"BT retained contributor occupancy");
  require(stats[8]==1,"BT bounded contributor occupancy differs from actual rows");
  std::puts("block_transport_contributor_budget=passed concurrent=256 hard_limit=8 last_use=1 zero_budget_reuse=1 unused_eviction=1");
}
}
void block_transport_cache_cases(Fixture& fixture) {
  auto& r=fixture.renderer;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportCache.slang").generic_string();
  require(block_transport_pipeline(r.device,path.c_str(),"main",pipeline),"BTGI production cache wrapper");
  std::vector<BlockTransportCandidate> candidates;
  for(int i=-16777217;candidates.size()<17;++i) {
    BlockSurfaceKey key{i,-33,16777217,unsigned(candidates.size()%6)};
    if((block_surface_hash(key)&(Capacity-1))==Capacity-3)candidates.push_back({key,{.2f,.4f,.8f,0}});
    require(i<-16700000,"BTGI collision fixture search exceeded bound");
  }
  candidates.push_back(candidates[0]);
  std::array<BlockTransportSurface,Capacity> empty{};
  const auto surfaces=buffer(r,empty.data(),sizeof(empty),sizeof(BlockTransportSurface),
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  const auto actual=run(r,pipeline,candidates,surfaces,Epoch,0);
  for(unsigned i=0;i<BlockTransportProbes;++i) {
    require(actual[i][0]==(Capacity-3+i)%Capacity && actual[i][1]==actual[i][0],
        "BTGI full-key collision chain lost or aliased an admitted surface");
    require(actual[i][2]==block_surface_hash(candidates[i].key),
        "BTGI shader hash differs from integer CPU hash beyond float precision");
  }
  require(actual[16][0]==UINT32_MAX && actual[16][1]==UINT32_MAX,"BTGI hash-probe bound exceeded");
  require(actual[17][0]==actual[0][0] && actual[17][1]==actual[0][0],"BTGI duplicate key consumed another slot");
  const auto stale=run(r,pipeline,candidates,surfaces,Epoch+1,1);
  for(const auto& item:stale)require(item[1]==UINT32_MAX,"BTGI lookup accepted previous geometry epoch");
  std::array<BlockTransportSurface,Capacity> retained{};
  checked(r.device->readBuffer(surfaces,0,sizeof(retained),retained.data()),"BTGI cache state readback");
  unsigned occupied=0;
  for(const auto& surface:retained)if(surface.state[0]==Epoch)++occupied;
  require(occupied==BlockTransportProbes,"BTGI cache overflow changed live occupancy");
  const auto parallel_surfaces=buffer(r,empty.data(),sizeof(empty),sizeof(BlockTransportSurface),
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  const std::vector<BlockTransportCandidate> duplicates(256,candidates[0]);
  const auto parallel=run(r,pipeline,duplicates,parallel_surfaces,Epoch,2);
  unsigned admitted=0;
  for(const auto& item:parallel) {
    require(item[0]==UINT32_MAX || item[0]==Capacity-3,"BTGI concurrent duplicate admission allocated an alias");
    if(item[0]!=UINT32_MAX)++admitted;
  }
  require(admitted>0,"BTGI concurrent admission made no progress");
  checked(r.device->readBuffer(parallel_surfaces,0,sizeof(retained),retained.data()),"BTGI parallel cache state");
  occupied=0;for(const auto& surface:retained)if(surface.state[0]==Epoch)++occupied;
  require(occupied==1,"BTGI concurrent duplicate keys occupied multiple slots");
  eviction_cases(r,pipeline,candidates);
  admission_safety_cases(r,pipeline,candidates);
  contributor_budget_cases(r,pipeline,candidates);
  block_transport_contributor_domain_cases(r);
  require(r.debug.errors.load()==0,"BTGI cache graphics validation errors");
  std::puts("block_transport_cache=passed hardware=1 production_admission=1 signed_hash=1 collisions=16 overflow=1 duplicate=1 concurrent_duplicates=256 stale_epoch=1 validation_errors=0");
}
}
