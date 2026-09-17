#include "CharacterMotion.h"
#include "PlayerMovement.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace octaryn::character_motion {

void step(const Input &input, float deltaSeconds, State &state,
 SolidQuery query, void *context) {
 if (!query) return;
 constexpr float Pi = 3.14159265358979323846f;
 constexpr float TwoPi = Pi * 2.0f;
 const float pitch = std::clamp(
 std::isfinite(input.camera_pitch) ? input.camera_pitch : state.pitch,
 -Pi * 0.5f + FLT_EPSILON, Pi * 0.5f - FLT_EPSILON);
 float yaw = std::isfinite(input.camera_yaw) ? input.camera_yaw : state.yaw;
 yaw = std::fmod(yaw + Pi, TwoPi);
 if (yaw < 0.0f) yaw += TwoPi;
 yaw -= Pi;
 float remaining = std::isfinite(deltaSeconds) && deltaSeconds > 0.0f
 ? std::min(deltaSeconds, 0.25f) : 0.0f;
 if (remaining <= 0.0f) {
 state.pitch = pitch;
 state.yaw = yaw;
 state.velocity_x = state.velocity_y = state.velocity_z = 0.0f;
 return;
 }
 while (remaining > 0.0f) {
 const float dt = std::min(remaining, 0.05f);
 remaining -= dt;
 if ((input.flags & (1u << 2u)) != 0u)
 move_fly(input, dt, state, pitch, yaw);
 else
 move_walk(input, dt, state, pitch, yaw, query, context);
 }
}

} // namespace octaryn::character_motion
