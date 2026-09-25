#include "WorldItems.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <limits>
using namespace octaryn::world_items;
namespace {
unsigned checks{};
void require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
std::uint32_t ground(void*,std::int32_t,std::int32_t y,std::int32_t){return y<0?1u:0u;}
std::uint32_t cube(void*,std::int32_t x,std::int32_t y,std::int32_t z){return x==0&&y==0&&z==0?1u:0u;}
std::uint32_t enclosed(void*,std::int32_t,std::int32_t,std::int32_t){return 1;}
void advance(State& state,double seconds,float x=20,float y=2,float z=20) {
  while(seconds>0){const auto delta=std::min(.25,seconds);
    require(octaryn_items_tick(&state,delta,x,y,z,ground,nullptr)==0,"tick");seconds-=delta;}
}
void whole_stack() {
  State state;
  require(octaryn_items_drop(&state,1,4,drop_limit,1,0,3,0,0,0)==1&&
      state.receipt_result==DropResult::Accepted,"whole creative stack accepted as one command");
  require(state.item_count==16&&state.next_item==17&&state.receipt_count==999,"999 creates exactly16 IDs with full receipt count");
  unsigned sum{};
  for(unsigned i=0;i<state.item_count;++i) {
    sum+=state.items[i].count;
    require(state.items[i].id==i+1&&state.items[i].count==(i<15?64u:39u),"split order and per-entity64 cap");
  }
  require(sum==999&&octaryn_items_validate(&state)==0,"all999 units preserved in valid world state");
  const auto accepted=state;
  require(octaryn_items_drop(&state,1,4,999,1,100,3,0,0,0)==0&&
      std::memcmp(&state,&accepted,sizeof(state))==0,"whole stack replay cannot spawn twice after state restore");
  State pickup=accepted;
  for(unsigned i=0;i<pickup.item_count;++i)pickup.items[i].pickup_delay=0;
  require(octaryn_items_tick(&pickup,0,0,3,0,ground,nullptr)==0&&!pickup.item_count&&pickup.grant_count==16,
      "split entities become16 bounded pickup grants");
  sum=0;for(unsigned i=0;i<pickup.grant_count;++i) {
    sum+=pickup.grants[i].count;require(pickup.grants[i].count<=64,"pickup grant cap remains64");
  }
  require(sum==999&&octaryn_items_validate(&pickup)==0,"pickup transfer preserves all999 units");
  state.seconds=1;
  const auto before=state;
  octaryn_items_drop(&state,2,4,1000,1,0,3,0,0,0);
  require(state.receipt_result==DropResult::Invalid&&state.item_count==before.item_count&&
      state.next_item==before.next_item&&std::memcmp(state.items,before.items,sizeof(state.items))==0,
      "1000 rejected without spawning or consuming entity IDs");
  State full;
  for(unsigned i=0;i<241;++i) {
    full.seconds=i*.11;octaryn_items_drop(&full,i+1,4,1,1,0,3,0,0,0);
  }
  full.seconds+=1;const auto crowded=full;
  octaryn_items_drop(&full,242,4,999,1,0,3,0,0,0);
  require(full.receipt_result==DropResult::Full&&full.item_count==241&&full.next_item==crowded.next_item&&
      full.next_drop==crowded.next_drop&&std::memcmp(full.items,crowded.items,sizeof(full.items))==0,
      "15 free slots reject16-stack command atomically without advancing entity IDs or cooldown");
  const auto rejected=full;
  require(octaryn_items_drop(&full,242,4,999,1,0,3,0,0,0)==0&&
      std::memcmp(&full,&rejected,sizeof(full))==0,"full rejection replay remains stable");
  State edge;edge.next_item=std::numeric_limits<std::uint64_t>::max()-15;
  const auto initial_id=edge.next_item;
  octaryn_items_drop(&edge,1,4,999,1,0,3,0,0,0);
  require(edge.receipt_result==DropResult::Full&&!edge.item_count&&edge.next_item==initial_id,
      "ID exhaustion preflight prevents partial multi-stack spawn");
  State exact;exact.next_item=std::numeric_limits<std::uint64_t>::max()-16;
  octaryn_items_drop(&exact,1,4,999,1,0,3,0,0,0);
  require(exact.receipt_result==DropResult::Accepted&&exact.item_count==16&&
      exact.next_item==std::numeric_limits<std::uint64_t>::max()&&octaryn_items_validate(&exact)==0,
      "last16 usable entity IDs commit without overflow");
  State capacity=crowded;capacity.item_count=240;capacity.items[240]={};
  octaryn_items_drop(&capacity,242,4,999,1,0,3,0,0,0);
  require(capacity.receipt_result==DropResult::Accepted&&capacity.item_count==256&&
      capacity.next_item==crowded.next_item+16,"exact16 remaining world slots accept complete stack");
}
void run() {
  whole_stack();
  State state;octaryn_items_initialize(&state);
  require(octaryn_items_validate(&state)==0,"initialized state ABI");
  require(octaryn_items_drop(&state,1,4,32,1,0,3,0,0,0)==1,"accepted toss");
  require(state.item_count==1&&state.items[0].vz<0&&state.items[0].count==32,"counted authoritative directional toss");
  auto copy=state;
  require(octaryn_items_drop(&state,1,4,32,1,100,100,100,0,0)==0&&
    std::memcmp(&state,&copy,sizeof(state))==0,"replay never changes state or spawns twice");
  octaryn_items_drop(&state,2,4,1,1,0,3,0,0,0);
  require(state.receipt_result==DropResult::RateLimited&&state.item_count==1,"bounded toss rate");
  advance(state,3);
  require(state.items[0].y>=.125f&&state.items[0].y<.2f&&state.items[0].vy==0,"gravity settles above solid floor");
  require(state.items[0].z<0&&std::abs(state.items[0].vx)<.001f,"forward toss direction");
  auto item=state.items[0];
  advance(state,.01,item.x,item.y+1.5f,item.z);
  require(state.item_count==0&&state.grant_count==1&&state.grants[0].count==32,"pickup becomes durable grant");
  require(octaryn_items_acknowledge(&state,2)==-1&&state.grant_count==1,"cannot skip uncommitted grant");
  require(octaryn_items_acknowledge(&state,1)==1&&state.grant_count==0,"ack retires exact oldest grant");
  require(octaryn_items_acknowledge(&state,1)==0,"duplicate ack idempotent");
  octaryn_items_drop(&state,3,4,1000,1,0,3,0,0,0);
  require(state.receipt_result==DropResult::Invalid&&state.item_count==0,"oversize count rejected");
  octaryn_items_drop(&state,4,4,1,0,0,3,0,0,0);
  require(state.receipt_result==DropResult::Invalid,"module placeability enforced");
  octaryn_items_drop(&state,5,4,1,1,NAN,3,0,0,0);
  require(state.receipt_result==DropResult::Invalid,"nonfinite authoritative pose rejected");
  State merged;octaryn_items_initialize(&merged);
  octaryn_items_drop(&merged,1,4,40,1,0,3,0,0,0);merged.seconds=.1;
  octaryn_items_drop(&merged,2,4,40,1,0,3,0,0,0);
  advance(merged,0.001);
  require(merged.item_count==2&&merged.items[0].count==64&&merged.items[1].count==16,"merge conserves count and caps stack");
  merged.items[0].pickup_delay=0;merged.items[1].pickup_delay=2;
  merged.items[0].count=30;merged.items[1].count=20;advance(merged,.001);
  require(merged.item_count==1&&merged.items[0].count==50&&merged.items[0].pickup_delay>1.9,"merge retains newest pickup protection");
  auto frozen=merged.items[0];advance(merged,2,10000,3,10000);
  require(merged.items[0].age==frozen.age&&merged.items[0].y==frozen.y,"outside active region freezes physics and age");
  merged.items[0].age=299.99;advance(merged,.02);
  require(merged.item_count==0,"active item expires at lifetime");
  State restored=state;require(octaryn_items_validate(&restored)==0,"persisted roundtrip validates");
  restored.grant_count=65;require(octaryn_items_validate(&restored)!=0,"corrupt grant bound rejected");
  restored=state;restored.seconds=NAN;require(octaryn_items_validate(&restored)!=0,"corrupt clock rejected");
  State full;octaryn_items_initialize(&full);
  for(unsigned n=0;n<max_items;++n){full.seconds=n*.11;octaryn_items_drop(&full,n+1,4,1,1,0,3,0,0,0);}
  full.seconds+=1;octaryn_items_drop(&full,257,4,1,1,0,3,0,0,0);
  require(full.item_count==256&&full.receipt_result==DropResult::Full,"world item capacity preserves existing stacks");
  require(octaryn_items_validate(&full)==0,"bounded full state valid");
  State blocked;octaryn_items_initialize(&blocked);
  octaryn_items_drop(&blocked,1,4,5,1,0,2,0,0,0);
  blocked.items[0].pickup_delay=0;blocked.items[0].vx=blocked.items[0].vy=blocked.items[0].vz=0;
  blocked.grant_count=64;blocked.next_grant=65;
  for(unsigned n=0;n<64;++n)blocked.grants[n]={n+1,4,1};
  advance(blocked,.001,0,2,0);
  require(blocked.item_count==1&&blocked.grant_count==64,"full grant mailbox retains world item");
  require(octaryn_items_acknowledge(&blocked,1)==1,"retire oldest bounded grant");
  advance(blocked,.001,0,2,0);
  require(blocked.item_count==0&&blocked.grant_count==64&&blocked.grants[63].id==65,
    "pickup resumes with monotonic ID after acknowledgement");
  require(octaryn_items_validate(&blocked)==0,"grant backpressure state remains valid");
  State embedded;octaryn_items_initialize(&embedded);
  octaryn_items_drop(&embedded,1,4,9,1,.5f,.75f,.5f,0,0);
  require(octaryn_items_tick(&embedded,.01,20,2,20,cube,nullptr)==0,"embedded item tick");
  require(embedded.item_count==1&&embedded.items[0].count==9&&embedded.items[0].y>1.125f,
    "newly embedded stack escapes nearest clear space without losing units");
  embedded.items[0].x=embedded.items[0].y=embedded.items[0].z=.5f;
  require(octaryn_items_tick(&embedded,.01,20,2,20,enclosed,nullptr)==0,"enclosed item bounded tick");
  require(embedded.item_count==1&&embedded.items[0].x==.5f&&embedded.items[0].y==.5f,
    "fully enclosed item retained rather than teleported beyond search bound");
}
}
int main(){try{run();std::printf("world_items_native=passed checks=%u\n",checks);return 0;}
  catch(const std::exception& e){std::fprintf(stderr,"world_items_native=failed reason=%s\n",e.what());return 1;}}
