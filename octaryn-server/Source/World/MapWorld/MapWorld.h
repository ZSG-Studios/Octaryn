#pragma once

#include "PlayerSimulation.h"
#include "octaryn_host_api.h"
#include "CollisionResidencyStats.h"

#if defined(_WIN32)
#if defined(OCTARYN_MAP_WORLD_EXPORTS)
#define OCTARYN_MAP_WORLD_API __declspec(dllexport)
#else
#define OCTARYN_MAP_WORLD_API __declspec(dllimport)
#endif
#else
#define OCTARYN_MAP_WORLD_API __attribute__((visibility("default")))
#endif

extern "C" {

// Loads the GLB triangle soup plus manifest spawn pose; null on failure.
OCTARYN_MAP_WORLD_API void *octaryn_server_map_world_create(
    const char *glb_path_utf8, const char *manifest_path_utf8);

OCTARYN_MAP_WORLD_API void octaryn_server_map_world_destroy(void *handle);

// Requests collision around an authoritative body: 0 ready, 1 pending,
// -1 invalid arguments, -2 fatal tile preparation or protected-set budget failure.
// Pending commands must not be acknowledged until this region is ready.
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_collision_ready(
    void* handle, float x, float z, float radius);
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_collision_stats(
    void* handle, OctarynCollisionResidencyStats* stats, uint32_t byte_size);

// Fills the eye position and view angles from the manifest; 0/-1.
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_spawn(
    void *handle, OctarynServerPlayerState *state);

// Same contract as octaryn_server_player_step, moved against the map mesh.
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_step(
    void *handle, const OctarynServerPlayerInput *input, double delta_seconds,
    OctarynServerPlayerState *state, OctarynServerPlayerTickResult *result);

// Resident collision triangle count; streamed worlds may change this over time.
OCTARYN_MAP_WORLD_API unsigned long long
octaryn_server_map_world_triangle_count(void *handle);

// Closest-hit ray cast against the map collision mesh. Direction need not be
// normalized; the ray spans origin + direction * max_distance. Fills out_hit
// and returns 0 on hit, 1 on miss, <0 on error.
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_raycast(
    void *handle, float origin_x, float origin_y, float origin_z,
    float direction_x, float direction_y, float direction_z,
    float max_distance, octaryn_host_raycast_hit *out_hit);

// Ballistic state of one dropped item against the map collision mesh.
// Position is the collision capsule centre. Sleeping items are settled and
// skip simulation until the module teleports them or applies an impulse.
typedef struct octaryn_world_item_state {
  float x, y, z;
  float velocity_x, velocity_y, velocity_z;
  uint32_t grounded;
  uint32_t sleeping;
  float sleep_timer;
} octaryn_world_item_state;

// Steps one dropped item with swept motion, restitution bounces, contact
// friction, and settle-to-sleep. Pending streamed collision returns 0 with the
// entire state held unchanged. Never enter an analytic fall path while pending.
// Returns <0 on invalid arguments.
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_step_item(
    void *handle, octaryn_world_item_state *state, double delta_seconds);

} // extern "C"
