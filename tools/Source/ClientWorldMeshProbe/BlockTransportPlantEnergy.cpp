#include "BlockTransportSetup.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"
#include <slang-rhi/shader-cursor.h>
#include <bit>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <thread>

namespace mesh_probe {
namespace {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
constexpr unsigned Capacity=128,GeometryEpoch=101,RadianceEpoch=103,Generation=11,Slots=16;
struct EnergyCase {const char* name;bool cycle,remove_red;};
struct EnergyScene {
  std::array<BlockTransportSurface,Capacity> surfaces{};
  std::array<Words,Capacity*Slots> links{};
  std::array<Pixel,Capacity> direct{},environment{},poison{};
  std::array<unsigned,4> rows{};
  std::array<Words,3> queries{};
  explicit EnergyScene(unsigned layer,const EnergyCase& test) {
    const BlockSurfaceKey keys[]{{-17,9,16777217,6u|(layer<<4)},
        {-17,9,16777217,7u|(layer<<4)},{-20,9,16777217,1},{-14,9,16777217,0}};
    for(unsigned i=0;i<4;++i) {
      unsigned row=block_surface_hash(keys[i])&(Capacity-1),probe=0;
      while(surfaces[row].state[0]!=0 && probe++<BlockTransportProbes)row=(row+1)&(Capacity-1);
      require(probe<BlockTransportProbes,"BTGI plant energy hash fixture overflow");rows[i]=row;
      surfaces[row]={keys[i],{}, {GeometryEpoch,1,1,1},{Generation,1,1,0}};
      for(unsigned c=0;c<3;++c)surfaces[row].albedo[c]=i<2?.5f:.25f;
      if(i<2)queries[i]={std::bit_cast<unsigned>(keys[i].x),std::bit_cast<unsigned>(keys[i].y),
          std::bit_cast<unsigned>(keys[i].z),keys[i].direction+1};
    }
    queries[2]=queries[0];queries[2][3]=(8u|(layer<<4))+1;
    for(unsigned row=0;row<Capacity;++row) {
      direct[row][3]=environment[row][3]=std::bit_cast<float>(RadianceEpoch);
      poison[row]={1000.f,2000.f,3000.f,std::bit_cast<float>(RadianceEpoch)};
    }
    direct[rows[2]][0]=test.remove_red?0.f:4.f;direct[rows[3]][2]=8.f;
    for(unsigned side=0;side<2;++side)for(unsigned slot=0;slot<Slots;++slot) {
      links[rows[side]*Slots+slot]={rows[side+2],GeometryEpoch,Generation,1};
      if(test.cycle)links[rows[side+2]*Slots+slot]={rows[side],GeometryEpoch,Generation,1};
    }
  }
};
class EnergyProbe {
  WorldRenderer& r;
  Slang::ComPtr<rhi::IComputePipeline> bounce,lookup;
  std::ofstream heartbeat{"frame-timing.csv",std::ios::app};
  std::chrono::steady_clock::time_point previous=std::chrono::steady_clock::now();
  unsigned checks=0;
  Slang::ComPtr<rhi::IBuffer> upload(const void* data,std::size_t bytes,unsigned stride,bool writable=false) {
    auto usage=rhi::BufferUsage::ShaderResource;
    if(writable)usage|=rhi::BufferUsage::UnorderedAccess;
    return buffer(r,data,bytes,stride,usage);
  }
  void finish(rhi::ICommandEncoder* commands) {
    auto submission=commands->finish();require(bool(submission),"BTGI plant energy finish");
    require(r.frame_queue.submit(r.queue,submission,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
        "BTGI plant energy bounded completion");
    std::this_thread::sleep_until(previous+std::chrono::milliseconds(34));
    const auto now=std::chrono::steady_clock::now();
    heartbeat<<r.frames++<<','<<std::chrono::duration<double,std::milli>(now-previous).count()<<'\n';
    heartbeat.flush();require(bool(heartbeat),"BTGI plant energy heartbeat write");previous=now;
  }
  void compare(float actual,double expected) {
    require(std::isfinite(actual) && std::abs(double(actual)-expected)<1e-6,
        "BTGI plant sides mixed, lost a path, reused removed light, or counted energy twice");++checks;
  }
public:
  explicit EnergyProbe(WorldRenderer& renderer):r(renderer) {
    require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Bounce.slang",
        "main",bounce),"BTGI plant energy production bounce pipeline");
    const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportPlantEnergy.slang").generic_string();
    require(block_transport_pipeline(r.device,path.c_str(),"main",lookup),"BTGI plant exact energy lookup pipeline");
    previous=std::chrono::steady_clock::now();
  }
  unsigned count()const{return checks;}
  void run(unsigned layer,const EnergyCase& test) {
    const EnergyScene scene(layer,test);
    const auto surfaces=upload(scene.surfaces.data(),sizeof(scene.surfaces),sizeof(BlockTransportSurface));
    const auto links=upload(scene.links.data(),sizeof(scene.links),sizeof(Words));
    const auto direct=upload(scene.direct.data(),sizeof(scene.direct),sizeof(Pixel));
    const auto environment=upload(scene.environment.data(),sizeof(scene.environment),sizeof(Pixel));
    const auto empty=upload(scene.poison.data(),sizeof(scene.poison),sizeof(Pixel));
    std::array<Slang::ComPtr<rhi::IBuffer>,4> orders;
    for(auto& order:orders)order=upload(scene.poison.data(),sizeof(scene.poison),sizeof(Pixel),true);
    for(unsigned order=0;order<4;++order) {
      auto commands=r.queue->createCommandEncoder();require(bool(commands),"BTGI plant energy commands");
      commands->setBufferState(orders[order],rhi::ResourceState::UnorderedAccess);
      auto* pass=commands->beginComputePass();require(pass!=nullptr,"BTGI plant energy pass");
      auto* root=pass->bindPipeline(bounce);require(root!=nullptr,"BTGI plant energy bounce binding");
      const rhi::ShaderCursor cursor(root);
      const auto bind=[&](const char* name,rhi::IBuffer* value) {checked(cursor[name].setBinding(value),name);};
      bind("btSurfaces",surfaces);bind("btLinks",links);bind("btDirect",direct);bind("btEnvironment",environment);
      bind("btB0",order>0?orders[0].get():empty.get());
      bind("btB1",order>1?orders[1].get():empty.get());
      bind("btB2",order>2?orders[2].get():empty.get());bind("btOutput",orders[order]);
      const Words solve{RadianceEpoch,Capacity,order,Slots},frame{GeometryEpoch,1,Capacity,Slots};
      checked(cursor["btSolve"].setData(solve.data(),sizeof(solve)),"BTGI plant energy solve");
      checked(cursor["btFrameInfo"].setData(frame.data(),sizeof(frame)),"BTGI plant energy geometry");
      pass->dispatchCompute((Capacity+63)/64,1,1);pass->end();
      commands->setBufferState(orders[order],rhi::ResourceState::ShaderResource);finish(commands);
      std::array<Pixel,Capacity> actual{};
      checked(r.device->readBuffer(orders[order],0,sizeof(actual),actual.data()),"BTGI plant energy order readback");
      for(unsigned node=0;node<4;++node)for(unsigned channel=0;channel<3;++channel) {
        const bool red=(node%2)==0,plant=node<2;
        const double source=red?(test.remove_red?0.:1.):2.;
        double weight=0;
        // Closed-form paths: source, one plant reflection, one return reflection.
        if(order==0 && !plant)weight=1;
        if(order==1 && plant)weight=.5;
        if(order==2 && !plant && test.cycle)weight=.125;
        if(order==3)weight=plant?(test.cycle?1.125:1.):(test.cycle?.5:0.);
        compare(actual[scene.rows[node]][channel],channel==(red?0u:2u)?source*weight:0.);
      }
    }
    const auto keys=upload(scene.queries.data(),sizeof(scene.queries),sizeof(Words));
    std::array<Pixel,3> actual{};
    const auto results=upload(actual.data(),sizeof(actual),sizeof(Pixel),true);
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BTGI plant energy lookup commands");
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"BTGI plant energy lookup pass");
    auto* root=pass->bindPipeline(lookup);require(root!=nullptr,"BTGI plant energy lookup bind");
    const rhi::ShaderCursor cursor(root);
    const auto bind=[&](const char* name,rhi::IBuffer* value) {checked(cursor[name].setBinding(value),name);};
    bind("btLookupSurfaces",surfaces);bind("btLookupIndirect",orders[3]);bind("btLookupEnvironment",environment);
    bind("energyKeys",keys);bind("energyResults",results);
    const Words info{1,GeometryEpoch,RadianceEpoch,Capacity};
    checked(cursor["btLookupInfo"].setData(info.data(),sizeof(info)),"BTGI plant energy lookup epochs");
    pass->dispatchCompute(3,1,1);pass->end();finish(commands);
    checked(r.device->readBuffer(results,0,sizeof(actual),actual.data()),"BTGI plant energy lookup readback");
    for(unsigned side=0;side<3;++side)for(unsigned channel=0;channel<4;++channel) {
      double wanted=0;
      if(side<2 && channel==3)wanted=1;
      if(side<2 && channel==(side==0?0u:2u))
        wanted=(side==0?(test.remove_red?0.:1.):2.)*(test.cycle?1.125:1.);
      compare(actual[side][channel],wanted);
    }
    std::printf("block_transport_plant_energy_case=%s status=passed\n",test.name);
  }
};
}
void block_transport_plant_energy_cases(Fixture& fixture,unsigned layer) {
  require(layer<29 && (layer<17 || layer>23),"BTGI plant energy fixture material");
  EnergyProbe probe(fixture.renderer);
  const EnergyCase cases[]{{"separate_colored_paths",false,false},{"separate_return_cycles",true,false},
      {"red_source_removed",true,true}};
  for(const auto& test:cases)probe.run(layer,test);
  require(fixture.renderer.debug.errors.load()==0,"BTGI plant energy GPU validation errors");
  std::printf("block_transport_plant_energy=passed hardware=1 production_bounce=1 exact_lookup=1 opposite_sides=1 colored_paths=1 path_oracle=1 source_removal=1 no_double_count=1 cases=3 scalar_checks=%u validation_errors=0\n",probe.count());
}
}
