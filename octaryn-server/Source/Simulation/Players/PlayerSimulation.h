#pragma once

#include <cstdint>

#if defined(_WIN32)
#define OCTARYN_SERVER_PLAYER_SIMULATION_API __declspec(dllexport)
#else
#define OCTARYN_SERVER_PLAYER_SIMULATION_API                                   \
  __attribute__((visibility("default")))
#endif

extern "C" {

using octaryn_server_player_block_query_fn = uint32_t (*)(void *context,
                                                          int32_t x, int32_t y,
                                                          int32_t z);
using octaryn_server_player_generated_block_fn = uint16_t (*)(void *context,
                                                              int32_t x,
                                                              int32_t y,
                                                              int32_t z);
using octaryn_server_player_block_solid_fn = uint32_t (*)(void *context,
                                                          uint16_t block);

struct OctarynServerPlayerInput {
  uint32_t flags;
  uint32_t controller;
  float move_x;
  float move_y;
  float move_z;
  float camera_x;
  float camera_y;
  float camera_z;
  float camera_pitch;
  float camera_yaw;
  int32_t relative_mouse;
};

struct OctarynServerPlayerInputIntent {
  int32_t version;
  uint64_t frame_index;
  double delta_seconds;
  OctarynServerPlayerInput input;
};

struct OctarynServerPlayerState {
  float x;
  float y;
  float z;
  float pitch;
  float yaw;
  float velocity_x;
  float velocity_y;
  float velocity_z;
  uint32_t is_on_ground;
  uint32_t control_mode;
  uint16_t selected_block;
  uint16_t reserved;
};

struct OctarynServerPlayerSaveState {
  float x;
  float y;
  float z;
  float pitch;
  float yaw;
  uint16_t selected_block;
  uint16_t reserved;
};

struct OctarynServerPlayerSpawnAlignment {
  uint32_t aligned;
  uint32_t adjusted;
  int32_t surface_y;
  uint16_t surface_block;
  uint16_t reserved;
};

OCTARYN_SERVER_PLAYER_SIMULATION_API float
octaryn_server_player_spawn_eye_height();

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_default_state(OctarynServerPlayerState *state);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_state_from_save(float x, float y, float z, float pitch,
                                      float yaw, uint16_t selected_block,
                                      OctarynServerPlayerState *state);

OCTARYN_SERVER_PLAYER_SIMULATION_API uint32_t
octaryn_server_player_save_state_changed(
    const OctarynServerPlayerSaveState *previous,
    const OctarynServerPlayerSaveState *current);

OCTARYN_SERVER_PLAYER_SIMULATION_API int octaryn_server_player_align_spawn(
    OctarynServerPlayerState *state, uint32_t loaded_from_save,
    octaryn_server_player_block_query_fn block_query, void *context,
    OctarynServerPlayerSpawnAlignment *alignment);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_align_spawn_with_block_store(
    OctarynServerPlayerState *state, uint32_t loaded_from_save,
    void *block_store, octaryn_server_player_generated_block_fn generated_block,
    octaryn_server_player_block_solid_fn is_solid_block, void *context,
    OctarynServerPlayerSpawnAlignment *alignment);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_move(const OctarynServerPlayerInput *input,
                           double delta_seconds,
                           octaryn_server_player_block_query_fn block_query,
                           void *context, OctarynServerPlayerState *state);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_move_with_block_store(
    const OctarynServerPlayerInput *input, double delta_seconds,
    void *block_store, octaryn_server_player_generated_block_fn generated_block,
    octaryn_server_player_block_solid_fn is_solid_block, void *context,
    OctarynServerPlayerState *state);

OCTARYN_SERVER_PLAYER_SIMULATION_API uint32_t
octaryn_server_player_has_input_intent(const OctarynServerPlayerInput *input);

OCTARYN_SERVER_PLAYER_SIMULATION_API int32_t
octaryn_server_player_read_input_intent_file(
    const char *intent_path, OctarynServerPlayerInputIntent *intent);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_idle(OctarynServerPlayerState *state);
}
