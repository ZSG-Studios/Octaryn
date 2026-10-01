#pragma once

#include <cstdint>

#if defined(_WIN32)
#define OCTARYN_SERVER_WORLD_PERSISTENCE_API __declspec(dllexport)
#else
#define OCTARYN_SERVER_WORLD_PERSISTENCE_API                                     __attribute__((visibility("default")))
#endif

extern "C" {

struct octaryn_server_persistence_player_state {
  float x;
  float y;
  float z;
  float pitch;
  float yaw;
};

struct octaryn_server_persistence_player_file_entry {
  int32_t player_id;
  octaryn_server_persistence_player_state state;
};


OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_read_player_file(
    const char *path, octaryn_server_persistence_player_state *state);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_write_player_file(
    const char *path, const octaryn_server_persistence_player_state *state);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_read_player_directory_entry(
    const char *directory, int32_t player_id,
    octaryn_server_persistence_player_state *state);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_write_player_directory_entry(
    const char *directory, int32_t player_id,
    const octaryn_server_persistence_player_state *state);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_player_directory_path(const char *directory,
                                                 int32_t player_id, char *path,
                                                 uint64_t path_capacity,
                                                 uint64_t *required_size);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_world_root_path_for_environment(
    const char *world_blocks_path, const char *build_preset, char *path,
    uint64_t path_capacity, uint64_t *required_size);

OCTARYN_SERVER_WORLD_PERSISTENCE_API int32_t
octaryn_server_persistence_player_directory_path_for_environment(
    const char *player_save_root, const char *world_blocks_path,
    const char *build_preset, char *path, uint64_t path_capacity,
    uint64_t *required_size);
}
