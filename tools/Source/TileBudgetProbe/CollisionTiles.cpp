#include "MeshCollisionScene.h"
#include "MeshCollisionWorld.h"
#include <box3d/constants.h>
#include <cassert>
#include <cstdio>
#include <limits>
using namespace octaryn::character_motion;
int main() {
  float floor[]{-1,0,-1,0,0,1,1,0,-1};
  float tiny[]{0,0,0,0,0,.00001f,.00001f,0,0};
  uint32_t indices[]{0,1,2};
  MeshCollision solid{floor,9,indices,3},empty{tiny,9,indices,3};
  MeshCollisionScene scene;
  const auto hits=[&] {
    return b3World_CastRayClosest(scene.collision_world()->world,{0,1,0},{0,-2,0},b3DefaultQueryFilter()).hit;
  };
  assert(scene.set_tile(1,solid) && hits());
  auto prepared=MeshCollisionScene::prepare_tile(empty);
  assert(prepared && scene.set_tile(2,std::move(prepared)) && scene.contains(2) && hits());
  assert(b3World_GetCounters(scene.collision_world()->world).bodyCount==1);
  assert(scene.set_tile(1,empty) && scene.contains(1) && !hits());
  assert(b3World_GetCounters(scene.collision_world()->world).bodyCount==0);
  assert(scene.set_tile(1,solid) && hits());
  uint32_t mixed[]{0,0,0,0,1,2};
  assert(scene.set_tile(1,MeshCollision{floor,9,mixed,6}) && hits());
  uint32_t invalid[]{0,1,3};
  assert(!scene.set_tile(1,MeshCollision{floor,9,invalid,3}) && hits());
  assert(!MeshCollisionScene::prepare_tile({floor,8,indices,3}));
  assert(!MeshCollisionScene::prepare_tile({floor,9,indices,2}));
  assert(!MeshCollisionScene::prepare_tile({nullptr,9,indices,3}));
  tiny[0]=std::numeric_limits<float>::quiet_NaN();
  assert(!MeshCollisionScene::prepare_tile(empty));
  tiny[0]=std::numeric_limits<float>::infinity();
  assert(!MeshCollisionScene::prepare_tile(empty));
  // The boundary is Box3D's own rule: exactly equal area remains accepted.
  float boundary[]{0,0,0,0,0,1,2*.01f*B3_LINEAR_SLOP*B3_LINEAR_SLOP,0,0};
  bool valid_empty=true;
  auto* mesh=build_collision_mesh({boundary,9,indices,3},&valid_empty);
  assert(mesh && !valid_empty);b3DestroyMesh(mesh);
  for(unsigned i=0;i<100;++i) {
    tiny[0]=0;
    assert(scene.set_tile(2,empty) && scene.contains(2));
    scene.remove_tile(2);assert(!scene.contains(2) && hits());
  }
  std::puts("collision_empty_tiles passed=1 floor_preserved=1 replacements=100 invalid_rejected=1 exact_threshold=1");
}
