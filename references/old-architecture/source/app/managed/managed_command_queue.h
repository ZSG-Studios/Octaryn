#pragma once

#include "app/managed/managed_abi.h"

void app_managed_command_queue_init(void);
void app_managed_command_queue_shutdown(void);
int app_managed_command_queue_enqueue(const oct_managed_native_command_t* command);
void app_managed_command_queue_drain(void);
