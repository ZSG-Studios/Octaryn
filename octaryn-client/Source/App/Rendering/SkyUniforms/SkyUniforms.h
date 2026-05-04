#pragma once

#include "WorldStream.h"
#include "Camera.h"
#include "RuntimeControls.h"

namespace octaryn_client_app {

struct sky_uniforms {
  float light_direction_and_sky_visibility[4]{};
  float twilight_celestial_cloud_time[4]{};
  float camera_position_and_cloud_height[4]{};
  float celestial_toggles[4]{};
};

float clamp01(float value);
sky_uniforms
build_sky_uniforms(const server_world_time_state &world_time,
                   const camera &camera,
                   const runtime_controls &controls);

} // namespace octaryn_client_app
