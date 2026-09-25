#include "BlockTransportReference.h"
#include "BlockTransportSampling.h"
#include "LocalLight.h"
#include <slang-rhi/shader-cursor.h>
#include <bit>
#include <fstream>

namespace mesh_probe {
namespace {
using namespace transport_reference;
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
constexpr unsigned Capacity=256,Geometry=173,Radiance=179,Links=16;
struct LightNode {Pixel minimum,maximum;Words links;};
struct Error {double mean{},rms{},peak{};};
Error error(const Field& actual,const Field& reference) {
  double total=0,delta=0,squared=0,energy=0,peak=0,max_error=0;
  for(unsigned row=0;row<Faces;++row)for(unsigned c=0;c<3;++c) {
    require(std::isfinite(actual[row][c]) && actual[row][c]>=0,"BT convergence invalid linear radiance");
    total+=reference[row][c];delta+=actual[row][c]-reference[row][c];
    squared+=std::pow(actual[row][c]-reference[row][c],2);energy+=reference[row][c]*reference[row][c];
    peak=std::max(peak,reference[row][c]);max_error=std::max(max_error,std::abs(actual[row][c]-reference[row][c]));
  }
  require(total>0 && energy>0 && peak>0,"BT convergence reference has no light");
  return {std::abs(delta)/total,std::sqrt(squared/energy),max_error/peak};
}
void settle(WorldRenderer& r) {
  for(unsigned attempt=0;attempt<64;++attempt) {
    const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BT convergence RT slot reuse");
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT convergence RT encoder");
    require(world_ray_prepare(r,commands,r.active_frame),"BT convergence production RT preparation");
    auto submission=commands->finish();require(bool(submission),"BT convergence RT finish");
    require(r.frame_queue.submit(r.queue,submission,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
        "BT convergence RT bounded completion");
    const bool ready=world_ray_coverage_complete(r);block_transport_complete(r,start);if(ready)return;
  }
  require(false,"BT convergence RT scene did not become ready");
}
class Convergence {
  Fixture& fixture;
  WorldRenderer& r;
  const Reference& reference;
  std::array<unsigned,Faces> slots{};
  Slang::ComPtr<rhi::IBuffer> surfaces,links,candidates,counters,schedule,rows,blocks,lights,tree,direct,environment,zero;
  std::array<Slang::ComPtr<rhi::IBuffer>,4> orders;
  std::array<Slang::ComPtr<rhi::IComputePipeline>,3> select;
  Slang::ComPtr<rhi::IComputePipeline> trace,resolve,inject,bounce;
  std::ofstream observations{"block-transport-convergence.csv"};
  unsigned batch{};
  Slang::ComPtr<rhi::IBuffer> storage(const void* value,std::size_t bytes,unsigned stride) {
    return buffer(r,value,bytes,stride,rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  }
  void dispatch(rhi::ICommandEncoder* commands,rhi::IComputePipeline* pipeline,unsigned groups,
      unsigned order=0,bool ray=false) {
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT convergence compute pass");
    auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT convergence production pipeline");
    require(bind_world_atlas(r.atlas,root),"BT convergence atlas");
    if(ray)require(world_ray_bind(r,root),"BT convergence shared exact RT scene");
    rhi::ShaderCursor cursor(root);
    const auto bind=[&](const char* name,rhi::IBuffer* value) {
      auto field=cursor[name];if(field.isValid())checked(field.setBinding(value),name);
    };
    const auto data=[&](const char* name,const void* value,std::size_t bytes) {
      auto field=cursor[name];if(field.isValid())checked(field.setData(value,bytes),name);
    };
    bind("btSurfaces",surfaces);bind("btLinks",links);bind("btCandidates",candidates);bind("btCounters",counters);
    bind("btWorkRows",rows);bind("btWorkBlocks",blocks);bind("btSchedule",schedule);
    bind("btDirect",direct);bind("btEnvironment",environment);bind("btIndirect",orders[3]);
    bind("localLights",lights);bind("giLightTree",tree);
    bind("btB0",order>0?orders[0].get():zero.get());bind("btB1",order>1?orders[1].get():zero.get());
    bind("btB2",order>2?orders[2].get():zero.get());bind("btOutput",orders[order]);
    const Words frame{Geometry,batch,Capacity,Links},work{0,Faces,Radiance,0},solve{Radiance,Capacity,order,Links};
    const Pixel low{0,0,0,1},high{32,32,32,0},sun{0,1,0,0},sky{},settings{1,64,.001f,0};
    const unsigned node_count=1,player_revision=0;
    data("btFrameInfo",frame.data(),sizeof(frame));data("btWork",work.data(),sizeof(work));data("btSolve",solve.data(),sizeof(solve));
    data("btCoverageMin",low.data(),sizeof(low));data("btCoverageMax",high.data(),sizeof(high));
    data("btSun",sun.data(),sizeof(sun));data("btSky",sky.data(),sizeof(sky));data("raySettings",settings.data(),sizeof(settings));
    data("giLightNodeCount",&node_count,sizeof(node_count));
    data("btPlayerRevision",&player_revision,sizeof(player_revision));
    pass->dispatchCompute(groups,1,1);pass->end();commands->globalBarrier();
  }
  Field read(rhi::IBuffer* source) {
    std::array<Pixel,Capacity> values{};
    checked(r.device->readBuffer(source,0,sizeof(values),values.data()),"BT convergence linear readback");
    Field result{};
    for(unsigned i=0;i<Faces;++i) {
      require(std::bit_cast<unsigned>(values[slots[i]][3])==Radiance,"BT convergence absent or stale radiance row");
      for(unsigned c=0;c<3;++c)result[i][c]=values[slots[i]][c];
    }
    return result;
  }
public:
  Convergence(Fixture& owner,const Reference& exact):fixture(owner),r(owner.renderer),reference(exact) {
    const auto pipeline=[&](const char* name,const char* entry,Slang::ComPtr<rhi::IComputePipeline>& value) {
      const auto path=std::string("octaryn-client/Shaders/BlockTransportGI/")+name+".slang";
      require(block_transport_pipeline(r.device,path.c_str(),entry,value),"BT convergence prepared production shader");
    };
    pipeline("Select","count_main",select[0]);pipeline("Select","prefix_main",select[1]);pipeline("Select","select_main",select[2]);
    pipeline("Trace","main",trace);pipeline("ResolveContributors","main",resolve);
    pipeline("Direct","main",inject);pipeline("Bounce","main",bounce);
    std::array<BlockTransportSurface,Capacity> initial{};
    for(unsigned i=0;i<Faces;++i) {
      const auto key=reference.surfaces[i];unsigned row=Capacity;
      for(unsigned attempt=0;attempt<BlockTransportProbes;++attempt) {
        const unsigned probe=(block_surface_hash(key)+attempt)&(Capacity-1);
        if(initial[probe].state[0]==0){row=probe;break;}
      }
      require(row<Capacity,"BT convergence known closed-room population exceeds hash probes");slots[i]=row;
      initial[row]={key,{.5f,.5f,.5f,1},{Geometry,0,UINT32_MAX,0},{1,0,0,1}};
    }
    surfaces=storage(initial.data(),sizeof(initial),sizeof(initial[0]));
    std::array<Words,Capacity*Links> empty_links{};std::array<BlockTransportCandidate,Faces*Links> empty_candidates{};
    std::array<Pixel,Capacity> empty_values{};std::array<unsigned,16> header{};std::array<unsigned,12> stats{};
    stats[8]=Faces;std::array<Words,Faces> queue{};Words block{};
    links=storage(empty_links.data(),sizeof(empty_links),sizeof(Words));
    candidates=storage(empty_candidates.data(),sizeof(empty_candidates),sizeof(BlockTransportCandidate));
    counters=storage(stats.data(),sizeof(stats),sizeof(unsigned));schedule=storage(header.data(),sizeof(header),sizeof(unsigned));
    rows=storage(queue.data(),sizeof(queue),sizeof(Words));blocks=storage(block.data(),sizeof(block),sizeof(Words));
    direct=storage(empty_values.data(),sizeof(empty_values),sizeof(Pixel));environment=storage(empty_values.data(),sizeof(empty_values),sizeof(Pixel));
    zero=storage(empty_values.data(),sizeof(empty_values),sizeof(Pixel));
    for(auto& value:orders)value=storage(empty_values.data(),sizeof(empty_values),sizeof(Pixel));
    WorldLocalLight light;light.position_range={10.3f,3.5f,10.7f,24};light.color_intensity={.9f,.4f,.15f,20};light.axis_v_type[3]=0;
    LightNode node{{10.3f,3.5f,10.7f,1},{10.3f,3.5f,10.7f,0},{0,0,0,1}};
    lights=storage(&light,sizeof(light),sizeof(light));tree=storage(&node,sizeof(node),sizeof(node));
    observations<<"batch,rows,direct_mean_error,direct_rms_error,indirect_mean_error,indirect_rms_error,indirect_peak_error\n";
  }
  std::pair<Error,Error> run() {
    ++batch;const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BT convergence frame reuse");
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT convergence command encoder");
    for(auto& pass:select)dispatch(commands,pass,1);
    dispatch(commands,trace,(Faces*Links+63)/64,0,true);
    dispatch(commands,resolve,(Faces*Links+63)/64);
    dispatch(commands,inject,(Faces+63)/64,0,true);
    for(unsigned order=0;order<4;++order)dispatch(commands,bounce,(Capacity+63)/64,order);
    auto submission=commands->finish();require(bool(submission),"BT convergence command finish");
    require(r.frame_queue.submit(r.queue,submission,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
        "BT convergence bounded completion");
    std::array<BlockTransportSurface,Capacity> actual{};std::array<Words,Capacity*Links> edges{};
    std::array<unsigned,16> header{};
    checked(r.device->readBuffer(surfaces,0,sizeof(actual),actual.data()),"BT convergence identities");
    checked(r.device->readBuffer(links,0,sizeof(edges),edges.data()),"BT convergence complete connections");
    checked(r.device->readBuffer(schedule,0,sizeof(header),header.data()),"BT convergence selected count");
    require(header[2]==Faces && header[3]==Faces,"BT convergence scheduler skipped resident interior rows");
    for(unsigned i=0;i<Faces;++i) {
      const unsigned row=slots[i];const auto& surface=actual[row];
      require(surface.key==reference.surfaces[i] && surface.state[0]==Geometry && surface.state[1]==batch &&
          surface.state[2]==batch && surface.state[3]==std::min(batch,16u),"BT convergence row did not advance exactly once");
      for(unsigned link=0;link<Links;++link) {
        const auto edge=edges[row*Links+link];
        const bool valid=edge[3]==1 && edge[0]<Capacity && edge[1]==Geometry && edge[2]==1 && actual[edge[0]].state[0]==Geometry;
        if(!valid) {
          std::array<Words,Faces> selected{};std::array<BlockTransportCandidate,Faces*Links> pending{};
          std::array<unsigned,12> stats{};
          checked(r.device->readBuffer(rows,0,sizeof(selected),selected.data()),"BT convergence failure queue readback");
          checked(r.device->readBuffer(candidates,0,sizeof(pending),pending.data()),"BT convergence failure candidate readback");
          checked(r.device->readBuffer(counters,0,sizeof(stats),stats.data()),"BT convergence failure counter readback");
          unsigned work_index=Faces;for(unsigned q=0;q<Faces;++q)if(selected[q][0]==row){work_index=q;break;}
          std::printf("block_transport_convergence_gap batch=%u sample_batch=%u face_index=%u source_row=%u slot=%u "
              "source_key=(%d,%d,%d,%u) source_state=(%u,%u,%u,%u) source_extra=(%u,%u,%u,%u) "
              "link=(%u,%u,%u,%u) queue_index=%u selected=%u occupied=%u\n",
              batch,batch-1,i,row,link,surface.key.x,surface.key.y,surface.key.z,surface.key.direction,
              surface.state[0],surface.state[1],surface.state[2],surface.state[3],
              surface.extra[0],surface.extra[1],surface.extra[2],surface.extra[3],
              edge[0],edge[1],edge[2],edge[3],work_index,header[2],header[3]);
          if(work_index<Faces) {
            const auto& candidate=pending[work_index*Links+link];const auto queue=selected[work_index];
            std::printf("block_transport_convergence_candidate queue=(%u,%u,%u,%u) key=(%d,%d,%d,%u) albedo=(%.9g,%.9g,%.9g,%.9g)\n",
                queue[0],queue[1],queue[2],queue[3],candidate.key.x,candidate.key.y,candidate.key.z,candidate.key.direction,
                candidate.albedo[0],candidate.albedo[1],candidate.albedo[2],candidate.albedo[3]);
          }
          if(edge[0]<Capacity) {
            const auto& target=actual[edge[0]];
            std::printf("block_transport_convergence_target key=(%d,%d,%d,%u) state=(%u,%u,%u,%u) generation=%u\n",
                target.key.x,target.key.y,target.key.z,target.key.direction,
                target.state[0],target.state[1],target.state[2],target.state[3],target.extra[0]);
          }
          std::printf("block_transport_convergence_counters");for(unsigned c=0;c<stats.size();++c)std::printf(" c%u=%u",c,stats[c]);
          std::printf("\n");std::fflush(stdout);
          const unsigned sample_batch=surface.state[1]-1;
          const auto sampled=block_transport_samples(fixture,{{surface.key,{sample_batch,link,sample_batch,0}}});
          require(sampled.size()==1,"BT convergence diagnostic sample missing");const auto& ray=sampled[0];
          std::printf("block_transport_convergence_ray batch=%u slot=%u origin=(%.9g,%.9g,%.9g,%.9g) "
              "direction=(%.9g,%.9g,%.9g) direct_origin=(%.9g,%.9g,%.9g,%.9g) tmin=0.00025\n",
              sample_batch,link,ray.origin[0],ray.origin[1],ray.origin[2],ray.origin[3],
              ray.direction[0],ray.direction[1],ray.direction[2],ray.direct[0],ray.direct[1],ray.direct[2],ray.direct[3]);
          std::fflush(stdout);
        }
        require(valid,"BT convergence closed-room reference has unknown, uncached or stale energy gaps");
      }
    }
    const auto direct_error=error(read(direct),reference.sources),indirect_error=error(read(orders[3]),reference.indirect);
    observations<<batch<<','<<Faces<<','<<direct_error.mean<<','<<direct_error.rms<<','
        <<indirect_error.mean<<','<<indirect_error.rms<<','<<indirect_error.peak<<'\n';observations.flush();
    require(bool(observations),"BT convergence metric write");block_transport_complete(r,start);
    if(batch==1 || batch==4 || batch==16 || batch==64)
      std::printf("block_transport_convergence_batch batch=%u direct_mean_error=%.9g direct_rms_error=%.9g indirect_mean_error=%.9g indirect_rms_error=%.9g indirect_peak_error=%.9g\n",
          batch,direct_error.mean,direct_error.rms,indirect_error.mean,indirect_error.rms,indirect_error.peak);
    return {direct_error,indirect_error};
  }
};
}
void block_transport_convergence_cases(Fixture& f) {
  auto& r=f.renderer;const auto reference=transport_reference::integrate(r);
  unsigned stone=0;for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].id=="octaryn.basegame.block.stone")stone=i;
  require(stone!=0,"BT convergence stone material");open_world_renderer_set_center(&r,0,0,0);
  auto room=column();const auto previous=r.sources.find({0,0});
  room.revision=previous==r.sources.end()?1:previous->second.revision+1;
  for(int z=8;z<=13;++z)for(int y=0;y<=5;++y)for(int x=8;x<=13;++x)
    if(x==8 || x==13 || y==0 || y==5 || z==8 || z==13)put(room,x,y,z,std::uint16_t(stone));
  room.blocks.compact();require(open_world_renderer_update(&r,room),"BT convergence closed-room publication");settle(r);
  const auto diagnostic_sample=block_transport_samples(f,{{reference.surfaces[0],{0,0,0,0}}});
  require(diagnostic_sample.size()==1 && diagnostic_sample[0].origin[3]==1,
      "BT convergence diagnostic production sample path failed");
  Convergence probe(f,reference);std::pair<Error,Error> final{};
  for(unsigned batch=0;batch<64;++batch)final=probe.run();
  require(final.first.mean<.01 && final.first.rms<.025,"BT area direct lighting misses independent linear reference");
  require(final.second.mean<.01 && final.second.rms<.025 && final.second.peak<.06,
      "BT indirect lighting misses independent complete-room linear reference");
  require(r.debug.errors.load()==0,"BT convergence graphics validation errors");
  std::printf("block_transport_convergence=passed hardware=1 production_trace=1 production_direct=1 production_bounce=1 production_selection=1 independent_geometry=1 linear_reference=1 no_missing_rows=1 rows=%u batches=64 mean_error=%.9g rms_error=%.9g validation_errors=0\n",
      Faces,final.second.mean,final.second.rms);
}
}
