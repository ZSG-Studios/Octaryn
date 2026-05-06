#pragma once

#include "octaryn_shared_abi_types.h"

#include <cstdint>

#if defined(_WIN32)
#define OCTARYN_SERVER_HOST_API __declspec(dllexport)
#else
#define OCTARYN_SERVER_HOST_API __attribute__((visibility("default")))
#endif

extern "C" {

struct octaryn_server_host_startup_policy {
  uint32_t disable_game_modules;
  uint32_t live_process_stream;
  uint32_t live_stream_interval_ms;
};

using octaryn_server_host_live_stream_iteration_fn =
    int32_t (*)(void *context);

OCTARYN_SERVER_HOST_API uint32_t
octaryn_server_host_environment_enabled(const char *name);

OCTARYN_SERVER_HOST_API octaryn_server_host_startup_policy
octaryn_server_host_get_startup_policy();

OCTARYN_SERVER_HOST_API void
octaryn_server_host_create_startup_frame(octaryn_host_frame_snapshot *frame);

OCTARYN_SERVER_HOST_API int32_t octaryn_server_host_run_live_stream_loop(
    uint32_t interval_ms, octaryn_server_host_live_stream_iteration_fn iteration,
    void *context);
}
