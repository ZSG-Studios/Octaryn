#include "BlockTransportWorldAdmission.h"
#include "BlockTransportWork.h"
#include "BlockTransportSampling.h"
#include "LocalLight.h"

namespace mesh_probe::world_admission {
namespace {
constexpr unsigned Rows=2048,RadianceEpoch=137;
const Key Low{-80,-52,58,0},High{-16,12,122,0},Receiver{-48,-24,112,3};
struct LightNode {Pixel bounds_min_flux,bounds_max;Words links;};
static_assert(sizeof(LightNode)==48);
struct RoomResult {Snapshot incident,direct;};
void settle(WorldRenderer& r) {
  for(unsigned attempt=0;attempt<64;++attempt) {
    const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BT offscreen room RT frame reuse");
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT offscreen room RT encoder");
    require(world_ray_prepare(r,commands,r.active_frame),"BT offscreen room production RT preparation");
    submit(r,commands);const bool ready=world_ray_coverage_complete(r);cap(r,start);if(ready)return;
  }
  require(false,"BT offscreen room geometry did not converge within bounded frames");
}
class RoomSolve {
  WorldRenderer& r;
  Cache& cache;
  BlockTransportWork selection;
  Slang::ComPtr<rhi::IComputePipeline> trace,resolve,direct,bounce;
  Slang::ComPtr<rhi::IBuffer> links,candidates,direct_values,environment,lights,tree,zero;
  std::array<Slang::ComPtr<rhi::IBuffer>,4> orders;
  void pass(rhi::IComputePipeline* pipeline,unsigned count,unsigned order,unsigned groups,bool ray) {
    const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BT offscreen room solve frame reuse");
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT offscreen room solve encoder");
    const Words frame{Epoch,Frame,Capacity,Links},work{0,count,RadianceEpoch,0},solve{RadianceEpoch,Capacity,order,Links};
    if(pipeline==trace.get())selection.encode(commands,cache.surfaces,direct_values,environment,orders[3],frame,RadianceEpoch);
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT offscreen room solve pass");
    auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT offscreen room solve pipeline binding");
    require(bind_world_atlas(r.atlas,root),"BT offscreen room atlas binding");
    if(ray)require(world_ray_bind(r,root),"BT offscreen room exact RT scene");
    const rhi::ShaderCursor cursor(root);
    selection.bind(root);
    const auto bind=[&](const char* name,rhi::IBuffer* buffer) {
      auto field=cursor[name];if(field.isValid())checked(field.setBinding(buffer),name);
    };
    const auto data=[&](const char* name,const void* value,std::size_t bytes) {
      auto field=cursor[name];if(field.isValid())checked(field.setData(value,bytes),name);
    };
    bind("btSurfaces",cache.surfaces);bind("btCounters",cache.counters);bind("btLinks",links);bind("btCandidates",candidates);
    bind("btDirect",direct_values);bind("btEnvironment",environment);bind("localLights",lights);bind("giLightTree",tree);
    bind("btB0",order>0?orders[0].get():zero.get());bind("btB1",order>1?orders[1].get():zero.get());
    bind("btB2",order>2?orders[2].get():zero.get());bind("btOutput",orders[order]);
    const Pixel minimum{-64,-32,96,1},maximum{-32,0,128,0},sun{0,1,0,0},sky{},settings{1,64,.002f,0};
    const unsigned nodes=1;
    data("btFrameInfo",frame.data(),sizeof(frame));data("btWork",work.data(),sizeof(work));data("btSolve",solve.data(),sizeof(solve));
    data("btCoverageMin",minimum.data(),sizeof(minimum));data("btCoverageMax",maximum.data(),sizeof(maximum));
    data("btSun",sun.data(),sizeof(sun));data("btSky",sky.data(),sizeof(sky));data("raySettings",settings.data(),sizeof(settings));
    data("giLightNodeCount",&nodes,sizeof(nodes));
    const unsigned player_revision=0;data("btPlayerRevision",&player_revision,sizeof(player_revision));
    for(const char* forbidden:{"btSurfaceKeys","positions","voxels","colors","btDimensions","projection"})
      require(!cursor[forbidden].isValid(),"BT offscreen room GI pass depends on screen data");
    pass->dispatchCompute(groups,1,1);pass->end();commands->globalBarrier();submit(r,commands);cap(r,start);
  }
public:
  RoomSolve(WorldRenderer& renderer,Cache& admitted,bool enabled):r(renderer),cache(admitted),selection(r,Capacity,Rows) {
    const auto pipeline=[&](const char* name,Slang::ComPtr<rhi::IComputePipeline>& output) {
      const auto path=std::string("octaryn-client/Shaders/BlockTransportGI/")+name+".slang";
      require(block_transport_pipeline(r.device,path.c_str(),"main",output),"BT offscreen room production pipeline");
    };
    pipeline("Trace",trace);pipeline("ResolveContributors",resolve);pipeline("Direct",direct);pipeline("Bounce",bounce);
    const auto rw=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess;
    std::vector<Words> empty_links(Capacity*Links);std::vector<BlockTransportCandidate> empty_candidates(Rows*Links);
    std::vector<Pixel> empty_values(Capacity);
    links=buffer(r,empty_links.data(),empty_links.size()*sizeof(Words),sizeof(Words),rw);
    candidates=buffer(r,empty_candidates.data(),empty_candidates.size()*sizeof(BlockTransportCandidate),sizeof(BlockTransportCandidate),rw);
    direct_values=buffer(r,empty_values.data(),empty_values.size()*sizeof(Pixel),sizeof(Pixel),rw);
    environment=buffer(r,empty_values.data(),empty_values.size()*sizeof(Pixel),sizeof(Pixel),rw);
    zero=buffer(r,empty_values.data(),empty_values.size()*sizeof(Pixel),sizeof(Pixel),rhi::BufferUsage::ShaderResource);
    for(auto& value:orders)value=buffer(r,empty_values.data(),empty_values.size()*sizeof(Pixel),sizeof(Pixel),rw);
    WorldLocalLight light;light.position_range={-47.5f,-20,112.5f,24};
    light.color_intensity={.9f,.15f,.05f,enabled?32.f:0.f};light.axis_v_type[3]=0;
    const LightNode node{{-47.5f,-20,112.5f,1},{-47.5f,-20,112.5f,0},{0,0,0,1}};
    lights=buffer(r,&light,sizeof(light),sizeof(light),rhi::BufferUsage::ShaderResource);
    tree=buffer(r,&node,sizeof(node),sizeof(node),rhi::BufferUsage::ShaderResource);
  }
  RoomResult run(bool enabled,const std::vector<BlockTransportSampleResult>& samples) {
    pass(trace,Rows,0,(Rows*Links+63)/64,true);
    const auto header=selection.header();
    require(header[2]==header[3] && header[3]>300 && header[3]<=Rows,
        "BT room occupied receivers did not fit the actual selected work queue");
    // Every room surface is admitted before tracing, including surfaces behind the camera.
    pass(resolve,Rows,0,(Rows*Links+63)/64,false);
    pass(direct,Rows,0,(Rows+63)/64,true);
    for(unsigned order=0;order<4;++order)pass(bounce,0,order,(Capacity+63)/64,false);
    const auto rows=cache.read();std::vector<Pixel> incident(Capacity),sources(Capacity),sky(Capacity);
    std::vector<Words> actual_links(Capacity*Links);
    checked(r.device->readBuffer(orders[3],0,incident.size()*sizeof(Pixel),incident.data()),"BT offscreen room indirect readback");
    checked(r.device->readBuffer(direct_values,0,sources.size()*sizeof(Pixel),sources.data()),"BT offscreen room source readback");
    checked(r.device->readBuffer(environment,0,sky.size()*sizeof(Pixel),sky.data()),"BT offscreen room sky readback");
    checked(r.device->readBuffer(links,0,actual_links.size()*sizeof(Words),actual_links.data()),"BT offscreen room transport readback");
    RoomResult result;unsigned receiver=Capacity;
    for(unsigned row=0;row<Capacity;++row)if(rows[row].state[0]==Epoch) {
      const auto identity=key(rows[row].key);
      result.incident.emplace(identity,incident[row]);result.direct.emplace(identity,sources[row]);
      for(unsigned c=0;c<3;++c)require(std::isfinite(incident[row][c]) && incident[row][c]>=0 && sky[row][c]==0,
          "BT sealed room gained sky or invalid energy");
      if(identity==Receiver)receiver=row;
      if(!enabled)for(unsigned c=0;c<3;++c)require(incident[row][c]==0 && sources[row][c]==0,
          "BT sealed room without a source is not dark");
    }
    require(receiver<Capacity,"BT offscreen floor receiver was never admitted");
    for(unsigned slot=0;slot<Links;++slot)require(actual_links[receiver*Links+slot][3]==1,
        "BT sealed room receiver lost a world contributor or leaked sky");
    if(enabled) {
      require(samples.size()==8,"BT room needs all eight joint receiver samples");double attenuation=0;
      for(const auto& sample:samples) {
        require(sample.direct[3]==1,"BT room receiver area source sample");const auto& p=sample.direct;
        const double x=-47.5-p[0],y=-20-p[1],z=112.5-p[2];
        const double squared=x*x+y*y+z*z,distance=std::sqrt(squared);
        attenuation+=std::pow(1-std::pow(distance/24,4),2)/squared*y/distance/8;
      }
      const double color[3]={.9,.15,.05};
      for(unsigned c=0;c<3;++c)require(std::abs(sources[receiver][c]-color[c]*32*attenuation/3.141592653589793)<2e-5,
          "BT offscreen direct source differs from analytic point-light Lambertian injection");
      require(incident[receiver][0]>.001f && incident[receiver][0]>incident[receiver][2]*4,
          "BT offscreen room did not receive colored indirect transport");
    }
    return result;
  }
};
void same(const Snapshot& expected,const Snapshot& actual) {
  require(expected.size()==actual.size(),"BT room camera change altered world radiance population");
  for(const auto& [identity,radiance]:expected) {
    const auto found=actual.find(identity);require(found!=actual.end(),"BT room camera change dropped a keyed surface");
    for(unsigned c=0;c<3;++c)require(std::abs(radiance[c]-found->second[c])<2e-5f,
        "BT room radiance depends on camera direction or resolution");
  }
}
}
void room(Fixture& f) {
  auto& r=f.renderer;r.ray_enabled=true;
  require(world_ray_available(r),"BT offscreen room prepared RT initialization");
  open_world_renderer_set_center(&r,-2,3,0);auto source=column(-2,3,-32,32);
  const auto previous=r.sources.find({-2,3});source.revision=previous==r.sources.end()?1:previous->second.revision+1;
  const auto stone=static_cast<std::uint16_t>(material(f,"stone"));
  for(int z=12;z<=20;++z)for(int y=8;y<=16;++y)for(int x=12;x<=20;++x)
    if(x==12 || x==20 || y==8 || y==16 || z==12 || z==20)put(source,x,y,z,stone);
  source.blocks.compact();require(open_world_renderer_update(&r,source),"BT offscreen room publication");settle(r);
  const auto found=r.columns.find({-2,3});require(found!=r.columns.end(),"BT offscreen room resident face owner");
  const auto mesh=f.read_mesh(found->second);f.verify("block_transport_offscreen_room",source,mesh);
  const auto wanted=expected(f,source,Low,High);const auto width=r.width,height=r.height;
  const bool culling=r.culling_enabled;r.culling_enabled=true;RoomResult reference;
  std::vector<BlockTransportSampleQuery> area_queries;
  for(unsigned i=0;i<8;++i)area_queries.push_back({{Receiver[0],Receiver[1],Receiver[2],unsigned(Receiver[3])},{0,0,0,i+1}});
  const auto sampled=block_transport_samples(f,area_queries);
  for(unsigned test=0;test<3;++test) {
    r.width=test?1280:320;r.height=test?720:180;
    world_renderer_prepare_draw(r,WorldCamera{-48,-20,90,test?0.f:3.14159265359f,0,1.03f});
    require((r.drawn_columns==0)==(test!=0),"BT lit room fixture is not wholly offscreen");
    Cache cache(r);cache.fill(mesh.gpu.faces,unsigned(mesh.faces.size()),Low,High,wanted);
    RoomSolve solve(r,cache,test!=2);const auto result=solve.run(test!=2,sampled);
    if(test==0)reference=result;else if(test==1) {same(reference.incident,result.incident);same(reference.direct,result.direct);}
  }
  r.width=width;r.height=height;r.culling_enabled=culling;
  require(r.debug.errors.load()==0,"BT offscreen room graphics validation errors");
  std::printf("block_transport_world_room=passed hardware=1 production_admit=1 production_trace=1 production_direct=1 production_bounce=1 offscreen_lit=1 camera_resolution_invariant=1 point_light_oracle=1 sealed_dark_without_source=1 rows=%zu receiver_indirect=%.9g receiver_direct=%.9g validation_errors=0\n",
      wanted.size(),reference.incident.at(Receiver)[0],reference.direct.at(Receiver)[0]);
}
}
