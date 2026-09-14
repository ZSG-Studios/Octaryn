#include "Probe.h"
#include "BlockEditService.h"
#include "BlockCommandQueue.h"
#include "BlockChangeQueue.h"
namespace fluid_probe {
void check_apply(const FluidRules& r) {
  BlockStore store;const BlockPosition lake{-33,-40,-65},dry{-32,-40,-65};
  BlockEditPolicy policy;
  policy.generated_block=[&](const auto& p){return p==lake?r.water[0]:AirBlock;};
  policy.is_known_block=[&](uint16_t b){return b==AirBlock || r.kind(b)!=FluidKind::None || b==r.stone || r.is_replaceable(b);};
  policy.can_apply_edit=[](const auto&,uint16_t){return true;};
  policy.can_stay_supported=[&](uint16_t b,const auto&,uint16_t below){return b!=r.replaceable[0] || r.is_solid(below);};
  auto result=apply_block_edit(store,{lake,AirBlock},policy);
  require(result.result.applied && result.result.changed && result.changes.size()==1,"generated water removal applies explicit air");
  uint16_t block=65535;require(store.try_get_block(lake,block) && block==AirBlock,"air override retained over generated water");
  BlockStore restored;restored.load(store.snapshot());require(restored.try_get_block(lake,block) && block==AirBlock,"snapshot/load retains explicit air");
  result=apply_block_edit(store,{lake,r.water[0]},policy);require(result.result.changed && !store.try_get_block(lake,block),"restoring generated water removes override");
  result=apply_block_edit(store,{dry,r.water[5]},policy);require(result.result.applied && result.result.changed,"internal apply accepts known simulation level");
  result=apply_block_edit(store,{dry,r.water[5]},policy);require(result.result.applied && !result.result.changed && result.changes.empty(),"same effective fluid is no-op");
  result=apply_block_edit(store,{dry,AirBlock},policy);require(result.result.changed && !store.try_get_block(dry,block),"restoring generated air removes fluid override");
  require(apply_block_edit(store,{dry,r.stone},policy).result.changed,"install support");
  const BlockPosition plant{dry.x,dry.y+1,dry.z};require(apply_block_edit(store,{plant,r.replaceable[0]},policy).result.changed,"install supported vegetation");
  result=apply_block_edit(store,{dry,r.lava[3]},policy);
  require(result.result.changed && result.changes.size()==2 && result.changes[1].position==plant && result.changes[1].block==AirBlock,"simulation edit retains native support cascade");
  BlockChangeQueue changes;changes.enqueue_all(result.changes);ReplicationChange output[2]{};uint32_t written=99;
  require(changes.drain(output,1,123,written)==-1 && written==0 && changes.pending_count()==2,"insufficient replication capacity retains entire change set");
  require(changes.drain(output,2,123,written)==0 && written==2 && changes.pending_count()==0,"primary and cascade drain together");
  for(size_t i=0;i<2;++i) {
    const auto expected=to_replication_change(result.changes[i],123);
    require(output[i].payload0==expected.payload0 && output[i].payload1==expected.payload1 && output[i].replication_id==123,"replication preserves exact edit order and signed coordinates");
  }
  ClientBlockCommandQueue commands;BlockCommandQueuePolicy client;
  client.is_client_placeable=[&](uint16_t b){return b==r.stone || b==r.water[0];};
  client.can_apply=[&](const auto& command){return can_apply_block_command(store,command,policy);};
  octaryn_host_command command{};command.version=HostCommandVersion;command.size=OCTARYN_HOST_COMMAND_SIZE;
  command.kind=HostCommandSetBlockKind;command.a=dry.x;command.b=dry.y;command.c=dry.z;command.d=r.water[5];size_t rejected=99;
  require(commands.submit(&command,1,client,rejected)!=0 && rejected==0 && commands.pending_count()==0,"client cannot place simulation-only level");
  command.d=r.water[0];require(commands.submit(&command,1,client,rejected)==0 && commands.pending_count()==1,"client source placement remains accepted");
  require(!apply_block_edit(store,{{0,WorldMinY-1,0},r.water[1]},policy).result.applied,"internal apply enforces signed Y bounds");
}
}
