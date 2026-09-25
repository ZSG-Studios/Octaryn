#include "BlockTransportSetup.h"
#include "../../validation/block_transport_oracle.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"
#include <slang-rhi/shader-cursor.h>
#include <bit>
#include <cmath>
#include <limits>

namespace mesh_probe {
namespace {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
using Color=block_transport_oracle::Color;
using Scene=block_transport_oracle::Scene;
constexpr unsigned Capacity=8,Links=16;
using Expected=std::array<Pixel,Capacity>;
Pixel value(Color color,unsigned epoch) {
  return {float(color[0]),float(color[1]),float(color[2]),std::bit_cast<float>(epoch)};
}
Color estimate(unsigned batch) {return batch&1?Color{2,.25,0}:Color{0,.75,2};}
Color average(unsigned count) {
  // Closed-form weights: an initial mean, then a geometric tail after batch 16.
  Color result{};
  for(unsigned batch=1;batch<=count;++batch) {
    const double weight=count<=16?1./count:batch<=16?
        std::pow(15./16,count-16)/16:std::pow(15./16,count-batch)/16;
    const auto sample=estimate(batch);
    for(unsigned c=0;c<3;++c)result[c]+=sample[c]*weight;
  }
  return result;
}
class HistoryProbe {
  WorldRenderer& r;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  Slang::ComPtr<rhi::IBuffer> surface_buffer,link_buffer,direct_buffer,environment_buffer,zero;
  std::array<Slang::ComPtr<rhi::IBuffer>,4> orders;
  std::array<Pixel,Capacity> poison{};
  bool inject=false;
  unsigned checks=0,frames=0;
  Slang::ComPtr<rhi::IBuffer> allocate(const void* bytes,std::size_t size,unsigned stride,bool output=false) {
    auto usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination;
    if(output)usage|=rhi::BufferUsage::UnorderedAccess;
    return buffer(r,bytes,size,stride,usage);
  }
public:
  unsigned geometry=211,radiance=223,frame=100;
  Scene scene{Capacity};
  std::array<BlockTransportSurface,Capacity> surfaces{};
  explicit HistoryProbe(WorldRenderer& renderer):r(renderer) {
    require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Bounce.slang",
        "main",pipeline),"BTGI history retained production pipeline");
    scene[0].reflectance=scene[4].reflectance={.25,.5,.75};
    scene[1].reflectance={.5,.25,.125};scene[2].reflectance={.125,.75,.5};
    scene[3].reflectance={.25,.5,.75};
    for(unsigned row=0;row<Capacity;++row) {
      scene[row].targets.assign(Links,-1);scene[row].valid=row<5;
      if(row>=5)continue;
      auto& surface=surfaces[row];surface.key={int(row)-4,-19,33-int(row),row%6};
      surface.state={geometry,0,frame,1};surface.extra={7,0,0,0};
      for(unsigned c=0;c<3;++c)surface.albedo[c]=float(scene[row].reflectance[c]);
    }
    scene[1].targets.assign(Links,3);scene[2].targets.assign(Links,3);sources(true);graph(1);
    std::array<Words,Capacity*Links> links{};std::array<Pixel,Capacity> empty{};
    surface_buffer=allocate(surfaces.data(),sizeof(surfaces),sizeof(BlockTransportSurface));
    link_buffer=allocate(links.data(),sizeof(links),sizeof(Words));
    direct_buffer=allocate(empty.data(),sizeof(empty),sizeof(Pixel));
    environment_buffer=allocate(empty.data(),sizeof(empty),sizeof(Pixel));
    zero=allocate(empty.data(),sizeof(empty),sizeof(Pixel));
    for(auto& output:orders)output=allocate(empty.data(),sizeof(empty),sizeof(Pixel),true);
  }
  void sources(bool enabled) {
    scene[1].direct={enabled?4.:0.,0,0};scene[2].direct={0,0,enabled?4.:0.};
    scene[3].direct={0,enabled?2.:0.,0};
  }
  void graph(unsigned batch) {
    for(unsigned row:{0u,4u})scene[row].targets.assign(Links,batch&1?1:2);
  }
  void next(unsigned count,unsigned refreshed=31) {
    ++frame;
    for(unsigned row=0;row<5;++row)if(refreshed&(1u<<row)) {
      surfaces[row].state[2]=frame;surfaces[row].state[3]=count;
    }
  }
  Expected current()const {
    Expected result{};const auto paths=block_transport_oracle::evaluate(scene);
    for(unsigned row=0;row<5;++row)result[row]=value(paths[row].indirect,radiance);
    return result;
  }
  void poison_history(unsigned mask=255,bool nonfinite=false) {
    checked(r.device->readBuffer(orders[3],0,sizeof(poison),poison.data()),"BTGI preserve unpoisoned history");
    for(unsigned row=0;row<Capacity;++row)if(mask&(1u<<row))
      poison[row]={nonfinite?std::numeric_limits<float>::quiet_NaN():999.f,888,777,std::bit_cast<float>(radiance)};
    inject=true;
  }
  void run(const Expected& expected) {
    const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BTGI history frame reuse");
    std::array<Words,Capacity*Links> links{};std::array<Pixel,Capacity> direct{},environment{};
    Scene raw=scene;
    for(unsigned row=0;row<Capacity;++row) {
      raw[row].valid=surfaces[row].state[0]==geometry && surfaces[row].state[2]!=UINT32_MAX && surfaces[row].state[3]>0;
      direct[row]=value(scene[row].direct,radiance);environment[row][3]=std::bit_cast<float>(radiance);
      for(unsigned slot=0;slot<Links;++slot)if(scene[row].targets[slot]>=0) {
        const unsigned target=unsigned(scene[row].targets[slot]);
        links[row*Links+slot]={target,geometry,surfaces[target].extra[0],1};
      }
    }
    const auto paths=block_transport_oracle::evaluate(raw);
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BTGI history commands");
    const auto upload=[&](rhi::IBuffer* target,const void* bytes,std::size_t size) {
      checked(commands->uploadBufferData(target,0,size,bytes),"BTGI history frame inputs");
      commands->setBufferState(target,rhi::ResourceState::ShaderResource);
    };
    upload(surface_buffer,surfaces.data(),sizeof(surfaces));upload(link_buffer,links.data(),sizeof(links));
    upload(direct_buffer,direct.data(),sizeof(direct));upload(environment_buffer,environment.data(),sizeof(environment));
    if(inject) {upload(orders[3],poison.data(),sizeof(poison));inject=false;}
    for(unsigned order=0;order<4;++order) {
      commands->setBufferState(orders[order],rhi::ResourceState::UnorderedAccess);
      auto* pass=commands->beginComputePass();require(pass!=nullptr,"BTGI history compute pass");
      auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BTGI history production binding");
      const rhi::ShaderCursor cursor(root);
      const auto bind=[&](const char* name,rhi::IBuffer* resource) {checked(cursor[name].setBinding(resource),name);};
      bind("btSurfaces",surface_buffer);bind("btLinks",link_buffer);bind("btDirect",direct_buffer);bind("btEnvironment",environment_buffer);
      bind("btB0",order>0?orders[0].get():zero.get());bind("btB1",order>1?orders[1].get():zero.get());
      bind("btB2",order>2?orders[2].get():zero.get());bind("btOutput",orders[order]);
      const Words solve{radiance,Capacity,order,Links},info{geometry,frame,Capacity,Links};
      checked(cursor["btSolve"].setData(solve.data(),sizeof(solve)),"BTGI history order");
      checked(cursor["btFrameInfo"].setData(info.data(),sizeof(info)),"BTGI history frame identity");
      pass->dispatchCompute(1,1,1);pass->end();commands->globalBarrier();
      commands->setBufferState(orders[order],rhi::ResourceState::ShaderResource);
    }
    auto submission=commands->finish();require(bool(submission),"BTGI history finish");
    require(r.frame_queue.submit(r.queue,submission,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
        "BTGI history bounded completion");
    for(unsigned order=0;order<4;++order) {
      std::array<Pixel,Capacity> actual{};
      checked(r.device->readBuffer(orders[order],0,sizeof(actual),actual.data()),"BTGI history readback");
      for(unsigned row=0;row<Capacity;++row) {
        const Pixel wanted=order==3?expected[row]:raw[row].valid?value(paths[row].outgoing[order],radiance):Pixel{};
        for(unsigned c=0;c<3;++c) {
          if(!std::isfinite(actual[row][c]) || std::abs(actual[row][c]-wanted[c])>2e-5f)
            std::fprintf(stderr,"block_transport_history_difference frame=%u order=%u row=%u channel=%u actual=%g expected=%g\n",
                frame,order,row,c,actual[row][c],wanted[c]);
          require(std::isfinite(actual[row][c]) && std::abs(actual[row][c]-wanted[c])<=2e-5f,
              "BTGI history differs from independent weighted path enumeration");++checks;
        }
        require(std::bit_cast<unsigned>(actual[row][3])==std::bit_cast<unsigned>(wanted[3]),"BTGI history validity epoch");++checks;
      }
    }
    ++frames;block_transport_complete(r,start);
  }
  void finish()const {
    require(frames==29 && checks==3712,"BTGI bounded history case coverage");
    require(r.debug.errors.load()==0,"BTGI history graphics validation errors");
    std::printf("block_transport_history=passed hardware=1 production_bounce=1 world_rows=1 alternating_graphs=1 raw_orders=1 held_batches=1 count_cap=1 source_removal=1 radiance_epoch=1 slot_reuse=1 geometry_reset=1 finite_history=1 frames=%u scalar_checks=%u validation_errors=0\n",frames,checks);
  }
};
}
void block_transport_history_cases(Fixture& f) {
  HistoryProbe probe(f.renderer);Expected held{};
  for(unsigned batch=1;batch<=16;++batch) {
    probe.next(batch);probe.graph(batch);auto expected=probe.current();
    expected[0]=expected[4]=value(average(batch),probe.radiance);probe.run(expected);held=expected;
  }
  for(unsigned repeat=0;repeat<3;++repeat) {
    probe.next(16,14);probe.graph(repeat+1);probe.run(held);
  }
  probe.next(16,15);probe.graph(17);auto expected=probe.current();
  expected[0]=value(average(17),probe.radiance);expected[4]=held[4];probe.run(expected);
  probe.next(16);probe.graph(18);expected=probe.current();expected[0]=value(average(18),probe.radiance);
  const auto previous=average(16),blue=estimate(18);Color delayed{};
  for(unsigned c=0;c<3;++c)delayed[c]=(15*previous[c]+blue[c])/16;
  expected[4]=value(delayed,probe.radiance);probe.run(expected);
  ++probe.radiance;probe.sources(false);probe.next(1,15);expected=probe.current();expected[4]={};probe.run(expected);
  ++probe.radiance;probe.sources(true);probe.next(16,15);expected=probe.current();expected[4]={};probe.run(expected);
  probe.poison_history();probe.next(1);++probe.surfaces[0].extra[0];probe.surfaces[0].key.x-=100;
  probe.surfaces[4].state[3]=0;expected=probe.current();expected[4]={};probe.run(expected);
  probe.poison_history(17);probe.next(16);probe.surfaces[4].state[2]=UINT32_MAX;
  expected=probe.current();expected[4]={};probe.surfaces[0].state[3]=1;probe.run(expected);
  ++probe.geometry;probe.poison_history();probe.next(16);probe.run({});
  ++probe.radiance;probe.next(1);
  for(unsigned row=0;row<5;++row) {probe.surfaces[row].state[0]=probe.geometry;probe.surfaces[row].extra[0]=1;}
  probe.run(probe.current());
  probe.poison_history(255,true);probe.next(16);probe.run(probe.current());
  probe.poison_history(255,true);probe.next(16,0);probe.run({});probe.finish();
}
}
