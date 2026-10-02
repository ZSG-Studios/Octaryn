#include "MapWorldBodies.h"
#include "MapWorldSession.h"
#include "MeshCollisionWorld.h"
#include <cmath>

namespace {
using octaryn::server::map_world::ServerMapWorld;
auto* bodies(void* handle,bool create=false) {
  auto* world=static_cast<ServerMapWorld*>(handle);
  if(world && !world->bodies && create) {
    auto* collision=octaryn::character_motion::acquire_mesh_world(world->collision());
    if(collision)world->bodies=std::make_unique<octaryn::server::map_world::SceneBodies>(collision);
  }
  return world?world->bodies.get():nullptr;
}
}
extern "C" {
int octaryn_server_map_world_body_create(void* w,const octaryn_scene_body_desc* d,uint64_t* result) {
  if(!w || !d || !result)return -1;
  auto* world=static_cast<ServerMapWorld*>(w);
  if(!world->ready(d->position[0],d->position[1],d->position[2],4))return 2;
  auto* b=bodies(w,true);return b?b->create(*d,*result):-1;
}
int octaryn_server_map_world_body_remove(void* w,uint64_t h) {auto* b=bodies(w);return b?b->remove(h):-1;}
int octaryn_server_map_world_body_pose(void* w,uint64_t h,octaryn_scene_body_pose* p) {auto* b=bodies(w);return b && p?b->pose(h,*p):-1;}
int octaryn_server_map_world_body_ray(void* w,const float* o,const float* d,float max,octaryn_scene_body_hit* p) {
  auto* b=bodies(w);if(!b || !o || !d || !p)return -1;
  auto* world=static_cast<ServerMapWorld*>(w);
  if(!std::isfinite(max) || max<=0 || max>100)return -1;
  for(unsigned i=0;i<3;++i)if(!std::isfinite(o[i]) || !std::isfinite(d[i]))return -1;
  const float length=std::hypot(d[0],d[1],d[2]);if(length<0.00001f)return -1;
  std::array<float,6> bounds{};
  for(unsigned i=0;i<3;++i) {const float end=o[i]+d[i]*max/length;bounds[i]=std::min(o[i],end)-.01f;bounds[i+3]=std::max(o[i],end)+.01f;}
  if(!world->ready_bounds(bounds))return 2;
  return b->ray(o,d,max,*p);
}
int octaryn_server_map_world_body_grab(void* w,uint64_t h,const float* point,const float* target,float force) {auto* b=bodies(w);return b?b->grab(h,point,target,force):-1;}
int octaryn_server_map_world_body_grab_move(void* w,const float* target) {auto* b=bodies(w);return b?b->move(target):-1;}
int octaryn_server_map_world_body_grab_release(void* w) {auto* b=bodies(w);if(!b)return -1;b->release();return 0;}
int octaryn_server_map_world_body_step(void* w,double dt) {
  if(!w || !std::isfinite(dt) || dt<=0 || dt>.25)return -1;auto* b=bodies(w);if(!b)return 0;
  auto* world=static_cast<ServerMapWorld*>(w);
  for(const auto& [handle,bounds]:b->bounds(dt)) {
    bool nearby=true;
    if(world->physics_anchor_valid) {
      const auto& actor=world->physics_anchor;
      nearby=std::hypot((bounds[0]+bounds[3])*.5f-actor[0],(bounds[1]+bounds[4])*.5f-actor[1],(bounds[2]+bounds[5])*.5f-actor[2])<48;
    }
    b->suspend(handle,!nearby || !world->ready_bounds(bounds));
  }
  return b->step(dt);
}
}
