#include "MapWorld.h"
#include "MapWorldSession.h"
#include "CharacterMotion.h"
#include <cmath>
#include <cstdio>

static unsigned calls;
namespace octaryn::character_motion {
// Tests authority dispatch and delta accounting, not the Jolt solver.
void step_on_mesh(const Input& input, float dt, State& state, const MeshCollision&) {
  ++calls;
  state.yaw=input.camera_yaw;
  state.pitch=input.camera_pitch;
  state.velocity_y -= 20.0f * dt;
  state.y += state.velocity_y * dt;
}
}
int main() {
  octaryn::server::map_world::ServerMapWorld world;
  OctarynServerPlayerInput input{};
  OctarynServerPlayerState state{};
  OctarynServerPlayerTickResult result{};
  state.y=10;
  state.yaw=.6f;
  state.pitch=-.25f;
  if(octaryn_server_map_world_step(&world,&input,.1,&state,&result)!=0 ||
      calls!=1 || result.tick_input!=0 || state.y>=10 ||
      std::abs(result.delta_y-(state.y-10))>1e-5f ||
      state.yaw!=.6f || state.pitch!=-.25f) return 1;
  const float previous=state.y;
  if(octaryn_server_map_world_step(&world,&input,.1,&state,&result)!=0 ||
      calls!=2 || state.y>=previous || state.velocity_y>=-2) return 2;
  if(octaryn_server_map_world_step(nullptr,&input,.1,&state,&result)!=-1) return 3;
  std::puts("map idle authority dispatch: passed (physics test double)");
}
