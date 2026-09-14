#include "Probe.h"
#include "FluidScheduler.h"
#include "BlockEditService.h"
#include <map>
#include <optional>
#include <tuple>
#include <iostream>
#include <limits>

namespace fluid_probe {
namespace {
using Key=std::tuple<int32_t,int32_t,int32_t>;
struct World {
  std::map<Key,uint16_t> blocks;
  std::optional<BlockPosition> unknown;
  std::vector<BlockEdit> applied;
  unsigned reads{},attempts{};
  bool retry{};
  uint16_t floor_block{};
  uint16_t get(BlockPosition p) const {
    auto it=blocks.find({p.x,p.y,p.z});
    return it==blocks.end()?(p.y<=0?floor_block:AirBlock):it->second;
  }
  void set(BlockPosition p,uint16_t b) {blocks[{p.x,p.y,p.z}]=b;}
  FluidRead reader() {return [this](BlockPosition p,uint16_t& b) {
    ++reads;if(unknown && p==*unknown) return false;b=get(p);return true;
  };}
  FluidApply writer() {return [this](BlockPosition p,uint16_t expected,uint16_t next) {
    ++attempts;if(retry || get(p)!=expected) return FluidApplyResult::Retry;
    if(expected==next) return FluidApplyResult::Unchanged;
    set(p,next);applied.push_back({p,next});return FluidApplyResult::Applied;
  };}
};
FluidTickBudget budget() {
  FluidTickBudget b;b.max_time_us=0;b.repair_samples=0;b.repair_schedules=0;return b;
}
FluidTickReport tick(FluidScheduler& scheduler,World& world,uint64_t now,FluidTickBudget b=budget()) {
  const unsigned before=world.reads;
  const auto r=scheduler.tick(now,world.reader(),world.writer(),b);
  require(r.evaluations<=b.evaluations && r.applies<=b.applies && r.reads<=b.reads &&
      r.repair_samples<=b.repair_samples && r.repair_schedules<=b.repair_schedules,
      "scheduler exceeds explicit service budget");
  require(r.reads==world.reads-before,"reported read budget includes actual callback reads");
  require(r.pending==scheduler.pending_count() && r.pending<=MaxPendingFluids,"reported queue bound");
  return r;
}
void timing(const FluidRules& rules) {
  const BlockPosition p{16,4,16},below{16,3,16};
  for(const auto kind:{FluidKind::Water,FluidKind::Lava}) {
    FluidScheduler scheduler(rules);require(scheduler.set_region({0,0,0}),"timing region");World w;
    w.set(p,rules.make(kind,0));scheduler.notify_change(p,AirBlock,w.get(p),1000);
    const uint64_t delay=kind==FluidKind::Water?250:500;
    tick(scheduler,w,1000+delay-1);
    require(w.applied.empty(),"fluid cannot flow before original kind-specific due time");
    tick(scheduler,w,1000+delay);
    require(w.get(below)==rules.make(kind,1),"fluid falls exactly at original due time");
    const auto applied=w.applied.size();tick(scheduler,w,1000+delay);
    require(w.applied.size()==applied,"same clock cannot accelerate delayed continuation");
  }
  FluidScheduler order(rules);require(order.set_region({0,0,0}),"ordering region");World w;
  const BlockPosition first{3,5,3},second{7,5,3},third{11,5,3};
  for(auto p:{third,second,first}) {w.set(p,rules.water[2]);require(order.schedule(p,100),"schedule order fixture");}
  require(order.schedule(first,200) && order.schedule(third,50) && order.pending_count()==3,"earliest due dedupe");
  tick(order,w,49);require(w.applied.empty(),"explicit deadline does not fire early");
  tick(order,w,50);tick(order,w,100);
  require(w.applied.size()==3 && w.applied[0].position==third && w.applied[1].position==first &&
      w.applied[2].position==second,"due then signed coordinate order is deterministic");
}
void retries_and_budgets(const FluidRules& rules) {
  const BlockPosition p{8,4,8};
  for(bool unknown:{false,true}) {
    FluidScheduler scheduler(rules);scheduler.set_region({0,0,0});World w;w.set(p,rules.water[2]);
    require(scheduler.schedule(p,0),"retry schedule");
    if(unknown) w.unknown=p;else w.retry=true;
    const auto deferred=tick(scheduler,w,0);
    require(deferred.retries==1 && w.get(p)==rules.water[2],"unavailable/apply-backpressure defers unchanged");
    w.unknown.reset();w.retry=false;const auto attempts=w.attempts;
    tick(scheduler,w,249);require(w.attempts==attempts && w.get(p)==rules.water[2],"retry does not hotloop before 250ms");
    tick(scheduler,w,250);require(w.get(p)==AirBlock && w.applied.size()==1,"retry commits exactly once");
  }
  for(unsigned which=0;which<3;++which) {
    FluidScheduler scheduler(rules);scheduler.set_region({0,0,0});World w;
    for(int x:{4,8,12}) {const BlockPosition at{x,5,4};w.set(at,rules.water[3]);scheduler.schedule(at,0);}
    auto b=budget();if(which==0)b.evaluations=1;else if(which==1)b.applies=1;else b.reads=1;
    const auto r=tick(scheduler,w,0,b);
    require(r.budget_exhausted,"limited service reports exhausted budget");
    require(which==2?w.applied.empty():w.applied.size()==1,"per-kind work budget bounds mutation");
    for(uint64_t now=250;now<=1000;now+=250) {tick(scheduler,w,now);if(w.applied.size()==3)break;}
    require(w.applied.size()==3,"budget interruption retains every scheduled update");
  }
  FluidScheduler unavailable(rules);unavailable.set_region({0,0,0});World unknown;
  const BlockPosition cold{0,WorldMinY,0};unknown.set(cold,rules.water[3]);unknown.unknown=cold;
  unavailable.schedule(cold,0);tick(unavailable,unknown,0);
  const auto retired=tick(unavailable,unknown,250);
  require(retired.repair_deferred==1 && unavailable.pending_count()==0,
      "twice unavailable work retires to repair instead of monopolizing due queue");
  unknown.unknown.reset();auto recovery=budget();recovery.repair_samples=1;recovery.repair_schedules=1;
  tick(unavailable,unknown,500,recovery);tick(unavailable,unknown,750,recovery);
  require(unknown.get(cold)==AirBlock,"cyclic repair recovers previously unavailable fluid");
  FluidScheduler repair(rules);repair.set_region({0,0,0});World w;
  auto b=budget();b.repair_samples=7;b.repair_schedules=2;
  auto r=tick(repair,w,0,b);require(r.repair_samples==7,"empty-region repair sample budget enforced");
  FluidScheduler limited(rules);limited.set_region({0,0,0});World wet;
  wet.set({0,WorldMinY,0},rules.water[3]);wet.set({1,WorldMinY,0},rules.water[3]);
  b.repair_samples=32;b.repair_schedules=1;
  r=tick(limited,wet,0,b);require(r.repair_schedules==1,"repair neighborhood scheduling budget enforced");
  FluidScheduler enclosed(rules);enclosed.set_region({0,0,0});
  const FluidRead sealed=[&](BlockPosition at,uint16_t& block) {
    block=at==BlockPosition{0,WorldMinY,0}?rules.water[0]:rules.stone;return true;
  };
  const FluidApply no_apply=[](auto,uint16_t,uint16_t) {
    require(false,"enclosed stable source must not apply repair");return FluidApplyResult::Retry;
  };
  b.repair_samples=1;b.repair_schedules=1;
  const auto stable=enclosed.tick(0,sealed,no_apply,b);
  require(stable.repair_samples==1 && stable.repair_schedules==0 && stable.pending==0,
      "original repair predicate skips enclosed stable source");
}
void regions_and_repair(const FluidRules& rules) {
  FluidScheduler scheduler(rules);require(scheduler.set_region({-1,-1,0}),"signed region accepted");
  require(scheduler.schedule({-32,WorldMinY,-32},500) && scheduler.schedule({-1,WorldMaxYExclusive-1,-1},500),"region inclusive signed boundaries");
  require(!scheduler.schedule({0,0,-1},0) && !scheduler.schedule({-1,WorldMaxYExclusive,-1},0) &&
      !scheduler.schedule({-1,WorldMinY-1,-1},0),"out-of-region and signed-Y positions rejected");
  require(!scheduler.set_region({0,0,33}),"oversized region rejected");
  require(!scheduler.set_region({std::numeric_limits<int32_t>::max(),0,0}),"unrepresentable block coordinate region rejected");
  require(scheduler.set_region({0,0,0}) && scheduler.pending_count()==0,"region move retires disjoint pending cells");
  require(!scheduler.schedule({-1,0,-1},0) && scheduler.schedule({0,0,0},0),"new region replaces old admission bounds");
  FluidScheduler halo(rules);halo.set_region({0,0,0});World edge;
  edge.set({-1,3,0},rules.water[0]);edge.set({-1,2,0},rules.stone);halo.schedule({0,3,0},0);
  tick(halo,edge,0);
  require(edge.get({0,3,0})==rules.water[1] && edge.applied.size()==1 && edge.applied[0].position.x==0,
      "read-only neighbor halo supplies fluid without applying outside active region");
  World moved;moved.set({0,WorldMinY,0},rules.water[3]);auto b=budget();b.repair_samples=1;b.repair_schedules=1;
  tick(scheduler,moved,0,b);tick(scheduler,moved,1000,b);
  require(moved.get({0,WorldMinY,0})==AirBlock,"new region repair cursor starts within new bounds");
  FluidScheduler full(rules);full.set_region({0,0,0});World world;
  for(int i=0;i<8192;++i) require(full.schedule({i%32,1+i/1024,(i/32)%32},0),"bounded scheduler fill");
  const BlockPosition stale{0,WorldMinY,0};world.set(stale,rules.water[3]);
  require(!full.schedule(stale,0) && full.pending_count()==8192,"saturation rejects new unique position at exact bound");
  auto report=tick(full,world,0);require(report.saturated>=1,"saturation admission loss reported");
  for(int i=0;i<32;++i) tick(full,world,0);
  require(full.pending_count()==0,"bounded batches drain all admitted no-op work");
  tick(full,world,1000,b);tick(full,world,2000,b);
  require(world.get(stale)==AirBlock,"repair discovers and fixes fluid missed during saturation");
  FluidScheduler dependencies(rules);dependencies.set_region({0,0,0});World dirty;
  for(int y=1;y<64 && dependencies.pending_count()<MaxPendingFluids;y+=3)
    for(int x=0;x<32;x+=4) for(int z=0;z<32;z+=4)
      dependencies.notify_change({x,y,z},rules.stone,rules.stone,0);
  require(dependencies.pending_count()==MaxPendingFluids,"topology dependency saturation fixture");
  const BlockPosition urgent{31,200,31};dirty.set(urgent,rules.water[3]);
  require(dependencies.schedule(urgent,0),"direct work displaces a saturated slope dependency");
  const auto serviced=tick(dependencies,dirty,0);
  require(dirty.get(urgent)==AirBlock && serviced.saturated>0,
      "saturated slope work cannot block a new due direct change");
}
void native_apply(const FluidRules& rules) {
  FluidScheduler scheduler(rules);scheduler.set_region({0,0,0});BlockStore store;
  const BlockPosition source{16,1,16},flow{17,1,16};
  BlockEditPolicy policy;
  policy.generated_block=[&](const auto& p){return p.y==0?rules.stone:AirBlock;};
  policy.is_known_block=[](uint16_t){return true;};
  policy.can_apply_edit=[](const auto&,uint16_t){return true;};
  policy.can_stay_supported=[](uint16_t block,const auto&,uint16_t below){return block!=9 || below!=AirBlock;};
  store.set_block({source,rules.water[0]});store.set_block({flow,rules.water[1]});
  require(apply_block_edit(store,{source,AirBlock},policy).result.changed,"native source removal fixture");
  scheduler.notify_change(source,rules.water[0],AirBlock,0);
  const FluidRead read=[&](auto p,uint16_t& b){b=get_effective_block(store,p,policy);return true;};
  unsigned changes=0;uint64_t current_now=0;
  const FluidApply apply=[&](auto p,uint16_t expected,uint16_t next) {
    if(get_effective_block(store,p,policy)!=expected)return FluidApplyResult::Retry;
    auto result=apply_block_edit_and_enqueue(store,nullptr,{p,next},policy);
    if(result.deferred)return FluidApplyResult::Retry;
    for(const auto& edit:result.changes) {
      ++changes;
      if(edit.position!=p) scheduler.notify_change(edit.position,expected,edit.block,current_now);
    }
    return result.result.changed?FluidApplyResult::Applied:FluidApplyResult::Unchanged;
  };
  for(current_now=250;current_now<=5000;current_now+=250) scheduler.tick(current_now,read,apply,budget());
  require(store.block_count()==0 && changes>0,"source removal drains native fluid overrides back to generated air");
  const BlockPosition below{8,3,8},plant{8,4,8};
  store.set_block({below,rules.water[3]});store.set_block({plant,9});scheduler.schedule(below,6000);
  const auto before=changes;current_now=6000;scheduler.tick(current_now,read,apply,budget());
  require(changes==before+2 && get_effective_block(store,plant,policy)==AirBlock,
      "scheduler apply adapter retains actual native support cascade");
}
void outward_flow(const FluidRules& rules) {
  const BlockPosition source{-1,1,-1};
  for(const auto kind:{FluidKind::Water,FluidKind::Lava}) {
    const uint64_t delay=kind==FluidKind::Water?250:500;
    for(const auto direction:{BlockPosition{1,0,0},{-1,0,0},{0,0,1},{0,0,-1}})
      for(const int distance:{2,kind==FluidKind::Water?5:3}) {
        FluidScheduler scheduler(rules);scheduler.set_region({-1,-1,1});
        World world;world.floor_block=rules.stone;
        const BlockPosition hole{source.x+direction.x*distance,0,source.z+direction.z*distance};
        world.set(hole,AirBlock);world.set(source,rules.make(kind,0));
        scheduler.notify_change(source,AirBlock,world.get(source),0);
        uint64_t now=0;
        for(int step=0;step<40;++step) tick(scheduler,world,now+=delay);
        const BlockPosition opposite{source.x-direction.x,1,source.z-direction.z};
        require(world.get(opposite)==AirBlock,"closest drop initially suppresses opposite spread");
        require(world.get(hole)==rules.make(kind,1),"preferred route falls into drop");
        const auto before=world.get(hole);world.set(hole,rules.stone);
        scheduler.notify_change(hole,before,rules.stone,now);
        for(int step=0;step<40;++step) tick(scheduler,world,now+=delay);
        // Independent finite-distance result on a flat floor, including diagonals.
        for(int x=-8;x<=8;++x) for(int z=-8;z<=8;++z) {
          const int distance_from_source=std::abs(x)+std::abs(z);
          const auto expected=distance_from_source<=7?rules.make(kind,distance_from_source):AirBlock;
          require(world.get({source.x+x,1,source.z+z})==expected,
              "plugged drop must resume symmetric bounded outward flow");
        }
        require(scheduler.pending_count()==0,"settled outward flow exhausts work without repair scanning");
        const auto changes=world.applied.size();tick(scheduler,world,now+=delay);
        require(world.applied.size()==changes,"settled floor does not generate endless mutations");
        world.set(source,AirBlock);scheduler.notify_change(source,rules.make(kind,0),AirBlock,now);
        for(int step=0;step<60;++step) tick(scheduler,world,now+=delay);
        for(int x=-8;x<=8;++x) for(int z=-8;z<=8;++z)
          require(world.get({source.x+x,1,source.z+z})==AirBlock,"removed source drains its complete footprint");
      }
    FluidScheduler falling(rules);falling.set_region({-1,-1,1});World world;
    world.floor_block=rules.stone;const BlockPosition high{-1,4,-1};
    world.set(high,rules.make(kind,0));falling.notify_change(high,AirBlock,world.get(high),0);
    uint64_t now=0;
    for(int step=0;step<60;++step) tick(falling,world,now+=delay);
    for(int y=1;y<4;++y)
      require(world.get({-1,y,-1})==rules.make(kind,1),"downward flow reaches its floor");
    for(int x=-7;x<=7;++x) for(int z=-7;z<=7;++z) {
      const int distance=std::abs(x)+std::abs(z);
      require(world.get({-1+x,1,-1+z})==(distance<=6?rules.make(kind,distance+1):AirBlock),
          "falling column spreads in every floor direction and stops at its level limit");
      if(distance) require(world.get({-1+x,3,-1+z})==AirBlock,"falling column retains downward priority");
    }
  }
}
}
void check_scheduler(const FluidRules& rules) {
  timing(rules);retries_and_budgets(rules);regions_and_repair(rules);native_apply(rules);outward_flow(rules);
  std::cout<<"fluid_scheduler=passed timing=250/500 budgets=passed retry=passed region_repair=passed native_apply=passed\n";
}
}
