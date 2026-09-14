#include "Probe.h"
#include "FluidSimulation.h"
#include "BlockChangeQueue.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <iostream>

namespace fluid_probe {
namespace {
using Handle=std::unique_ptr<void,decltype(&octaryn_server_fluid_destroy)>;
struct Config {
  std::vector<uint16_t> replaceable,solid;
  octaryn_server_fluid_config value{};
  explicit Config(const FluidRules& rules):replaceable(rules.replaceable),solid(rules.solid) {
    value.version=1;value.size=sizeof(value);value.stone=rules.stone;
    std::copy(rules.water.begin(),rules.water.end(),value.water);
    std::copy(rules.lava.begin(),rules.lava.end(),value.lava);
    value.replaceable_count=static_cast<uint32_t>(replaceable.size());value.replaceable=replaceable.data();
    value.solid_count=static_cast<uint32_t>(solid.size());value.solid=solid.data();
  }
};
struct Context {const FluidRules& rules;bool stone_world{},floor{};};
uint16_t generated(void* opaque,const octaryn_server_block_position* position) {
  const auto& c=*static_cast<Context*>(opaque);
  return c.stone_world || (c.floor && position->y<=0)?c.rules.stone:AirBlock;
}
uint32_t known(void*,uint16_t) {return 1;}
uint32_t allowed(void*,const octaryn_server_block_edit*,uint16_t) {return 1;}
uint32_t supported(void*,uint16_t block,const octaryn_server_block_position*,uint16_t below) {
  return block!=9 || below!=AirBlock;
}
int advance(void* simulation,BlockStore& store,BlockChangeQueue* queue,double dt,
    Context& context,octaryn_server_fluid_tick_report* report) {
  return octaryn_server_fluid_tick(simulation,&store,queue,dt,generated,known,allowed,supported,&context,report);
}
Handle copied_config(const FluidRules& rules) {
  Config config(rules);
  Handle simulation(octaryn_server_fluid_create(&config.value),octaryn_server_fluid_destroy);
  require(simulation!=nullptr,"valid ABI config creates owner");
  std::fill(std::begin(config.value.water),std::end(config.value.water),0);
  std::fill(std::begin(config.value.lava),std::end(config.value.lava),0);
  std::fill(config.replaceable.begin(),config.replaceable.end(),0);
  std::fill(config.solid.begin(),config.solid.end(),0);
  return simulation; // Original arrays and vectors now destroyed as well as overwritten.
}
void configurations(const FluidRules& rules) {
  Config c(rules);
  const auto rejects=[](const octaryn_server_fluid_config* config) {
    Handle result(octaryn_server_fluid_create(config),octaryn_server_fluid_destroy);
    require(!result,"malformed fluid ABI configuration accepted");
  };
  rejects(nullptr);
  auto bad=c.value;bad.version=2;rejects(&bad);
  bad=c.value;--bad.size;rejects(&bad);
  bad=c.value;bad.replaceable=nullptr;rejects(&bad);
  bad=c.value;bad.solid=nullptr;rejects(&bad);
  bad=c.value;bad.replaceable_count=65536;rejects(&bad);
  bad=c.value;bad.solid_count=65536;rejects(&bad);
  bad=c.value;bad.water[7]=bad.lava[2];rejects(&bad);
  const uint16_t duplicates[]={rules.stone,rules.stone};
  bad=c.value;bad.solid=duplicates;bad.solid_count=2;rejects(&bad);
  bad=c.value;bad.replaceable=nullptr;bad.replaceable_count=0;
  Handle empty(octaryn_server_fluid_create(&bad),octaryn_server_fluid_destroy);
  require(empty!=nullptr,"zero-length replaceable table accepts null pointer");
  require(octaryn_server_fluid_set_region(nullptr,0,0,0)==-1,"null ABI owner rejected");
  auto simulation=copied_config(rules);require(octaryn_server_fluid_set_region(simulation.get(),0,0,0)==0,"ABI region setup");
  require(octaryn_server_fluid_notify(simulation.get(),nullptr,0)==0 &&
      octaryn_server_fluid_notify(simulation.get(),nullptr,1)==-1,"notification pointer/count contract");
  BlockStore store;Context context{rules,true};
  const BlockPosition plant{16,3,16};
  store.set_block({plant,rules.replaceable[0]});store.set_block({{16,4,16},rules.water[0]});
  const BlockPosition pool{4,3,4};
  store.set_block({pool,AirBlock},true);store.set_block({{3,3,4},rules.water[0]});store.set_block({{5,3,4},rules.water[0]});
  const auto source=to_abi_block_edit({{3,3,4},rules.water[0]});
  require(octaryn_server_fluid_notify(simulation.get(),&source,1)==0,"notify copied solid-support fixture");
  const BlockPosition lava_target{24,3,24};
  store.set_block({lava_target,rules.replaceable[0]});store.set_block({{24,4,24},rules.lava[0]});
  const auto lava=to_abi_block_edit({{24,4,24},rules.lava[0]});
  require(octaryn_server_fluid_notify(simulation.get(),&lava,1)==0,"notify copied lava table");
  const auto edit=to_abi_block_edit({{16,4,16},rules.water[0]});
  require(octaryn_server_fluid_notify(simulation.get(),&edit,1)==0,"notify copied configuration");
  octaryn_server_fluid_tick_report report{};
  require(advance(simulation.get(),store,nullptr,.25,context,&report)==0,"tick copied owner");
  require(store.get_block(plant)==rules.water[1],"ABI retains copied fluid IDs and replaceable table after caller destruction");
  require(store.get_block(pool)==rules.water[0],"ABI retains copied solid-support table for source creation");
  require(advance(simulation.get(),store,nullptr,.25,context,&report)==0 && store.get_block(lava_target)==rules.lava[1],
      "ABI retains copied lava IDs after caller destruction");
}
void invalid_time(const FluidRules& rules) {
  auto simulation=copied_config(rules);BlockStore store;Context context{rules};
  octaryn_server_fluid_tick_report report{};
  require(advance(simulation.get(),store,nullptr,.125,context,&report)==0 && report.now_ms==125,"ABI accumulated time");
  for(double invalid:{-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
    report.now_ms=987;
    require(advance(simulation.get(),store,nullptr,invalid,context,&report)==-1 && report.now_ms==987,"invalid timestep rejects without output mutation");
    require(advance(simulation.get(),store,nullptr,0,context,&report)==0 && report.now_ms==125,"invalid timestep leaves accumulated clock unchanged");
  }
  require(advance(simulation.get(),store,nullptr,.25,context,nullptr)==-1,"missing report rejected");
  require(advance(simulation.get(),store,nullptr,0,context,&report)==0 && report.now_ms==125,"missing report cannot advance clock");
  require(advance(simulation.get(),store,nullptr,1,context,&report)==0 && report.now_ms==375,"large timestep capped at 250ms");
  require(report.version==1 && report.size==sizeof(report),"ABI output layout contract");
}
void cascades(const FluidRules& rules,bool retain_delta) {
  auto simulation=copied_config(rules);octaryn_server_fluid_set_region(simulation.get(),0,0,0);
  Context context{rules,true};BlockStore store;
  const BlockPosition fluid{8,3,8},plant{8,4,8};
  store.set_block({fluid,rules.water[3]});store.set_block({plant,9});
  const auto edit=to_abi_block_edit({fluid,rules.water[3]});octaryn_server_fluid_notify(simulation.get(),&edit,1);
  auto queue=std::make_unique<BlockChangeQueue>();
  if(retain_delta) for(size_t i=0;i<MaxPendingBlockChanges-1;++i)
    require(queue->enqueue({{static_cast<int32_t>(i),0,0},rules.stone}),"ABI saturation fixture fill");
  octaryn_server_fluid_tick_report report{};
  require(advance(simulation.get(),store,retain_delta?queue.get():nullptr,.25,context,&report)==0,"ABI cascade service");
  if(retain_delta) {
    require(report.changed==0 && report.capacity_deferrals==1 && store.get_block(fluid)==rules.water[3] &&
        store.get_block(plant)==9 && queue->pending_count()==8191,"ABI cascade defers atomically with one slot");
    std::vector<ReplicationChange> old(8191);uint32_t written{};
    require(queue->drain(old.data(),8191,1,written)==0 && written==8191,"drain ABI backpressure fixture");
    require(advance(simulation.get(),store,queue.get(),.25,context,&report)==0,"ABI cascade retry");
  }
  require(report.changed==2 && report.apply_attempts==1 && store.get_block(fluid)==AirBlock && store.get_block(plant)==AirBlock,
      "ABI changed count includes primary and support cascade");
  require(queue->pending_count()==(retain_delta?2u:0u),"null delta queue leaves no retained replication");
  uint16_t value=65535;
  require(store.try_get_block(fluid,value) && value==AirBlock && store.try_get_block(plant,value) && value==AirBlock,
      "ABI cascade preserves explicit air over generated terrain");
}
void edited_outlet(const FluidRules& rules) {
  for(const auto kind:{FluidKind::Water,FluidKind::Lava}) {
    auto simulation=copied_config(rules);octaryn_server_fluid_set_region(simulation.get(),0,0,0);
    Context context{rules,false,true};BlockStore store;
    const auto policy=policy_from_abi(generated,known,allowed,supported,&context);
    const BlockPosition source{16,1,16},hole{18,0,16};
    const auto edit=[&](BlockPosition p,uint16_t block) {
      const auto result=apply_block_edit_and_enqueue(store,nullptr,{p,block},policy);
      require(result.result.changed,"ABI outlet fixture commits authoritative edit");
      for(const auto& change:result.changes) {
        const auto native=to_abi_block_edit(change);
        require(octaryn_server_fluid_notify(simulation.get(),&native,1)==0,
            "ABI outlet edit notification carries only the resulting block");
      }
    };
    edit(hole,AirBlock);edit(source,rules.make(kind,0));
    octaryn_server_fluid_tick_report report{};
    for(int i=0;i<120;++i)
      require(advance(simulation.get(),store,nullptr,.25,context,&report)==0,"ABI routed flow ticks");
    require(get_effective_block(store,{15,1,16},policy)==AirBlock,"ABI initial slope excludes opposite direction");
    edit(hole,rules.stone);
    for(int i=0;i<120;++i)
      require(advance(simulation.get(),store,nullptr,.25,context,&report)==0,"ABI plugged outlet ticks");
    for(int x=-8;x<=8;++x) for(int z=-8;z<=8;++z) {
      const int distance=std::abs(x)+std::abs(z);
      require(get_effective_block(store,{16+x,1,16+z},policy)==
          (distance<=7?rules.make(kind,distance):AirBlock),"ABI terrain edit restores complete outward footprint");
    }
    require(store.block_count()==113,"ABI stores only fluid overrides, not generated floor blocks");
    BlockStore reloaded;reloaded.load(store.snapshot());
    require(reloaded.block_count()==113 && reloaded.get_block({15,1,15})==rules.make(kind,2),
        "native snapshot/load retains authoritative diagonal flow");
  }
}
}
void check_fluid_abi(const FluidRules& rules) {
  configurations(rules);invalid_time(rules);cascades(rules,false);cascades(rules,true);edited_outlet(rules);
  std::cout<<"fluid_abi=passed configuration=passed ownership=passed invalid_time=passed cascade=passed replication=passed\n";
}
}
