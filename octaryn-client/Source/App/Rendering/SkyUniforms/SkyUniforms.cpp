#include "SkyUniforms.h"

#include <algorithm>
#include <cmath>

namespace octaryn_client_app {

namespace {

constexpr float kPi = 3.14159265358979323846f;

float smootherstep(float edge0, float edge1, float value) {
  const float t = clamp01((value - edge0) / (edge1 - edge0));
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

void normalize3(float vector[3]) {
  const float length = std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] +
                                 vector[2] * vector[2]);
  if (length <= 0.000001f) {
    vector[0] = 0.0f;
    vector[1] = 1.0f;
    vector[2] = 0.0f;
    return;
  }

  vector[0] /= length;
  vector[1] /= length;
  vector[2] /= length;
}

} // namespace

float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

sky_uniforms
build_sky_uniforms(const server_world_time_state &world_time,
                   const camera &camera,
                   const runtime_controls &controls) {
  const float day_fraction = clamp01(world_time.day_fraction);
  const float angle = day_fraction * kPi * 2.0f - kPi * 0.5f;
  float sun_direction[3] = {
      std::cos(angle),
      std::sin(angle),
      0.0f,
  };
  normalize3(sun_direction);

  const float day_visibility = smootherstep(-0.10f, 0.25f, sun_direction[1]);
  const float twilight = smootherstep(-0.28f, 0.02f, sun_direction[1]) *
                         (1.0f - smootherstep(0.06f, 0.36f, sun_direction[1]));
  sky_uniforms uniforms{};
  uniforms.light_direction_and_sky_visibility[0] = -sun_direction[0];
  uniforms.light_direction_and_sky_visibility[1] = -sun_direction[1];
  uniforms.light_direction_and_sky_visibility[2] = -sun_direction[2];
  uniforms.light_direction_and_sky_visibility[3] =
      std::max(0.08f, day_visibility);
  uniforms.twilight_celestial_cloud_time[0] = twilight;
  uniforms.twilight_celestial_cloud_time[1] = day_visibility;
  uniforms.twilight_celestial_cloud_time[2] =
      controls.sky_gradient_enabled != 0u ? 1.0f : 0.0f;
  uniforms.twilight_celestial_cloud_time[3] =
      static_cast<float>(std::fmod(world_time.total_seconds, 86400.0));
  uniforms.camera_position_and_cloud_height[0] = camera.position[0];
  uniforms.camera_position_and_cloud_height[1] = camera.position[1];
  uniforms.camera_position_and_cloud_height[2] = camera.position[2];
  uniforms.camera_position_and_cloud_height[3] = 192.0f;
  uniforms.celestial_toggles[0] = controls.stars_enabled != 0u ? 1.0f : 0.0f;
  uniforms.celestial_toggles[1] = controls.sun_enabled != 0u ? 1.0f : 0.0f;
  uniforms.celestial_toggles[2] = controls.moon_enabled != 0u ? 1.0f : 0.0f;
  uniforms.celestial_toggles[3] = 0.0f;
  return uniforms;
}

} // namespace octaryn_client_app
