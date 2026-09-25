#include "BlockTransportActorHistory.h"
#include "BlockTransportActorBounds.h"
namespace mesh_probe {
void block_transport_actor_history_cases(Fixture& fixture) {
  using namespace actor_history;auto& r=fixture.renderer;
  bounds(fixture);std::vector<BlockTransportSampleQuery> queries;
  for(unsigned batch=0;batch<2;++batch)for(unsigned slot=0;slot<16;++slot)queries.push_back({Receiver,{batch,slot,batch,0}});
  const auto rays=block_transport_samples(fixture,queries);
  const Vector clear=placement(rays,false),covered=placement(rays,true),far{20,20,20};
  Vector behind{};bool found=false;
  for(unsigned slot=0;slot<16 && !found;++slot)if(blocked(rays[slot],covered))
    for(float extra:{2.f,4.f,8.f,16.f}) {
      for(unsigned c=0;c<3;++c)behind[c]=rays[slot].origin[c]+rays[slot].direction[c]*float(world_limit(rays[slot])+extra);
      if(mask(rays,0,behind)==0){found=true;break;}
    }
  require(found,"BT actor behind-wall control crosses a valid previous path");
  const auto make=[&](Vector position){return BlockTransportPlayerBlocker(r,position[0]-11,position[1]-3,position[2]-11);};
  auto clear_actor=make(clear),clear_moved=make(moved(clear)),covered_actor=make(covered),covered_moved=make(moved(covered)),
      distant_actor=make(far),behind_actor=make(behind);
  unsigned stone=0;for(unsigned i=1;i<fixture.catalog.size();++i)if(fixture.catalog[i].id=="octaryn.basegame.block.stone")stone=i;
  require(stone!=0,"BT actor room stone material");open_world_renderer_set_center(&r,0,0,0);
  auto room=column();const auto previous=r.sources.find({0,0});room.revision=previous==r.sources.end()?1:previous->second.revision+1;
  for(int z=8;z<=13;++z)for(int y=0;y<=5;++y)for(int x=8;x<=13;++x)
    if(x==8 || x==13 || y==0 || y==5 || z==8 || z==13)put(room,x,y,z,std::uint16_t(stone));
  room.blocks.compact();require(open_world_renderer_update(&r,room),"BT actor room publication");plant_probe::settle(r);
  struct Case {const char* name;BlockTransportPlayerBlocker* first;Vector first_position;
    BlockTransportPlayerBlocker* second;Vector second_position;bool enabled,reset;unsigned mutation,rechecks;};
  const Case cases[]{
    {"old_clear_new_random_hit",&clear_actor,clear,&clear_moved,moved(clear),true,false,0,16},
    {"unchanged_old_blocked_paths",&covered_actor,covered,&covered_moved,moved(covered),true,false,0,16},
    {"actor_enters_old_path",&distant_actor,far,&covered_actor,covered,true,true,0,16},
    {"actor_leaves_old_path",&covered_actor,covered,&distant_actor,far,true,true,0,16},
    {"actor_removed",&covered_actor,covered,&covered_actor,covered,false,true,0,0},
    {"actor_behind_old_world_endpoint",&covered_actor,covered,&behind_actor,behind,true,true,0,16},
    {"unknown_paths_do_not_reset",&clear_actor,clear,&clear_moved,moved(clear),true,false,1,0},
    {"stale_target_fails_closed",&distant_actor,far,&distant_actor,far,true,true,2,15},
  };
  for(const auto& test:cases) {
    Probe probe(r);probe.run("seed",test.first->scene,true,1,mask(rays,0,test.first_position),true);
    probe.run(test.name,test.second->scene,test.enabled,2,test.enabled?mask(rays,1,test.second_position):0,
        false,test.reset,test.mutation,test.rechecks);
  }
  require(r.debug.errors.load()==0,"BT actor history graphics validation errors");
  std::puts("block_transport_actor_history=passed hardware=1 production_trace=1 production_direct=1 production_selection=1 same_previous_rays=1 moving_actor=1 real_disocclusion=1 unknown_denominator=1 bounded_player_queries=1 unchanged_world_ray_budget=1 cases=8 validation_errors=0");
}
}
