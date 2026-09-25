#include "BlockTransportSetup.h"
#include "../../validation/block_transport_oracle.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <fstream>
#include <thread>

namespace mesh_probe {
void block_transport_cache_cases(Fixture&);
void block_transport_admission_cases(Fixture&);
void block_transport_raster_cases(Fixture&);
void block_transport_reconstruction_cases(Fixture&);
void block_transport_ray_cases(Fixture&);
void block_transport_reflection_cases(Fixture&);
void block_transport_history_cases(Fixture&);
void block_transport_select_cases(Fixture&);
void block_transport_sampling_cases(Fixture&);
void block_transport_convergence_cases(Fixture&);
void block_transport_dynamic_cases(Fixture&);
void block_transport_plant_cases(Fixture&);
void block_transport_leaf_cases(Fixture&);
void block_transport_actor_history_cases(Fixture&);
void block_transport_local_area_cases(Fixture&);
void block_transport_boundary_cases(Fixture&);
namespace world_admission {void room(Fixture&);}
namespace {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
using block_transport_oracle::Scene;
constexpr unsigned Epoch=19,GeometryEpoch=23,Generation=7,Slots=16;
using Surface=BlockTransportSurface;
enum class Fault {None,LinkEpoch,SurfaceEpoch,DirectEpoch,EnvironmentEpoch,AllEpoch,BounceEpoch,LinkGeneration,FreshDirect,FreshRow};
class SolveProbe {
  WorldRenderer& r;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  std::ofstream heartbeat{"frame-timing.csv",std::ios::app};
  std::chrono::steady_clock::time_point previous=std::chrono::steady_clock::now();
  std::uint64_t first_frame=0;
  unsigned checks=0;
  Slang::ComPtr<rhi::IBuffer> upload(const void* data,std::size_t bytes,unsigned stride,bool writable=false) {
    auto usage=rhi::BufferUsage::ShaderResource;
    if(writable)usage|=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopyDestination;
    return buffer(r,data,bytes,stride,usage);
  }
  void cap() {
    std::this_thread::sleep_until(previous+std::chrono::milliseconds(34));
    const auto now=std::chrono::steady_clock::now();
    heartbeat<<r.frames++<<','<<std::chrono::duration<double,std::milli>(now-previous).count()<<'\n';
    heartbeat.flush();require(bool(heartbeat),"BTGI oracle heartbeat write");previous=now;
  }
public:
  explicit SolveProbe(WorldRenderer& renderer):r(renderer) {
    require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Bounce.slang",
        "main",pipeline),"BTGI production bounce pipeline");
    first_frame=r.frames;previous=std::chrono::steady_clock::now();
  }
  unsigned count()const{return checks;}
  std::uint64_t first()const{return first_frame;}
  std::uint64_t frames()const{return r.frames-first_frame;}
  void run(const char* name,const Scene& input,Fault fault=Fault::None) {
    const unsigned count=static_cast<unsigned>(input.size());
    require(count>1,"BTGI oracle fixture needs multiple surfaces");
    Scene expected_scene=input;
    std::vector<Surface> surfaces(count);
    std::vector<Words> links(count*Slots);
    std::vector<Pixel> direct(count),environment(count),zero(count);
    for(unsigned i=0;i<count;++i) {
      auto& s=surfaces[i];s.key={int(i)-3,-17,33-int(i),i%6};
      s.state={input[i].valid?GeometryEpoch:GeometryEpoch-1,1,1,1};
      s.extra={Generation,1,1,0};
      require(input[i].targets.size()==Slots,"BTGI oracle denominator differs from production budget");
      for(unsigned c=0;c<3;++c) {
        s.albedo[c]=float(input[i].reflectance[c]);
        direct[i][c]=float(input[i].direct[c]*.75);
        environment[i][c]=float(input[i].direct[c]*.25);
      }
      direct[i][3]=environment[i][3]=std::bit_cast<float>(Epoch);
      for(unsigned slot=0;slot<Slots;++slot) {
        const int target=input[i].targets[slot];
        links[i*Slots+slot]=target<0?Words{0,0,0,0}:Words{unsigned(target),GeometryEpoch,Generation,1};
        if(fault==Fault::LinkEpoch && target==1) {
          links[i*Slots+slot][1]=GeometryEpoch-1;expected_scene[i].targets[slot]=-1;
        }
      }
      if(fault==Fault::LinkGeneration)for(unsigned slot=0;slot<Slots;++slot) {
        if(input[i].targets[slot]==1) {
          links[i*Slots+slot][2]=Generation-1;expected_scene[i].targets[slot]=-1;
        }
      }
      if(fault==Fault::FreshDirect || fault==Fault::FreshRow) {
        if(fault==Fault::FreshDirect)s.state[3]=0;
        else s.state[2]=UINT32_MAX;
        expected_scene[i].valid=false;
        for(unsigned c=0;c<3;++c)direct[i][c]=environment[i][c]=1000.f;
        zero[i]={1000.f,1000.f,1000.f,std::bit_cast<float>(Epoch)};
      }
      if(fault==Fault::DirectEpoch) {
        direct[i][3]=std::bit_cast<float>(Epoch-1);
        for(auto& c:expected_scene[i].direct)c*=.25;
      }
      if(fault==Fault::EnvironmentEpoch) {
        environment[i][3]=std::bit_cast<float>(Epoch-1);
        for(auto& c:expected_scene[i].direct)c*=.75;
      }
      if(fault==Fault::AllEpoch) {s.state[0]=GeometryEpoch-1;expected_scene[i].valid=false;}
    }
    if(fault==Fault::SurfaceEpoch) {surfaces[1].state[0]=GeometryEpoch-1;expected_scene[1].valid=false;}
    const auto expected=block_transport_oracle::evaluate(expected_scene);
    const auto surface_buffer=upload(surfaces.data(),surfaces.size()*sizeof(Surface),sizeof(Surface));
    const auto link_buffer=upload(links.data(),links.size()*sizeof(Words),sizeof(Words));
    const auto direct_buffer=upload(direct.data(),direct.size()*sizeof(Pixel),sizeof(Pixel));
    const auto environment_buffer=upload(environment.data(),environment.size()*sizeof(Pixel),sizeof(Pixel));
    const auto empty=upload(zero.data(),zero.size()*sizeof(Pixel),sizeof(Pixel));
    std::array<Slang::ComPtr<rhi::IBuffer>,4> orders;
    for(auto& order:orders)order=upload(zero.data(),zero.size()*sizeof(Pixel),sizeof(Pixel),true);
    for(unsigned order=0;order<4;++order) {
      auto commands=r.queue->createCommandEncoder();require(bool(commands),"BTGI oracle command encoder");
      if(order==1 && fault==Fault::BounceEpoch) {
        Pixel stale{};for(unsigned c=0;c<3;++c)stale[c]=float(expected[1].outgoing[0][c]);
        stale[3]=std::bit_cast<float>(Epoch-1);
        checked(commands->uploadBufferData(orders[0],sizeof(Pixel),sizeof(Pixel),stale.data()),"BTGI stale bounce injection");
        commands->setBufferState(orders[0],rhi::ResourceState::ShaderResource);
      }
      commands->setBufferState(orders[order],rhi::ResourceState::UnorderedAccess);
      auto* pass=commands->beginComputePass();require(pass!=nullptr,"BTGI oracle compute pass");
      auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BTGI oracle pipeline binding");
      const rhi::ShaderCursor cursor(root);
      const auto bind=[&](const char* field,rhi::IBuffer* value) {
        if(cursor[field].isValid())checked(cursor[field].setBinding(value),field);
      };
      bind("btSurfaces",surface_buffer);bind("btLinks",link_buffer);
      bind("btDirect",direct_buffer);bind("btEnvironment",environment_buffer);
      bind("btB0",order>0?orders[0].get():empty.get());
      bind("btB1",order>1?orders[1].get():empty.get());
      bind("btB2",order>2?orders[2].get():empty.get());
      bind("btOutput",orders[order]);
      const Words options{Epoch,count,order,Slots};
      checked(cursor["btSolve"].setData(options.data(),sizeof(options)),"BTGI oracle solve options");
      const Words frame{GeometryEpoch,1,count,Slots};
      checked(cursor["btFrameInfo"].setData(frame.data(),sizeof(frame)),"BTGI independent geometry epoch");
      pass->dispatchCompute((count+63)/64,1,1);pass->end();
      commands->setBufferState(orders[order],rhi::ResourceState::ShaderResource);
      auto submission=commands->finish();require(bool(submission),"BTGI oracle command finish");
      require(r.frame_queue.submit(r.queue,submission,r.active_frame) &&
          r.frame_queue.wait(r.active_frame,2000),"BTGI oracle bounded completion");
      std::vector<Pixel> actual(count);
      checked(r.device->readBuffer(orders[order],0,actual.size()*sizeof(Pixel),actual.data()),"BTGI oracle readback");
      for(unsigned i=0;i<count;++i)for(unsigned c=0;c<3;++c) {
        double wanted=order<3?expected[i].outgoing[order][c]:expected[i].indirect[c];
        if(fault==Fault::BounceEpoch && order>0)wanted=0;
        const double error=std::abs(double(actual[i][c])-wanted);
        if(!std::isfinite(actual[i][c]) || error>2e-5*std::max(1.,std::abs(wanted)))
          std::fprintf(stderr,"block_transport_difference case=%s order=%u surface=%u channel=%u actual=%.9g expected=%.12g\n",
              name,order,i,c,actual[i][c],wanted);
        require(std::isfinite(actual[i][c]) && error<=2e-5*std::max(1.,std::abs(wanted)),
            "BTGI production gather differs from independent path enumeration");
        ++checks;
      }
      cap();
    }
    std::printf("block_transport_oracle_case=%s status=passed\n",name);
  }
};
}
void block_transport_oracle_cases(Fixture& fixture) {
  SolveProbe probe(fixture.renderer);
  probe.run("three_surface_chain",block_transport_oracle::chain());
  const auto mixed=block_transport_oracle::mixed();
  probe.run("colored_cycles",mixed);
  auto half=block_transport_oracle::chain();half.resize(2);
  half[1].direct={8,4,2};half[1].targets.assign(Slots,-1);
  for(unsigned i=Slots/2;i<Slots;++i)half[0].targets[i]=-1;
  probe.run("unknown_slots_keep_denominator",half);
  probe.run("stale_link_geometry_epoch",mixed,Fault::LinkEpoch);
  probe.run("recycled_slot_link_generation",mixed,Fault::LinkGeneration);
  probe.run("fresh_slot_poisoned_direct_history",mixed,Fault::FreshDirect);
  probe.run("fresh_slot_poisoned_untraced_row",mixed,Fault::FreshRow);
  probe.run("stale_surface_geometry_epoch",mixed,Fault::SurfaceEpoch);
  probe.run("stale_direct_keeps_environment",mixed,Fault::DirectEpoch);
  probe.run("stale_environment_keeps_direct",mixed,Fault::EnvironmentEpoch);
  probe.run("global_epoch_rejection",mixed,Fault::AllEpoch);
  probe.run("stale_bounce_epoch",half,Fault::BounceEpoch);
  auto constant=mixed;
  for(unsigned i=0;i<constant.size();++i) {
    constant[i].reflectance={1,1,1};constant[i].direct={2,3,4};
    constant[i].targets.assign(Slots,int((i+1)%constant.size()));
  }
  probe.run("per_order_constant_radiance",constant);
  for(auto& surface:constant)surface.direct={0,0,0};
  probe.run("source_removal_sealed_dark",constant);
  require(fixture.renderer.debug.errors.load()==0,"BTGI numerical GPU validation errors");
  std::printf("block_transport_probe=passed hardware=1 production_bounce=1 path_oracle=1 scalar_checks=%u first_frame=%llu oracle_frames=%llu source_removal=1 denominator=1 stale_epochs=1 validation_errors=0\n",probe.count(),
      static_cast<unsigned long long>(probe.first()),static_cast<unsigned long long>(probe.frames()));
}
void block_transport_cases(Fixture& fixture,const BlockTransportGroup& group) {
  require(fixture.renderer.frames==0,"BTGI fixture frame origin");
  BlockTransportSetup setup(fixture.renderer,group);
  {std::ofstream heartbeat("frame-timing.csv");heartbeat<<"frame,total_ms\n";
    heartbeat.flush();require(bool(heartbeat),"BTGI heartbeat initialization");}
  const std::string_view name=group.name;
  if(name=="cache") {block_transport_cache_cases(fixture);block_transport_select_cases(fixture);}
  else if(name=="admission")block_transport_admission_cases(fixture);
  else if(name=="room")world_admission::room(fixture);
  else if(name=="raster") {block_transport_raster_cases(fixture);block_transport_reconstruction_cases(fixture);}
  else if(name=="rays")block_transport_ray_cases(fixture);
  else if(name=="plants")block_transport_plant_cases(fixture);
  else if(name=="numerical") {
    block_transport_reflection_cases(fixture);block_transport_history_cases(fixture);block_transport_oracle_cases(fixture);
  } else if(name=="sampling")block_transport_sampling_cases(fixture);
  else if(name=="convergence")block_transport_convergence_cases(fixture);
  else if(name=="dynamic")block_transport_dynamic_cases(fixture);
  else if(name=="leaf")block_transport_leaf_cases(fixture);
  else if(name=="actor")block_transport_actor_history_cases(fixture);
  else if(name=="local-area")block_transport_local_area_cases(fixture);
  else if(name=="boundary")block_transport_boundary_cases(fixture);
  else require(false,"BTGI unknown fixture group");
  setup.verify();
  require(fixture.renderer.debug.errors.load()==0,"BTGI group graphics validation errors");
  require(fixture.renderer.frames>0,"BTGI group did not execute any paced frames");
  std::printf("block_transport_group=passed name=%s frames=%llu validation_errors=0\n",group.name,
      static_cast<unsigned long long>(fixture.renderer.frames));
}

}
