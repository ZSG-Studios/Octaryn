#include "WorldItems.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

using namespace octaryn::world_items;
namespace {
bool position(float x) {return std::isfinite(x)&&std::abs(x)<1000000;}
bool valid_item(const Item& i) {
  return i.id && i.block>0 && i.block<=65535 && i.count>0 && i.count<=stack_limit &&
    position(i.x)&&position(i.y)&&position(i.z)&&std::isfinite(i.vx)&&std::isfinite(i.vy)&&
    std::isfinite(i.vz)&&std::abs(i.vx)<=64&&std::abs(i.vy)<=64&&std::abs(i.vz)<=64&&
    std::isfinite(i.age)&&i.age>=0&&i.age<=300&&std::isfinite(i.pickup_delay)&&
    i.pickup_delay>=0&&i.pickup_delay<=2;
}
bool blocked(const Item& i,octaryn_item_solid_fn solid,void* context) {
  constexpr float half=.125f;
  for(int y=static_cast<int>(std::floor(i.y-half));y<=static_cast<int>(std::floor(i.y+half));++y)
    for(int z=static_cast<int>(std::floor(i.z-half));z<=static_cast<int>(std::floor(i.z+half));++z)
      for(int x=static_cast<int>(std::floor(i.x-half));x<=static_cast<int>(std::floor(i.x+half));++x)
        if(solid(context,x,y,z)) return true;
  return false;
}
bool leave_solid(Item& item,octaryn_item_solid_fn solid,void* context) {
  if(!blocked(item,solid,context))return true;
  // Resolve placement around an existing item without unbounded searches or deleting it.
  constexpr int directions[6][3]={{0,1,0},{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},{0,-1,0}};
  for(unsigned step=1;step<=6;++step)for(const auto& direction:directions) {
    auto candidate=item;const float distance=static_cast<float>(step)*.25f;
    candidate.x+=static_cast<float>(direction[0])*distance;
    candidate.y+=static_cast<float>(direction[1])*distance;
    candidate.z+=static_cast<float>(direction[2])*distance;
    if(!blocked(candidate,solid,context)) {
      item=candidate;item.vx=item.vy=item.vz=0;return true;
    }
  }
  item.vx=item.vy=item.vz=0;return false;
}
void erase_item(State& state,std::uint32_t index) {
  for(auto i=index+1;i<state.item_count;++i) state.items[i-1]=state.items[i];
  state.items[--state.item_count]={};
}
float distance2(float x,float y,float z) {return x*x+y*y+z*z;}
}
extern "C" {
void octaryn_items_initialize(State* state) {if(state)*state=State{};}
int octaryn_items_validate(const State* s) {
  if(!s||s->version!=1||s->size!=sizeof(State)||s->reserved||!s->next_item||!s->next_grant||
      s->item_count>max_items||s->grant_count>max_grants||!std::isfinite(s->seconds)||s->seconds<0||
      !std::isfinite(s->next_drop)||s->next_drop<0||s->acknowledged_grant>=s->next_grant||
      static_cast<unsigned>(s->receipt_result)>4) return -1;
  std::set<std::uint64_t> ids;
  for(std::uint32_t i=0;i<s->item_count;++i)
    if(!valid_item(s->items[i])||s->items[i].id>=s->next_item||!ids.insert(s->items[i].id).second)return -1;
  auto previous=s->acknowledged_grant;
  for(std::uint32_t i=0;i<s->grant_count;++i) {
    const auto& g=s->grants[i];
    if(g.id<=previous||g.id>=s->next_grant||!g.block||g.block>65535||!g.count||g.count>stack_limit)return -1;
    previous=g.id;
  }
  return 0;
}
int octaryn_items_drop(State* s,std::uint64_t command,std::uint32_t block,std::uint32_t count,
    std::uint32_t placeable,float x,float y,float z,float yaw,float pitch) {
  if(!s||!command)return -1;
  if(command<=s->last_command)return 0;
  if(command!=s->last_command+1)return -1;
  s->last_command=command;s->receipt_block=block;s->receipt_count=count;s->receipt_result=DropResult::Invalid;
  if(!placeable||!block||block>65535||!count||count>drop_limit||!position(x)||!position(y)||
      !position(z)||!std::isfinite(yaw)||!std::isfinite(pitch))return 1;
  if(s->seconds<s->next_drop){s->receipt_result=DropResult::RateLimited;return 1;}
  const auto stacks=(count+stack_limit-1)/stack_limit;
  if(s->item_count>max_items||stacks>max_items-s->item_count||!s->next_item||
      stacks>std::numeric_limits<std::uint64_t>::max()-s->next_item) {
    s->receipt_result=DropResult::Full;return 1;
  }
  Item item{};item.block=block;
  item.x=x;item.y=y-.25f;item.z=z;
  item.vx=std::sin(yaw)*std::cos(pitch)*4;
  item.vy=std::sin(pitch)*4+2;item.vz=-std::cos(yaw)*std::cos(pitch)*4;
  item.pickup_delay=2;
  // Every capacity/ID check precedes this bounded, non-throwing commit.
  for(auto remaining=count;remaining;) {
    item.id=s->next_item++;item.count=std::min(remaining,stack_limit);
    s->items[s->item_count++]=item;remaining-=item.count;
  }
  s->next_drop=s->seconds+.1;
  s->receipt_result=DropResult::Accepted;return 1;
}
int octaryn_items_acknowledge(State* s,std::uint64_t grant) {
  if(!s)return -1;
  if(grant<=s->acknowledged_grant)return 0;
  if(!s->grant_count||s->grants[0].id!=grant)return -1;
  s->acknowledged_grant=grant;
  for(std::uint32_t i=1;i<s->grant_count;++i)s->grants[i-1]=s->grants[i];
  s->grants[--s->grant_count]={};return 1;
}
int octaryn_items_tick(State* s,double delta,float px,float py,float pz,octaryn_item_solid_fn solid,void* context) {
  if(!s||!solid||!std::isfinite(delta)||delta<0||delta>.25||!position(px)||!position(py)||!position(pz))return -1;
  s->seconds+=delta;
  const auto steps=std::max(1,static_cast<int>(std::ceil(delta*120)));
  const float dt=static_cast<float>(delta/steps);
  for(std::uint32_t n=0;n<s->item_count;) {
    auto& i=s->items[n];
    // Frozen outside the local simulation region, including despawn age.
    if(distance2(i.x-px,0,i.z-pz)>64*64){++n;continue;}
    i.age+=delta;i.pickup_delay=std::max(0.0,i.pickup_delay-delta);
    if(i.age>=300){erase_item(*s,n);continue;}
    if(!leave_solid(i,solid,context)){++n;continue;}
    for(int step=0;step<steps;++step) {
      i.vy=std::max(-32.f,i.vy-20*dt);
      float* positions[]={&i.x,&i.y,&i.z};float* velocities[]={&i.vx,&i.vy,&i.vz};
      for(unsigned axis=0;axis<3;++axis) {
        auto& pos=*positions[axis];auto& vel=*velocities[axis];const auto before=pos;
        pos+=vel*dt;
        if(blocked(i,solid,context)) {pos=before;vel=0;if(axis==1){i.vx*=.8f;i.vz*=.8f;}}
      }
      const auto drag=std::exp(-.6f*dt);i.vx*=drag;i.vz*=drag;
    }
    // The authoritative eye is 1.62 above the feet. Pickup spans the player's body.
    const auto closest_y=std::clamp(i.y,py-1.5f,py);
    if(i.pickup_delay==0&&distance2(i.x-px,i.y-closest_y,i.z-pz)<1.25f*1.25f&&
        s->grant_count<max_grants&&s->next_grant<std::numeric_limits<std::uint64_t>::max()) {
      s->grants[s->grant_count++]={s->next_grant++,i.block,i.count};erase_item(*s,n);continue;
    }
    ++n;
  }
  for(std::uint32_t a=0;a<s->item_count;++a) for(std::uint32_t b=a+1;b<s->item_count;) {
    auto& left=s->items[a];auto& right=s->items[b];
    if(left.block==right.block&&left.count<stack_limit&&
        distance2(left.x-right.x,left.y-right.y,left.z-right.z)<.75f*.75f) {
      const auto amount=std::min(stack_limit-left.count,right.count);
      left.count+=amount;right.count-=amount;left.age=std::min(left.age,right.age);
      left.pickup_delay=std::max(left.pickup_delay,right.pickup_delay);
      if(!right.count){erase_item(*s,b);continue;}
    }
    ++b;
  }
  return 0;
}
}
