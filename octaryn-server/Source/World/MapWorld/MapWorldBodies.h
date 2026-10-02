#pragma once
#include "MapWorld.h"
#include "octaryn_scene_physics.h"
#include <memory>
#include <array>
#include <vector>

namespace octaryn::server::map_world {
class SceneBodies {
public:
  explicit SceneBodies(void* collision_world);
  ~SceneBodies();
  int create(const octaryn_scene_body_desc&,uint64_t&);
  int remove(uint64_t);
  int pose(uint64_t,octaryn_scene_body_pose&) const;
  int ray(const float* origin,const float* direction,float distance,octaryn_scene_body_hit&) const;
  int grab(uint64_t,const float* point,const float* target,float force);
  int move(const float* target);
  void release();
  int step(double);
  std::vector<std::pair<uint64_t,std::array<float,6>>> bounds(double) const;
  void suspend(uint64_t,bool);
private:
  struct State;
  std::unique_ptr<State> state_;
};
}

extern "C" {
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_body_create(void*,const octaryn_scene_body_desc*,uint64_t*);
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_body_remove(void*,uint64_t);
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_body_pose(void*,uint64_t,octaryn_scene_body_pose*);
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_body_ray(void*,const float*,const float*,float,octaryn_scene_body_hit*);
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_body_grab(void*,uint64_t,const float*,const float*,float);
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_body_grab_move(void*,const float*);
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_body_grab_release(void*);
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_body_step(void*,double);
}
