#pragma once

void app_persist_world_clock(void);
void app_mark_world_clock_dirty(void);
void app_persist_world_clock_if_due(void);
void app_refresh_world_clock_snapshot(void);
void app_init_world_clock(void);
void app_step_world_clock_hours(int delta_hours);
