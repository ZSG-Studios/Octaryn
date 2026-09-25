#pragma once
#include "BlockTransportActorHistory.h"
namespace mesh_probe::actor_history {
inline void bounds(Fixture& fixture) {
  struct Query {Words previous;Pixel origin,direction;};static_assert(sizeof(Query)==48);
  std::vector<Query> queries;std::vector<BlockTransportSurface> surfaces;std::vector<double> expected;
  for(unsigned face=0;face<10;++face) {
    const unsigned tag=face<6?face:(11u<<4)|face;
    Vector n{};Pixel center{10.5f,2.5f,10.5f,0};
    if(face<6){n[face/2]=face%2?1.f:-1.f;center[face/2]+=n[face/2]*.5f;}
    else {const float sign=face%2?-1.f:1.f;n={(face<8?-1:1)*sign*.7071067811865475f,0,sign*.7071067811865475f};}
    surfaces.push_back({{10,2,10,tag},{.5f,.5f,.5f,face<6?1.f:0.f},{Epoch,1,1,16},{7,0,0,1}});
    Query q{{face,Epoch,7,1},center,{}};
    for(unsigned c=0;c<3;++c){q.origin[c]+=n[c]*2;q.direction[c]=-n[c];}
    queries.push_back(q);expected.push_back(2);
  }
  auto q=queries.front();q.previous[2]=6;queries.push_back(q);expected.push_back(-1);
  q=queries.front();q.previous[1]=Epoch-1;queries.push_back(q);expected.push_back(-1);
  q=queries.front();q.direction={0,1,0,0};queries.push_back(q);expected.push_back(-1);
  q={{std::bit_cast<unsigned>(1.75f),Epoch,0,4},{10,1,10,0},{0,1,0,0}};
  queries.push_back(q);expected.push_back(1.75);
  q.previous[0]=0x7fc00000u;queries.push_back(q);expected.push_back(-1);
  q.previous={0,0,0,2};queries.push_back(q);expected.push_back(31.002);
  q.previous={0,0,0,0};queries.push_back(q);expected.push_back(-1);
  q=queries.front();q.origin[0]=std::nextafter(10.f,-INFINITY);
  queries.push_back(q);expected.push_back(10.-q.origin[0]);
  q.previous={std::bit_cast<unsigned>(.00005f),Epoch,0,4};
  queries.push_back(q);expected.push_back(.00005f);
  auto& r=fixture.renderer;Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportActorHistory.slang").generic_string();
  require(block_transport_pipeline(r.device,path.c_str(),"main",pipeline),"BT actor endpoint prepared production helper");
  auto input=buffer(r,queries.data(),queries.size()*sizeof(Query),sizeof(Query),rhi::BufferUsage::ShaderResource);
  auto targets=buffer(r,surfaces.data(),surfaces.size()*sizeof(surfaces[0]),sizeof(surfaces[0]),rhi::BufferUsage::ShaderResource);
  std::vector<Pixel> actual(queries.size());auto output=buffer(r,actual.data(),actual.size()*sizeof(Pixel),sizeof(Pixel),rhi::BufferUsage::UnorderedAccess);
  const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
  require(r.frame_queue.wait(r.active_frame,2000),"BT actor endpoint frame reuse");
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT actor endpoint encoder");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT actor endpoint pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT actor endpoint binding");const rhi::ShaderCursor cursor(root);
  const Words frame{Epoch,1,unsigned(surfaces.size()),16};const Pixel low{0,0,0,1},high{32,32,32,0},settings{1,64,.001f,0};
  const unsigned count=unsigned(queries.size());
  checked(cursor["btSurfaces"].setBinding(targets),"BT actor endpoint target surfaces");
  checked(cursor["btFrameInfo"].setData(frame.data(),sizeof(frame)),"BT actor endpoint epochs");
  checked(cursor["btCoverageMin"].setData(low.data(),sizeof(low)),"BT actor endpoint coverage minimum");
  checked(cursor["btCoverageMax"].setData(high.data(),sizeof(high)),"BT actor endpoint coverage maximum");
  checked(cursor["raySettings"].setData(settings.data(),sizeof(settings)),"BT actor endpoint ray limit");
  checked(cursor["actorHistoryQueries"].setBinding(input),"BT actor endpoint inputs");
  checked(cursor["actorHistoryResults"].setBinding(output),"BT actor endpoint results");
  checked(cursor["actorHistoryCount"].setData(&count,sizeof(count)),"BT actor endpoint count");
  pass->dispatchCompute(1,1,1);pass->end();plant_probe::submit(r,commands);
  checked(r.device->readBuffer(output,0,actual.size()*sizeof(Pixel),actual.data()),"BT actor endpoint readback");
  for(unsigned i=0;i<count;++i) {
    require(actual[i][1]==(expected[i]<0?0.f:1.f),"BT actor stale, invalid or unknown endpoint accepted");
    if(expected[i]>=0)require(std::isfinite(actual[i][0]) &&
        std::abs(actual[i][0]-expected[i])<std::min(1e-4,std::max(1e-8,expected[i]*1e-5)),
        "BT actor endpoint does not match independent cube/plant plane");
  }
  block_transport_complete(r,start);
  std::puts("block_transport_actor_bounds=passed production_helper=1 cube_faces=6 plant_sides=4 stale_generation=1 stale_epoch=1 parallel=1 stored_world_limit=1 nonfinite=1 certified_sky=1 unknown_zero=1 near_world=1 near_actor=1 cases=19");
}
}
