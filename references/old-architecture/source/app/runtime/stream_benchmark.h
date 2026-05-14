#pragma once

#include "app/player/player.h"

void app_stream_benchmark_init(void);
bool app_stream_benchmark_active(void);
void app_stream_benchmark_step(player_t* player, float dt);
