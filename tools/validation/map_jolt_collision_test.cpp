#include "CharacterMotion.h"
#include "CharacterGeometry.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <chrono>
using namespace octaryn::character_motion;
int main() {
  // Upward-facing finite platform, exact production Jolt mesh path.
  static const float positions[]={-4,0,-4, -4,0,4, 4,0,4, 4,0,-4};
  static const uint32_t indices[]={0,1,2,0,2,3};
  MeshCollision mesh{positions,12,indices,6};
  Input input{}; State state{}; state.y=EyeOffset+2;
  int checks=0,failures=0;
  auto check=[&](bool okay,const char* name){++checks;if(!okay){++failures;std::printf("FAIL %s y=%f vy=%f ground=%u\n",name,state.y,state.velocity_y,state.is_on_ground);}};
  for(int i=0;i<120;++i)step_on_mesh(input,1.0f/60,state,mesh);
  check(state.is_on_ground!=0,"lands on platform");
  check(std::abs(state.y-EyeOffset)<.04f,"resting height");
  input.flags=1;
  step_on_mesh(input,1.0f/60,state,mesh);
  check(state.velocity_y>7 && state.is_on_ground==0,"ground jump rises");
  for(int i=0;i<120;++i)step_on_mesh(input,1.0f/60,state,mesh);
  check(state.is_on_ground!=0 && std::abs(state.y-EyeOffset)<.04f,
        "held jump lands without repeating");
  input.flags=0;step_on_mesh(input,1.0f/60,state,mesh);
  // A stale snapshot/teleport ground bit must not authorize a midair jump.
  State airborne=state; airborne.x=8; airborne.y=EyeOffset+3;
  input.flags=1; step_on_mesh(input,1.0f/60,airborne,mesh);
  check(airborne.velocity_y<0 && airborne.is_on_ground==0,"stale support cannot jump");
  input.flags=0;input.move_x=1;
  for(int i=0;i<90;++i)step_on_mesh(input,1.0f/60,state,mesh);
  check(state.x>5,"walks beyond finite edge");
  check(state.is_on_ground==0,"loses ground at edge");
  check(state.velocity_y < -4,"fall accelerates after edge");
  check(state.y < EyeOffset-.5f,"falls below platform");
  static const float room_positions[]={
      -4,0,-4,-4,0,4,4,0,4,4,0,-4,
      2,0,-4,2,4,-4,2,4,4,2,0,4,
      -4,2.1f,-4,4,2.1f,-4,4,2.1f,4,-4,2.1f,4};
  static const uint32_t room_indices[]={0,1,2,0,2,3,4,6,5,4,7,6,8,9,10,8,10,11};
  MeshCollision room{room_positions,36,room_indices,18};
  state={};state.y=EyeOffset;input={};input.move_x=1;input.flags=2;
  for(int i=0;i<90;++i)step_on_mesh(input,1.0f/60,state,room);
  check(state.x>1.5f && state.x<=2-CollisionRadius+.02f,"sprint cannot cross wall");
  check(state.is_on_ground!=0,"wall contact retains floor support");
  state={};state.y=EyeOffset;input={};input.flags=1;
  float peak=state.y;
  for(int i=0;i<120;++i){step_on_mesh(input,1.0f/60,state,room);peak=std::max(peak,state.y);}
  check(peak>EyeOffset+.1f && peak<=2.1f-(CollisionHeight-EyeOffset)+.02f,
        "jump cannot penetrate low ceiling");
  check(state.is_on_ground!=0 && std::abs(state.y-EyeOffset)<.04f,
        "ceiling jump returns to floor");
  state={};state.y=EyeOffset;input={};
  const auto started=std::chrono::steady_clock::now();
  for(int i=0;i<600;++i)step_on_mesh(input,1.0f/60,state,mesh);
  const double elapsed=std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-started).count();
  check(state.is_on_ground!=0 && std::abs(state.y-EyeOffset)<.04f,
        "stable idle support");
  // A 0.18m curb is below the configured step height and should not stop the
  // capsule; a 0.42m stair is within the exact step contract and should climb.
  static const float curb_positions[]={-4,0,-4,-4,0,4,4,0,4,4,0,-4,
      -1,0,-1,-1,.18f,-1,1,.18f,-1,1,0,-1};
  static const uint32_t curb_indices[]={0,1,2,0,2,3,4,6,5,4,7,6};
  MeshCollision curb{curb_positions,24,curb_indices,12};
  state={}; state.y=EyeOffset; input={}; input.move_x=1; input.flags=2;
  for(int i=0;i<100;++i)step_on_mesh(input,1.0f/60,state,curb);
  check(state.x>3.0f,"rounded capsule clears small curb");
  std::printf("map_jolt_idle_step_mean_ms=%.6f samples=600\n",elapsed/600);
  std::printf("map_jolt_collision checks=%d failures=%d\n",checks,failures);
  return failures?1:0;
}
