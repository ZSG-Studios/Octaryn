#pragma once

#include <SDL3/SDL_stdinc.h>

namespace octaryn::physics {

bool jolt_physics_service_startup();
void jolt_physics_service_tick(double frame_seconds);
void jolt_physics_service_shutdown();
bool jolt_physics_service_initialized();
Uint32 jolt_physics_service_body_count();
int jolt_physics_service_active_job_count();

} // namespace octaryn::physics
