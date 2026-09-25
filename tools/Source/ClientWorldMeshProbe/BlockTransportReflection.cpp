#include "BlockTransportSetup.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <thread>

namespace mesh_probe {
namespace {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
constexpr unsigned Capacity=128,GeometryEpoch=67,RadianceEpoch=79;
struct Query {Words key{};Pixel material{},parameters{},normal{};};
struct Result {Pixel incident{},primary{},sun{},full_sun{};};
struct Expected {Pixel incident{},primary{};};
static_assert(sizeof(Query)==64 && sizeof(Result)==64);
enum class Fault {None,Missing,Geometry,Untraced,Fresh,DirectMissing,DirectEpoch,EnvironmentEpoch,IndirectEpoch,InvalidTag};
struct Cases {
  std::array<BlockTransportSurface,Capacity> surfaces{};
  std::array<Pixel,Capacity> indirect{},environment{},direct{};
  std::vector<Query> queries;
  std::vector<Expected> expected;
  void add(unsigned tag,Fault fault=Fault::None,bool back_face=false,float roughness=.5f,float metallic=0,
      unsigned address=0,float scale=1) {
    const BlockSurfaceKey key{address?int(address):1000+int(queries.size())*64,-17,16777217,tag};
    const Pixel gi{scale,scale*2,scale*3,0},sky{.25f,.5f,.75f,0},source{2.5f,1.5f,4.5f,0};
    const bool resident=fault!=Fault::Missing && fault!=Fault::InvalidTag;
    const bool row_valid=resident && fault!=Fault::Geometry && fault!=Fault::Untraced && fault!=Fault::Fresh;
    const bool direct_valid=fault!=Fault::DirectMissing && fault!=Fault::DirectEpoch;
    const bool gi_valid=fault!=Fault::IndirectEpoch,sky_valid=fault!=Fault::EnvironmentEpoch;
    if(resident) {
      unsigned row=UINT32_MAX;
      for(unsigned attempt=0;attempt<BlockTransportProbes;++attempt) {
        const unsigned candidate=(block_surface_hash(key)+attempt)&(Capacity-1);
        if(surfaces[candidate].state[0]==0){row=candidate;break;}
      }
      require(row!=UINT32_MAX,"BTGI reflection fixture hash overflow");
      surfaces[row]={key,{.2f,.4f,.6f,1},{GeometryEpoch,1,1,1},{7,1,1,0}};
      if(fault==Fault::Geometry)surfaces[row].state[0]=GeometryEpoch-1;
      if(fault==Fault::Untraced)surfaces[row].state[2]=UINT32_MAX;
      if(fault==Fault::Fresh)surfaces[row].state[3]=0;
      indirect[row]=gi;environment[row]=sky;direct[row]=source;
      indirect[row][3]=std::bit_cast<float>(gi_valid?RadianceEpoch:RadianceEpoch-1);
      environment[row][3]=std::bit_cast<float>(sky_valid?RadianceEpoch:RadianceEpoch-1);
      direct[row][3]=std::bit_cast<float>(direct_valid?RadianceEpoch:RadianceEpoch-1);
      if(fault==Fault::DirectMissing)direct[row]={};
    }
    Query query;query.key={std::bit_cast<unsigned>(key.x),std::bit_cast<unsigned>(key.y),
        std::bit_cast<unsigned>(key.z),tag+1};
    query.material={.2f,.4f,.6f,metallic};query.parameters={roughness,2,back_face?1.f:0.f,0};
    const unsigned axis=tag<6?tag/2:2;query.normal[axis]=(tag&1)?1.f:-1.f;
    Expected e;
    if(row_valid) {
      for(unsigned c=0;c<3;++c)e.primary[c]=(gi_valid?gi[c]:0)+(sky_valid?sky[c]:0);
      e.primary[3]=1;
      if(!back_face || tag>=6) {
        e.incident=e.primary;e.incident[3]=gi_valid && sky_valid && direct_valid?1.f:0.f;
        if(e.incident[3]!=0)for(unsigned c=0;c<3;++c)e.incident[c]+=source[c];
      }
    }
    queries.push_back(query);expected.push_back(e);
  }
};
void execute(Fixture& fixture,Cases& cases) {
  auto& r=fixture.renderer;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportReflection.slang").generic_string();
  require(block_transport_pipeline(r.device,path.c_str(),"main",pipeline),"BTGI reflected incident pipeline");
  const auto upload=[&](const void* bytes,std::size_t size,unsigned stride) {
    return buffer(r,bytes,size,stride,rhi::BufferUsage::ShaderResource);
  };
  const auto surfaces=upload(cases.surfaces.data(),sizeof(cases.surfaces),sizeof(BlockTransportSurface));
  const auto direct=upload(cases.direct.data(),sizeof(cases.direct),sizeof(Pixel));
  const auto environment=upload(cases.environment.data(),sizeof(cases.environment),sizeof(Pixel));
  const auto indirect=upload(cases.indirect.data(),sizeof(cases.indirect),sizeof(Pixel));
  const auto queries=upload(cases.queries.data(),cases.queries.size()*sizeof(Query),sizeof(Query));
  std::vector<Result> actual(cases.queries.size());
  const auto results=buffer(r,actual.data(),actual.size()*sizeof(Result),sizeof(Result),
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess);
  unsigned checks=0;
  for(unsigned active:{1u,0u}) {
    const auto start=std::chrono::steady_clock::now();
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BTGI reflection commands");
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"BTGI reflection pass");
    auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BTGI reflection binding");
    const rhi::ShaderCursor cursor(root);
    const auto bind=[&](const char* name,rhi::IBuffer* value) {checked(cursor[name].setBinding(value),name);};
    bind("btLookupSurfaces",surfaces);bind("btLookupDirect",direct);
    bind("btLookupEnvironment",environment);bind("btLookupIndirect",indirect);
    bind("probeQueries",queries);bind("probeResults",results);
    const Words info{active,GeometryEpoch,RadianceEpoch,Capacity};
    const unsigned count=static_cast<unsigned>(cases.queries.size());
    checked(cursor["btLookupInfo"].setData(info.data(),sizeof(info)),"BTGI reflection epochs");
    checked(cursor["probeCount"].setData(&count,sizeof(count)),"BTGI reflection query count");
    pass->dispatchCompute((count+63)/64,1,1);pass->end();
    auto submission=commands->finish();require(bool(submission),"BTGI reflection finish");
    require(r.frame_queue.submit(r.queue,submission,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
        "BTGI reflection bounded completion");
    checked(r.device->readBuffer(results,0,actual.size()*sizeof(Result),actual.data()),"BTGI reflection readback");
    for(unsigned i=0;i<count;++i) {
      const auto& query=cases.queries[i];const auto& a=actual[i];
      const Expected e=active?cases.expected[i]:Expected{};
      for(unsigned c=0;c<4;++c) {
        require(std::abs(a.incident[c]-e.incident[c])<1e-5f && std::abs(a.primary[c]-e.primary[c])<1e-5f,
            "BTGI reflected incident lost direct light, used stale history, or changed primary GI");++checks;
      }
      // At normal incidence G=1, Fresnel=F0 and GGX D=1/(pi*alpha^2).
      const double alpha=std::max(double(query.parameters[0])*query.parameters[0],.0025),pi=std::acos(-1.);
      for(unsigned c=0;c<3;++c) {
        const double metallic=query.material[3],albedo=query.material[c],f0=.04*(1-metallic)+albedo*metallic;
        const double specular=f0/(4*pi*alpha*alpha)*query.parameters[1];
        const double diffuse=(1-f0)*albedo*(1-metallic)/pi*query.parameters[1];
        const double wanted=specular+(e.incident[3]!=0?0:diffuse),full=specular+diffuse;
        require(std::isfinite(a.sun[c]) && std::abs(double(a.sun[c])-wanted)<3e-5*std::max(1.,wanted) &&
            std::abs(double(a.full_sun[c])-full)<3e-5*std::max(1.,full),
            "BTGI cached diffuse duplicated sun or removed the secondary GGX specular lobe");++checks;
      }
      if(e.incident[3]==0)require(a.sun==a.full_sun,"BTGI cache fallback changed default direct BRDF");
    }
    block_transport_complete(r,start);
  }
  require(r.debug.errors.load()==0,"BTGI reflection validation errors");
  std::printf("block_transport_reflection=passed hardware=1 production_lookup=1 local_direct=1 sun_once=1 specular_preserved=1 stale_fallback=1 backface_policy=1 plant_sides=1 primary_unchanged=1 default_unchanged=1 scalar_checks=%u validation_errors=0\n",checks);
}
}
void block_transport_reflection_cases(Fixture& fixture) {
  Cases cases;
  for(unsigned direction=0;direction<6;++direction)cases.add(direction);
  for(auto fault:{Fault::Missing,Fault::Geometry,Fault::Untraced,Fault::Fresh,Fault::DirectMissing,
      Fault::DirectEpoch,Fault::EnvironmentEpoch,Fault::IndirectEpoch})cases.add(5,fault);
  cases.add(5,Fault::None,true);
  cases.add(6u|(11u<<4),Fault::None,false,.5f,0,2048,2);
  cases.add(7u|(11u<<4),Fault::None,true,.5f,0,2048,3);
  cases.add(6u|(17u<<4),Fault::InvalidTag,true);
  for(float roughness:{.25f,.5f,1.f})for(float metallic:{0.f,.5f,1.f})cases.add(3,Fault::None,false,roughness,metallic);
  execute(fixture,cases);
}
}
