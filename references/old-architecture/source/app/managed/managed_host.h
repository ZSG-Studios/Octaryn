#pragma once

#include "app/managed/managed_abi.h"

bool app_managed_host_startup(void);
void app_managed_host_tick(const oct_managed_frame_snapshot_t* frame_snapshot);
void app_managed_host_drain_commands(void);
void app_managed_host_shutdown(void);
