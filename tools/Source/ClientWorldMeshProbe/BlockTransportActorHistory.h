#pragma once
#include "BlockTransportWork.h"
#include "BlockTransportReference.h"
#include "BlockTransportSampling.h"
#include "BlockTransportPlayerBlocker.h"
#include "BlockTransportPlantProbe.h"
#include "LocalLight.h"
namespace mesh_probe::actor_history {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
using Vector=std::array<float,3>;
using Sample=BlockTransportSampleResult;
constexpr unsigned Capacity=256,Epoch=241;
constexpr BlockSurfaceKey Receiver{10,0,10,3};
inline double world_limit(const Sample& ray) {
  const double low[3]={9,1,9},high[3]={13,5,13};double result=1e30;
  for(unsigned c=0;c<3;++c)if(std::abs(ray.direction[c])>1e-12) {
    const double value=((ray.direction[c]>0?high[c]:low[c])-ray.origin[c])/ray.direction[c];
    if(value>0)result=std::min(result,value);
  }
  return result;
}
inline bool blocked(const Sample& ray,Vector center) {
  double near=0,far=world_limit(ray);
  for(unsigned c=0;c<3;++c) {
    if(std::abs(ray.direction[c])<1e-12) {
      if(ray.origin[c]<center[c]-.25 || ray.origin[c]>center[c]+.25)return false;
      continue;
    }
    double a=(center[c]-.25-ray.origin[c])/ray.direction[c],b=(center[c]+.25-ray.origin[c])/ray.direction[c];
    if(a>b)std::swap(a,b);near=std::max(near,a);far=std::min(far,b);
  }
  return far>=near && far>.00025;
}
inline unsigned mask(const std::vector<Sample>& rays,unsigned batch,Vector center) {
  unsigned result=0;for(unsigned slot=0;slot<16;++slot)if(blocked(rays[batch*16+slot],center))result|=1u<<slot;
  return result;
}
inline Vector moved(Vector p) {p[0]+=.02f;p[2]+=.03f;return p;}
inline Vector placement(const std::vector<Sample>& rays,bool previous) {
  for(unsigned slot=0;slot<16;++slot)for(float distance:{.5f,.75f,1.f,1.5f,2.f,2.5f}) {
    const auto& ray=rays[(previous?0:16)+slot];Vector p{};
    for(unsigned c=0;c<3;++c)p[c]=ray.origin[c]+ray.direction[c]*distance;
    if(p[0]<9.3f || p[0]>12.7f || p[1]<1.3f || p[1]>4.7f || p[2]<9.3f || p[2]>12.7f)continue;
    const auto a=mask(rays,0,p),b=mask(rays,0,moved(p));
    if(previous?a!=0 && a==b:a==0 && b==0 && mask(rays,1,moved(p))!=0)return p;
  }
  require(false,"BT actor fixture cannot distinguish old and new ray paths");return {};
}
class Probe {
  WorldRenderer& r;
  BlockTransportWork selection;
  Slang::ComPtr<rhi::IBuffer> surfaces,links,candidates,counters,direct,environment,zero,lights,tree;
  Slang::ComPtr<rhi::IComputePipeline> trace,resolve,inject;
  unsigned receiver{},frame{};
  std::array<BlockTransportSurface,Capacity> rows{};
  std::array<Words,Capacity*16> connections{};
  Slang::ComPtr<rhi::IBuffer> storage(const void* value,std::size_t bytes,unsigned stride) {
    return buffer(r,value,bytes,stride,rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopyDestination);
  }
  void dispatch(rhi::ICommandEncoder* commands,rhi::IComputePipeline* pipeline,rhi::IAccelerationStructure* actor,bool enabled,unsigned revision) {
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT actor history compute pass");
    auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT actor history pipeline binding");
    require(bind_world_atlas(r.atlas,root),"BT actor history masks");
    if(pipeline!=resolve.get())require(world_ray_bind(r,root),"BT actor history real world scene");
    selection.bind(root);const rhi::ShaderCursor cursor(root);
    const auto bind=[&](const char* name,rhi::IBuffer* value){auto c=cursor[name];if(c.isValid())checked(c.setBinding(value),name);};
    const auto data=[&](const char* name,const void* value,std::size_t bytes){auto c=cursor[name];if(c.isValid())checked(c.setData(value,bytes),name);};
    bind("btSurfaces",surfaces);bind("btLinks",links);bind("btCandidates",candidates);bind("btCounters",counters);
    bind("btDirect",direct);bind("btEnvironment",environment);bind("localLights",lights);bind("giLightTree",tree);
    const Words info{Epoch,frame,Capacity,16},work{0,1,Epoch,0};
    const Pixel low{0,0,0,1},high{32,32,32,0},sun{0,1,0,0},sky{},settings{1,64,.001f,0};
    const unsigned nodes=0,active=enabled?1:0;
    data("btFrameInfo",info.data(),sizeof(info));data("btWork",work.data(),sizeof(work));data("btPlayerRevision",&revision,sizeof(revision));
    data("btCoverageMin",low.data(),sizeof(low));data("btCoverageMax",high.data(),sizeof(high));
    data("btSun",sun.data(),sizeof(sun));data("btSky",sky.data(),sizeof(sky));data("raySettings",settings.data(),sizeof(settings));
    data("giLightNodeCount",&nodes,sizeof(nodes));data("playerShadowEnabled",&active,sizeof(active));
    auto player=cursor["playerShadowScene"];if(player.isValid())checked(player.setBinding(rhi::Binding(actor)),"BT actor history triangle scene");
    pass->dispatchCompute(1,1,1);pass->end();commands->globalBarrier();
  }
public:
  explicit Probe(WorldRenderer& renderer):r(renderer),selection(r,Capacity,1) {
    for(const auto key:transport_reference::keys()) {
      unsigned slot=Capacity;
      for(unsigned attempt=0;attempt<16;++attempt) {
        const auto candidate=(block_surface_hash(key)+attempt)&(Capacity-1);
        if(rows[candidate].state[0]==0){slot=candidate;break;}
      }
      require(slot<Capacity,"BT actor history closed-room cache population");
      rows[slot]={key,{.5f,.5f,.5f,1},{Epoch,0,UINT32_MAX,0},{1,0,0,1}};
      if(key==Receiver)receiver=slot;
    }
    require(rows[receiver].key==Receiver,"BT actor receiver identity");
    surfaces=storage(rows.data(),sizeof(rows),sizeof(rows[0]));links=storage(connections.data(),sizeof(connections),sizeof(Words));
    std::array<BlockTransportCandidate,16> empty_candidates{};std::array<unsigned,12> empty_counters{};
    candidates=storage(empty_candidates.data(),sizeof(empty_candidates),sizeof(empty_candidates[0]));
    counters=storage(empty_counters.data(),sizeof(empty_counters),sizeof(unsigned));
    std::array<Pixel,Capacity> values{};direct=storage(values.data(),sizeof(values),sizeof(Pixel));
    environment=storage(values.data(),sizeof(values),sizeof(Pixel));zero=storage(values.data(),sizeof(values),sizeof(Pixel));
    WorldLocalLight light{};std::array<Pixel,3> node{};lights=storage(&light,sizeof(light),sizeof(light));tree=storage(node.data(),sizeof(node),sizeof(node));
    require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Trace.slang","main",trace) &&
        block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/ResolveContributors.slang","main",resolve) &&
        block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Direct.slang","main",inject),"BT actor history retained kernels");
  }
  void run(const char* name,rhi::IAccelerationStructure* actor,bool enabled,unsigned revision,unsigned expected_mask,
      bool seed=false,bool reset=false,unsigned mutation=0,unsigned expected_rechecks=16) {
    ++frame;const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BT actor history frame reuse");
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT actor history command encoder");
    std::array<unsigned,16> header{};header[0]=receiver;
    checked(commands->uploadBufferData(selection.schedule,0,sizeof(header),header.data()),"BT actor fixed receiver selection");
    if(!seed) {
      rows[receiver].state[3]=16;
      if(mutation==1)for(unsigned slot=0;slot<16;++slot)connections[receiver*16+slot]={UINT32_MAX,0,0,0};
      if(mutation==2) {unsigned slot=0;while(slot<16 && connections[receiver*16+slot][3]!=1)++slot;
        require(slot<16,"BT actor stale-link fixture lacks clear world path");--connections[receiver*16+slot][2];}
      std::array<Pixel,Capacity> values{};values[receiver]={1,1,1,std::bit_cast<float>(Epoch)};
      checked(commands->uploadBufferData(surfaces,0,sizeof(rows),rows.data()),"BT actor prior history count");
      checked(commands->uploadBufferData(links,0,sizeof(connections),connections.data()),"BT actor prior resolved links");
      checked(commands->uploadBufferData(direct,0,sizeof(values),values.data()),"BT actor prior direct history");
    }
    commands->globalBarrier();selection.encode(commands,surfaces,direct,environment,zero,{Epoch,frame,Capacity,16},Epoch);
    dispatch(commands,trace,actor,enabled,revision);dispatch(commands,resolve,actor,enabled,revision);dispatch(commands,inject,actor,enabled,revision);
    plant_probe::submit(r,commands);
    checked(r.device->readBuffer(surfaces,0,sizeof(rows),rows.data()),"BT actor history surface readback");
    checked(r.device->readBuffer(links,0,sizeof(connections),connections.data()),"BT actor history link readback");
    std::array<Pixel,Capacity> actual{};checked(r.device->readBuffer(direct,0,sizeof(actual),actual.data()),"BT actor history direct readback");
    unsigned mask=0;
    for(unsigned slot=0;slot<16;++slot) {
      const auto link=connections[receiver*16+slot];
      require(link[3]==1 || link[3]==4,"BT actor closed room has missing contributors");
      if(link[3]==4) {mask|=1u<<slot;const float limit=std::bit_cast<float>(link[0]);
        require(link[1]==Epoch && std::isfinite(limit) && limit>0 && limit<8,"BT actor lost underlying world endpoint");}
    }
    require(mask==expected_mask,"BT actor triangle visibility differs from independent slab oracle");
    const auto stats=selection.header();
    require(stats[2]==1 && stats[9]==(enabled?16u:0u) && stats[10]==(seed?0u:expected_rechecks),"BT actor previous/current query accounting");
    require(rows[receiver].state[3]==(seed || reset?1u:16u),"BT actor unchanged path reset history or changed path retained it");
    if(!seed)for(unsigned c=0;c<3;++c)require(std::abs(actual[receiver][c]-(reset?0.f:15.f/16))<1e-6,
        "BT actor history reset did not reach production Direct");
    std::array<unsigned,12> counts{};checked(r.device->readBuffer(counters,0,sizeof(counts),counts.data()),"BT actor terrain count");
    require(counts[6]==frame*16,"BT actor revalidation issued extra world rays");
    block_transport_complete(r,start);
    if(!seed)std::printf("block_transport_actor_history_case=%s status=passed previous_queries=%u current_queries=%u\n",name,stats[10],stats[9]);
  }
};
}
