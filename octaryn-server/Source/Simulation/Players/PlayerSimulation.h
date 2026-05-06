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

struct OctarynServerPlayerInputProcessPlan {
  uint32_t should_continue;
  uint32_t should_tick;
  uint32_t reason;
  int32_t handle_result;
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

struct OctarynServerPlayerSaveDecision {
  uint32_t should_save;
  uint32_t reserved;
  double seconds_since_last_save;
};

struct OctarynServerPlayerSession {
  OctarynServerPlayerState state;
  OctarynServerPlayerSaveState last_saved;
  double seconds_since_last_save;
  uint32_t loaded_from_save;
  uint32_t reserved;
};

struct OctarynServerPlayerSessionSaveResult {
  uint32_t should_save;
  uint32_t reserved;
  OctarynServerPlayerSaveState save_state;
};

struct OctarynServerPlayerSpawnAlignment {
  uint32_t aligned;
  uint32_t adjusted;
  int32_t surface_y;
  uint16_t surface_block;
  uint16_t reserved;
};

struct OctarynServerPlayerTickResult {
  uint32_t tick_input;
  uint32_t reserved;
  float delta_x;
  float delta_y;
  float delta_z;
};

OCTARYN_SERVER_PLAYER_SIMULATION_API float
octaryn_server_player_spawn_eye_height();

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_default_state(OctarynServerPlayerState *state);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_state_from_save(float x, float y, float z, float pitch,
                                      float yaw, uint16_t selected_block,
                                      OctarynServerPlayerState *state);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_save_state_from_state(
    const OctarynServerPlayerState *state,
    OctarynServerPlayerSaveState *save_state);

OCTARYN_SERVER_PLAYER_SIMULATION_API uint32_t
octaryn_server_player_save_state_changed(
    const OctarynServerPlayerSaveState *previous,
    const OctarynServerPlayerSaveState *current);

OCTARYN_SERVER_PLAYER_SIMULATION_API uint32_t
octaryn_server_player_should_save_state(
    const OctarynServerPlayerSaveState *previous,
    const OctarynServerPlayerSaveState *current, double seconds_since_last_save,
    uint32_t force);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_save_decision(
    const OctarynServerPlayerSaveState *previous,
    const OctarynServerPlayerSaveState *current, double seconds_since_last_save,
    double delta_seconds, uint32_t force,
    OctarynServerPlayerSaveDecision *decision);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_session_from_state(
    const OctarynServerPlayerState *state, uint32_t loaded_from_save,
    OctarynServerPlayerSession *session);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_session_align_spawn_with_block_store(
    OctarynServerPlayerSession *session, void *block_store,
    octaryn_server_player_generated_block_fn generated_block,
    octaryn_server_player_block_solid_fn is_solid_block, void *context,
    OctarynServerPlayerSpawnAlignment *alignment);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_session_step_with_block_store(
    const OctarynServerPlayerInput *input, double delta_seconds,
    void *block_store, octaryn_server_player_generated_block_fn generated_block,
    octaryn_server_player_block_solid_fn is_solid_block, void *context,
    OctarynServerPlayerSession *session, OctarynServerPlayerTickResult *result);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_session_save_decision(
    OctarynServerPlayerSession *session, double delta_seconds, uint32_t force,
    OctarynServerPlayerSessionSaveResult *result);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_session_note_saved(
    OctarynServerPlayerSession *session,
    const OctarynServerPlayerSaveState *save_state);

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

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_step(const OctarynServerPlayerInput *input,
                           double delta_seconds,
                           octaryn_server_player_block_query_fn block_query,
                           void *context, OctarynServerPlayerState *state,
                           OctarynServerPlayerTickResult *result);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_step_with_block_store(
    const OctarynServerPlayerInput *input, double delta_seconds,
    void *block_store, octaryn_server_player_generated_block_fn generated_block,
    octaryn_server_player_block_solid_fn is_solid_block, void *context,
    OctarynServerPlayerState *state, OctarynServerPlayerTickResult *result);

OCTARYN_SERVER_PLAYER_SIMULATION_API uint32_t
octaryn_server_player_has_input_intent(const OctarynServerPlayerInput *input);

OCTARYN_SERVER_PLAYER_SIMULATION_API int32_t
octaryn_server_player_read_input_intent_file(
    const char *intent_path, OctarynServerPlayerInputIntent *intent);

OCTARYN_SERVER_PLAYER_SIMULATION_API int32_t
octaryn_server_player_plan_input_intent(
    int32_t intent_read_result, uint32_t allow_transient_invalid,
    const OctarynServerPlayerInputIntent *intent,
    OctarynServerPlayerInputProcessPlan *plan);

OCTARYN_SERVER_PLAYER_SIMULATION_API int
octaryn_server_player_idle(OctarynServerPlayerState *state);
}
