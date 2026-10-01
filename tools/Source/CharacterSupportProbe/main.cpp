#include "CharacterMotion.h"
#include "CharacterGeometry.h"
#include "MeshCollisionScene.h"
#include "MeshCollisionWorld.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace motion=octaryn::character_motion;
constexpr float Dt=1.f/60;
struct Floor {
  std::array<float,12> vertices;
  std::array<uint32_t,6> indices{0,1,2,0,2,3};
  motion::MeshCollision mesh;
  explicit Floor(float degrees):vertices{-32,-32*std::tan(degrees*.01745329252f),32,
       32,32*std::tan(degrees*.01745329252f),32,
       32,32*std::tan(degrees*.01745329252f),-32,
       -32,-32*std::tan(degrees*.01745329252f),-32},
      mesh{vertices.data(),vertices.size(),indices.data(),indices.size()} {}
  ~Floor(){motion::release_mesh_collision(mesh);}
};
motion::State settle(const motion::MeshCollision& mesh) {
  motion::State state{};state.y=motion::EyeOffset+1;
  for(int i=0;i<240;++i)motion::step_on_mesh({},Dt,state,mesh);
  return state;
}
bool idle(float slope,bool observe) {
  Floor floor(slope);auto state=settle(floor.mesh);
  if(!state.is_on_ground){std::printf("support_probe not_grounded slope=%.3f eye=%.9f,%.9f,%.9f velocity=%.9f\n",
      slope,state.x,state.y,state.z,state.velocity_y);return false;}
  const auto initial=state;
  const b3Pos center{state.x,state.y-motion::EyeOffset+motion::CollisionRadius,state.z};
  const auto hit=b3World_CastRayClosest(motion::acquire_mesh_world(floor.mesh)->world,center,
      {0,-.45f,0},b3DefaultQueryFilter());
  std::printf("support_contact slope=%.3f separation=%.9f\n",slope,
      hit.hit?b3Dot(b3SubPos(center,hit.point),hit.normal)-motion::CollisionRadius:999.f);
  for(int i=0;i<108000;++i)motion::step_on_mesh({},Dt,state,floor.mesh);
  const float drift=std::hypot(state.x-initial.x,state.z-initial.z);
  std::printf("support_idle slope=%.3f steps=108000 drift_m=%.9f grounded=%u\n",slope,drift,state.is_on_ground);
  return observe || (drift<.002f && state.is_on_ground && std::abs(state.y-initial.y)<.002f);
}
bool downhill_and_jump() {
  Floor floor(30);auto state=settle(floor.mesh);const auto start=state;
  motion::Input input{};input.move_x=-1;
  for(int i=0;i<60;++i)motion::step_on_mesh(input,Dt,state,floor.mesh);
  if(state.x>start.x-3 || state.y>=start.y-1 || !state.is_on_ground)return false;
  input={};input.flags=1;const float before=state.y;
  motion::step_on_mesh(input,Dt,state,floor.mesh);input.flags=0;
  float peak=state.y;
  for(int i=0;i<180;++i){motion::step_on_mesh(input,Dt,state,floor.mesh);peak=std::max(peak,state.y);}
  return peak>before+.6f && state.is_on_ground;
}
bool removed_support() {
  Floor floor(0);motion::MeshCollisionScene scene;
  if(!scene.set_tile(1,floor.mesh))return false;
  auto mesh=scene.view();auto state=settle(mesh);const float before=state.y;
  scene.remove_tile(1);
  for(int i=0;i<30;++i)motion::step_on_mesh({},Dt,state,mesh);
  return !state.is_on_ground && state.y<before-1 && state.velocity_y<-5;
}
bool steep_remains_airborne() {
  Floor floor(50);auto state=settle(floor.mesh);const float before=state.y;
  for(int i=0;i<60;++i)motion::step_on_mesh({},Dt,state,floor.mesh);
  return !state.is_on_ground && state.y<before-.1f;
}
bool changed_support() {
  Floor floor(0);motion::MeshCollisionScene scene;
  if(!scene.set_tile(1,floor.mesh))return false;
  auto mesh=scene.view();auto state=settle(mesh);const float resting=state.y;
  // Stale grounded state inside the broad probe range must still settle.
  state.y+=.1f;state.is_on_ground=1;
  for(int i=0;i<120;++i)motion::step_on_mesh({},Dt,state,mesh);
  if(std::abs(state.y-resting)>.01f)return false;
  for(unsigned i=1;i<floor.vertices.size();i+=3)floor.vertices[i]-=.08f;
  if(!scene.set_tile(1,floor.mesh))return false;
  for(int i=0;i<120;++i)motion::step_on_mesh({},Dt,state,mesh);
  if(!state.is_on_ground || std::abs(state.y-(resting-.08f))>.01f)return false;
  // A newly published penetrating wall must invalidate idle rest.
  const float wall[]={.1f,-1,-2,.1f,3,-2,.1f,3,2,.1f,-1,2};
  const uint32_t triangles[]={0,2,1,0,3,2};
  if(!scene.set_tile(2,{wall,12,triangles,6}))return false;
  const float before=state.x;
  for(int i=0;i<30;++i)motion::step_on_mesh({},Dt,state,mesh);
  return std::abs(state.x-before)>.05f;
}
int main(int argc,char** argv) {
  const bool observe=argc>1 && std::strcmp(argv[1],"--observe")==0;
  if(!idle(.21f,observe))return 1;
  if(observe)return 0;
  for(float slope:{0.f,30.f,44.9f})if(!idle(slope,false))return 2;
  if(!downhill_and_jump())return 3;
  if(!removed_support())return 4;
  if(!steep_remains_airborne())return 5;
  if(!changed_support())return 6;
  std::puts("support_probe passed=1 downhill=1 jump=1 removed_support=1 steep_airborne=1 stale_ground=1 lowered_floor=1 penetrating_wall=1");
}
