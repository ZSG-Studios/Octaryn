#pragma once

#include "PlayerSimulation.h"

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

// Fills the eye position and view angles from the manifest; 0/-1.
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_spawn(
    void *handle, OctarynServerPlayerState *state);

// Same contract as octaryn_server_player_step, moved against the map mesh.
OCTARYN_MAP_WORLD_API int octaryn_server_map_world_step(
    void *handle, const OctarynServerPlayerInput *input, double delta_seconds,
    OctarynServerPlayerState *state, OctarynServerPlayerTickResult *result);

OCTARYN_MAP_WORLD_API unsigned long long
octaryn_server_map_world_triangle_count(void *handle);

} // extern "C"
