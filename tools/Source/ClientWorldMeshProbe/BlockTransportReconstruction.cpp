#include "BlockTransportSetup.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

namespace mesh_probe {
void block_transport_reconstruction_raster_cases(Fixture&);
namespace {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
constexpr unsigned Capacity=4096,GeometryEpoch=53,RadianceEpoch=71;
struct Query {Words key{};Pixel uv{};Words face{};Pixel local{};};
struct Result {Pixel reconstructed{},exact{};Words key{};Pixel uv{};};
struct Expected {Result result;bool address{};};
static_assert(sizeof(Query)==64 && sizeof(Result)==64);
Words packed(BlockSurfaceKey key) {
  return {std::bit_cast<unsigned>(key.x),std::bit_cast<unsigned>(key.y),std::bit_cast<unsigned>(key.z),key.direction+1};
}
BlockSurfaceKey offset(BlockSurfaceKey key,int u,int v) {
  if(key.direction<2){key.z+=u;key.y+=v;}
  else if(key.direction<4){key.x+=u;key.z+=v;}
  else {key.x+=u;key.y+=v;}
  return key;
}
Pixel field(double u,double v) {
  return {float(3+.7*u+.4*v+.2*u*v),float(4+.2*u+.6*v+.1*u*v),float(2+.1*u+.2*v+.3*u*v),1};
}
struct Cases {
  std::array<BlockTransportSurface,Capacity> surfaces{};
  std::array<Pixel,Capacity> indirect{},environment{};
  std::vector<Query> queries;
  std::vector<Expected> expected;
  unsigned insert(BlockSurfaceKey key,Pixel value,bool eligible=true) {
    const unsigned first=block_surface_hash(key)&(Capacity-1);
    for(unsigned attempt=0;attempt<BlockTransportProbes;++attempt) {
      const unsigned row=(first+attempt)&(Capacity-1);
      if(surfaces[row].state[0]!=0)continue;
      surfaces[row]={key,{1,1,1,eligible?1.f:0.f},{GeometryEpoch,1,1,1},{7,0,0,0}};
      for(unsigned c=0;c<3;++c){indirect[row][c]=value[c]*.75f;environment[row][c]=value[c]*.25f;}
      indirect[row][3]=environment[row][3]=std::bit_cast<float>(RadianceEpoch);return row;
    }
    require(false,"BTGI reconstruction fixture hash pool overflow");return 0;
  }
  void query(BlockSurfaceKey key,Pixel uv,Pixel reconstructed,Pixel exact) {
    queries.push_back({packed(key),uv,{},{}});Expected e;e.result.reconstructed=reconstructed;e.result.exact=exact;
    expected.push_back(e);
  }
  void address(Words face,Pixel local,Words key,Pixel uv) {
    Query q;q.face=face;q.local=local;queries.push_back(q);
    Expected e;e.address=true;e.result.key=key;e.result.uv=uv;expected.push_back(e);
  }
};
void interpolation_cases(Cases& cases) {
  for(unsigned direction=0;direction<6;++direction) {
    const BlockSurfaceKey center{16777217+int(direction)*64,-16777219-int(direction)*64,
        33554433+int(direction)*64,direction};
    const BlockSurfaceKey constant{-4096-int(direction)*64,128,4096+int(direction)*64,direction};
    for(int v=-1;v<=1;++v)for(int u=-1;u<=1;++u) {
      cases.insert(offset(center,u,v),field(u,v));cases.insert(offset(constant,u,v),{3,4,5,1});
    }
    for(float v:{0.f,.25f,.5f,.75f,1.f})for(float u:{0.f,.25f,.5f,.75f,1.f}) {
      cases.query(center,{u,v,0,0},field(double(u)-.5,double(v)-.5),field(0,0));
      cases.query(constant,{u,v,0,0},{3,4,5,1},{3,4,5,1});
    }
    // Neighboring owners must agree at their shared boundary.
    cases.query(offset(center,1,0),{0,.75f,0,0},field(.5,.25),field(1,0));
  }
}
enum class Fault {Missing,Opposing,Corner,Step,DiagonalOnly,Geometry,Indirect,Environment,Untraced,Fresh,Cutout,OwnerCutout};
void rejection_cases(Cases& cases) {
  unsigned serial=0;
  for(auto fault:{Fault::Missing,Fault::Opposing,Fault::Corner,Fault::Step,Fault::DiagonalOnly,
      Fault::Geometry,Fault::Indirect,Fault::Environment,Fault::Untraced,Fault::Fresh,Fault::Cutout,Fault::OwnerCutout}) {
    const int base=16384+int(serial++)*64;
    const BlockSurfaceKey owner{base,base,base,3};
    const Pixel exact{2,3,4,1};const unsigned own=cases.insert(owner,exact);
    if(fault==Fault::OwnerCutout)cases.surfaces[own].albedo[3]=-11;
    for(unsigned tap=1;tap<4;++tap) {
      if((fault==Fault::Missing && tap==3) || (fault==Fault::DiagonalOnly && tap!=3))continue;
      auto key=offset(owner,(tap&1)?1:0,(tap&2)?1:0);
      if(tap==3) {
        if(fault==Fault::Opposing)key.direction^=1;
        if(fault==Fault::Corner)key.direction=0;
        if(fault==Fault::Step)++key.y;
      }
      const unsigned row=cases.insert(key,{100,200,300,1});
      if(tap==3) {
        if(fault==Fault::Geometry)cases.surfaces[row].state[0]=GeometryEpoch-1;
        if(fault==Fault::Indirect)cases.indirect[row][3]=std::bit_cast<float>(RadianceEpoch-1);
        if(fault==Fault::Environment)cases.environment[row][3]=std::bit_cast<float>(RadianceEpoch-1);
        if(fault==Fault::Untraced)cases.surfaces[row].state[2]=UINT32_MAX;
        if(fault==Fault::Fresh)cases.surfaces[row].state[3]=0;
        if(fault==Fault::Cutout)cases.surfaces[row].albedo[3]=-11;
      }
    }
    cases.query(owner,{.75f,.75f,0,0},exact,exact);
  }
  const BlockSurfaceKey valid{32768,8,32768,3};const Pixel exact{2,3,4,1};
  cases.insert(valid,exact);
  for(Pixel uv:{Pixel{-1,-1,0,0},Pixel{2,0,0,0},Pixel{std::numeric_limits<float>::quiet_NaN(),.5f,0,0}})
    cases.query(valid,uv,exact,exact);
  for(int boundary:{std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}) {
    const BlockSurfaceKey edge{boundary,boundary,boundary,3};cases.insert(edge,exact);
    const float uv=boundary<0?0.f:1.f;cases.query(edge,{uv,uv,0,0},exact,exact);
  }
  cases.query({-99999,77,99999,3},{.75f,.75f,0,0},{},{});
  const BlockSurfaceKey partial{60000,8,60000,3};const unsigned row=cases.insert(partial,exact);
  cases.environment[row][3]=std::bit_cast<float>(RadianceEpoch-1);
  cases.query(partial,{.75f,.75f,0,0},{1.5f,2.25f,3,1},{1.5f,2.25f,3,1});
}
void address_cases(Cases& cases) {
  for(unsigned direction=0;direction<6;++direction) {
    const BlockSurfaceKey anchor{-16777219,33554433,16777217,direction};
    const unsigned normal=direction/2,u=normal==0?2:0,v=normal==1?2:1;
    Words face{std::bit_cast<unsigned>(anchor.x),std::bit_cast<unsigned>(anchor.y),
        std::bit_cast<unsigned>(anchor.z),(direction<<16)|(6u<<20)|(12u<<25)};
    for(unsigned edge=0;edge<2;++edge) {
      Pixel local{};local[normal]=float(direction&1);local[u]=edge?7.f:2.25f;local[v]=edge?13.f:5.75f;
      const auto owner=offset(anchor,edge?6:2,edge?12:5);const Pixel uv=edge?Pixel{1,1,0,0}:Pixel{.25f,.75f,0,0};
      cases.address(face,local,packed(owner),uv);
      const auto words=packed(owner);Words unit{words[0],words[1],words[2],direction<<16};
      local[u]=uv[0];local[v]=uv[1];cases.address(unit,local,packed(owner),uv);
    }
  }
  cases.address({0,0,0,6u<<16},{},{},{-1,-1,0,0});
  cases.address({0,0,0,0},{std::numeric_limits<float>::quiet_NaN(),0,0,0},{},{-1,-1,0,0});
  cases.address({0x7fffffffu,0,0,(2u<<16)|(31u<<20)}, {31,0,0,0},{},{-1,-1,0,0});
}
void run(WorldRenderer& r,Cases& cases) {
  const auto start=std::chrono::steady_clock::now();
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportReconstruction.slang").generic_string();
  require(block_transport_pipeline(r.device,path.c_str(),"main",pipeline),"BTGI production reconstruction probe");
  const auto read=rhi::BufferUsage::ShaderResource,rw=read|rhi::BufferUsage::UnorderedAccess;
  const auto surfaces=buffer(r,cases.surfaces.data(),sizeof(cases.surfaces),sizeof(BlockTransportSurface),read);
  const auto indirect=buffer(r,cases.indirect.data(),sizeof(cases.indirect),sizeof(Pixel),read);
  const auto environment=buffer(r,cases.environment.data(),sizeof(cases.environment),sizeof(Pixel),read);
  const auto queries=buffer(r,cases.queries.data(),cases.queries.size()*sizeof(Query),sizeof(Query),read);
  std::vector<Result> results(cases.queries.size());
  const auto output=buffer(r,results.data(),results.size()*sizeof(Result),sizeof(Result),rw);
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BTGI reconstruction commands");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BTGI reconstruction compute pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BTGI reconstruction pipeline bind");
  const rhi::ShaderCursor cursor(root);
  const auto bind=[&](const char* field,rhi::IBuffer* value){checked(cursor[field].setBinding(value),field);};
  bind("btLookupSurfaces",surfaces);bind("btLookupIndirect",indirect);bind("btLookupEnvironment",environment);
  bind("probeQueries",queries);bind("probeResults",output);
  const Words info{1,GeometryEpoch,RadianceEpoch,Capacity},probe{static_cast<unsigned>(results.size()),0,0,0};
  checked(cursor["btLookupInfo"].setData(info.data(),sizeof(info)),"BTGI reconstruction epochs");
  checked(cursor["probeInfo"].setData(probe.data(),sizeof(probe)),"BTGI reconstruction query count");
  pass->dispatchCompute((probe[0]+63)/64,1,1);pass->end();
  auto submission=commands->finish();require(bool(submission),"BTGI reconstruction finish");
  require(r.frame_queue.submit(r.queue,submission,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
      "BTGI reconstruction bounded completion");
  checked(r.device->readBuffer(output,0,results.size()*sizeof(Result),results.data()),"BTGI reconstruction readback");
  for(unsigned i=0;i<results.size();++i) {
    const auto& actual=results[i];const auto& expected=cases.expected[i];
    if(expected.address) {
      require(actual.key==expected.result.key,"BTGI local coordinates changed exact signed face identity");
      require(actual.uv==expected.result.uv,"BTGI merged/unit local UV mismatch or upper edge wrapped to zero");
    } else for(unsigned c=0;c<4;++c) {
      const auto close=[](float a,float b){return std::isfinite(a) && std::abs(a-b)<=2e-5f*std::max(1.f,std::abs(b));};
      if(!close(actual.reconstructed[c],expected.result.reconstructed[c]))
        std::fprintf(stderr,"block_transport_reconstruction_difference query=%u channel=%u actual=%.9g expected=%.9g\n",
            i,c,actual.reconstructed[c],expected.result.reconstructed[c]);
      require(close(actual.reconstructed[c],expected.result.reconstructed[c]),"BTGI reconstruction differs from analytic field or exact fallback");
      require(close(actual.exact[c],expected.result.exact[c]),"BTGI exact reflected-hit lookup changed");
    }
  }
  require(r.debug.errors.load()==0,"BTGI reconstruction graphics validation errors");
  block_transport_complete(r,start);
  std::printf("block_transport_reconstruction=passed hardware=1 production_lookup=1 integer_coordinates=1 face_directions=6 constant=1 bilinear=1 topology=1 freshness=1 cutout_exclusion=1 edge_uv=1 exact_lookup=1 queries=%u validation_errors=0\n",probe[0]);
}
}
void block_transport_reconstruction_cases(Fixture& fixture) {
  Cases cases;interpolation_cases(cases);rejection_cases(cases);address_cases(cases);run(fixture.renderer,cases);
  block_transport_reconstruction_raster_cases(fixture);
}
}
