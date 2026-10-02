#pragma once
#include <stdint.h>

// Native owner ABI. Source IDs are content identities, handles are lifetime-local.
typedef struct octaryn_scene_body_shape {
  uint32_t kind, point_count; // 1 box, 2 sphere, 3 capsule, 4 convex hull
  const float* points; // tightly packed xyz; only used by convex hull
  float local_position[3], local_rotation[4];
  float half_extents[3], capsule_a[3], capsule_b[3], radius;
} octaryn_scene_body_shape;
typedef struct octaryn_scene_body_desc {
  uint64_t source_id;
  uint32_t shape_count, flags;
  const octaryn_scene_body_shape* shapes;
  float position[3], rotation[4];
  float mass, linear_damping, angular_damping, friction, restitution;
  float inertia[9], center[3];
} octaryn_scene_body_desc;
typedef struct octaryn_scene_body_pose {
  uint64_t handle, source_id;
  float position[3], rotation[4], velocity[3], angular_velocity[3];
  uint32_t flags; // 1 sleeping, 2 grabbed, 4 removed, 8 inactive
} octaryn_scene_body_pose;
typedef struct octaryn_scene_body_hit {
  uint64_t handle, source_id;
  float point[3], normal[3], distance;
} octaryn_scene_body_hit;

// Client bridge: ordered command journal and server-authored snapshot only.
typedef struct octaryn_host_scene_physics_api {
  uint32_t version, size;
  int32_t (*submit)(const uint8_t* utf8_request);
  int32_t (*snapshot)(uint8_t* utf8_snapshot, uint32_t capacity);
} octaryn_host_scene_physics_api;

#if defined(__cplusplus)
static_assert(sizeof(octaryn_scene_body_shape)==88);
static_assert(sizeof(octaryn_scene_body_desc)==120);
static_assert(sizeof(octaryn_scene_body_pose)==72);
static_assert(sizeof(octaryn_scene_body_hit)==48);
static_assert(sizeof(octaryn_host_scene_physics_api)==24);
#else
_Static_assert(sizeof(octaryn_scene_body_shape)==88,"scene shape ABI");
_Static_assert(sizeof(octaryn_scene_body_desc)==120,"scene body ABI");
_Static_assert(sizeof(octaryn_scene_body_pose)==72,"scene pose ABI");
_Static_assert(sizeof(octaryn_scene_body_hit)==48,"scene hit ABI");
_Static_assert(sizeof(octaryn_host_scene_physics_api)==24,"scene table ABI");
#endif
