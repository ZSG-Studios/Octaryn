#pragma once

#include "octaryn_native_schedule_runtime.h"

#if defined(_WIN32)
#define OCTARYN_SERVER_AUTHORITY_TICK_API __declspec(dllexport)
#else
#define OCTARYN_SERVER_AUTHORITY_TICK_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*octaryn_server_authority_tick_execute_fn)(void *context);

typedef struct octaryn_server_authority_tick_callbacks {
  octaryn_server_authority_tick_execute_fn command_drain;
  void *command_drain_context;
  octaryn_server_authority_tick_execute_fn player_tick;
  void *player_context;
  octaryn_server_authority_tick_execute_fn world_time_tick;
  void *world_time_context;
} octaryn_server_authority_tick_callbacks;

OCTARYN_SERVER_AUTHORITY_TICK_API int octaryn_server_authority_tick_execute(
    void *schedule_runtime,
    const octaryn_server_authority_tick_callbacks *callbacks,
    octaryn_native_schedule_runtime_report *report);

OCTARYN_SERVER_AUTHORITY_TICK_API int
octaryn_server_authority_tick_validate_report(
    const octaryn_native_schedule_runtime_report *report);

#ifdef __cplusplus
}
#endif
