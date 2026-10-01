#pragma once

#include "octaryn_shared_abi_types.h"

#include <cstdint>

#if defined(_WIN32)
#if defined(OCTARYN_SESSION_STREAM_EXPORTS)
#define OCTARYN_SERVER_SESSION_STREAM_API __declspec(dllexport)
#else
#define OCTARYN_SERVER_SESSION_STREAM_API __declspec(dllimport)
#endif
#else
#define OCTARYN_SERVER_SESSION_STREAM_API __attribute__((visibility("default")))
#endif

extern "C" {

struct octaryn_server_chunk_view_intent {
  int32_t version;
  uint64_t epoch;
  int32_t center_chunk_x;
  int32_t center_chunk_z;
  uint32_t radius;
  uint32_t has_previous_window;
  int32_t previous_center_chunk_x;
  int32_t previous_center_chunk_z;
  uint32_t previous_radius;
};

struct octaryn_server_chunk_stream_process_tick_decision {
  uint32_t should_tick;
  uint32_t use_host_only_tick;
  uint32_t use_default_frame;
};

struct octaryn_server_chunk_stream_process_write_plan {
  uint32_t should_continue;
  uint32_t should_write;
  uint32_t use_previous_window;
  uint32_t reason;
  int32_t handle_result;
  int32_t center_chunk_x;
  int32_t center_chunk_z;
  uint32_t radius;
};

struct octaryn_server_chunk_stream_process_stage_plan {
  octaryn_server_chunk_stream_process_tick_decision tick;
  octaryn_server_chunk_stream_process_write_plan write;
};

struct octaryn_server_chunk_stream_write_decision {
  uint32_t use_previous_window;
  uint32_t should_write;
};

using octaryn_server_chunk_stream_process_tick_fn =
    int32_t (*)(void *context, const octaryn_host_frame_snapshot *frame);

OCTARYN_SERVER_SESSION_STREAM_API void *
octaryn_server_chunk_stream_write_tracker_create();

OCTARYN_SERVER_SESSION_STREAM_API void
octaryn_server_chunk_stream_write_tracker_destroy(void *tracker);

OCTARYN_SERVER_SESSION_STREAM_API octaryn_server_chunk_stream_write_decision
octaryn_server_chunk_stream_write_tracker_decide(
    void *tracker, uint32_t metadata_only, uint32_t submitted_block_commands,
    int32_t center_chunk_x, int32_t center_chunk_z, uint32_t radius,
    uint32_t has_previous_window, int32_t previous_center_chunk_x,
    int32_t previous_center_chunk_z, uint32_t previous_radius);

OCTARYN_SERVER_SESSION_STREAM_API void
octaryn_server_chunk_stream_write_tracker_note_written(void *tracker,
                                                       int32_t center_chunk_x,
                                                       int32_t center_chunk_z,
                                                       uint32_t radius);

OCTARYN_SERVER_SESSION_STREAM_API int32_t
octaryn_server_chunk_stream_read_view_intent(
    const char *intent_path, octaryn_server_chunk_view_intent *intent);

OCTARYN_SERVER_SESSION_STREAM_API int32_t
octaryn_server_chunk_stream_read_process_intent(
    const char *intent_path, uint32_t allow_transient_invalid,
    octaryn_server_chunk_view_intent *intent,
    octaryn_server_chunk_stream_process_write_plan *plan);

OCTARYN_SERVER_SESSION_STREAM_API int32_t
octaryn_server_chunk_stream_plan_process_write(
    void *tracker, int32_t intent_read_result, uint32_t allow_transient_invalid,
    const octaryn_server_chunk_view_intent *intent, uint32_t metadata_only,
    uint32_t submitted_block_commands,
    octaryn_server_chunk_stream_process_write_plan *plan);

OCTARYN_SERVER_SESSION_STREAM_API void
octaryn_server_chunk_stream_process_write_plan_note_written(
    void *tracker, const octaryn_server_chunk_stream_process_write_plan *plan);

OCTARYN_SERVER_SESSION_STREAM_API const char *
octaryn_server_chunk_stream_process_write_reason_name(uint32_t reason,
                                                      int32_t handle_result);

OCTARYN_SERVER_SESSION_STREAM_API
octaryn_server_chunk_stream_process_tick_decision
octaryn_server_chunk_stream_decide_process_tick(uint32_t has_player_input,
                                                uint32_t submitted_commands,
                                                uint32_t metadata_only);

OCTARYN_SERVER_SESSION_STREAM_API int32_t
octaryn_server_chunk_stream_plan_process_stage(
    void *tracker, const octaryn_server_chunk_view_intent *intent,
    uint32_t has_player_input, uint32_t submitted_commands,
    uint32_t metadata_only,
    octaryn_server_chunk_stream_process_stage_plan *plan);

OCTARYN_SERVER_SESSION_STREAM_API int32_t
octaryn_server_chunk_stream_execute_process_tick(
    const octaryn_server_chunk_stream_process_tick_decision *decision,
    const octaryn_host_frame_snapshot *frame,
    octaryn_server_chunk_stream_process_tick_fn host_only_tick,
    octaryn_server_chunk_stream_process_tick_fn tick, void *context);

OCTARYN_SERVER_SESSION_STREAM_API int32_t
octaryn_server_chunk_stream_create_process_frame(
    octaryn_host_frame_snapshot *frame);
}
