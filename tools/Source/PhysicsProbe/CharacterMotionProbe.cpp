#include "CharacterMotion.h"

#include <cmath>
#include <cstdio>

// Floor at y=0 with a 0.2m lip patch at z=[-2.4,-2.0]: standing, settling,
// step-up and jump behavior through the Box3D character mover.
namespace {

const float kSoup[] = {
    // Floor quad, +Y facing, spanning x=[-6,6] z=[-10,6].
    -6.0f, 0.0f, 6.0f, 6.0f, 0.0f, 6.0f, 6.0f, 0.0f, -10.0f,
    -6.0f, 0.0f, 6.0f, 6.0f, 0.0f, -10.0f, -6.0f, 0.0f, -10.0f,
    // Raised lip patch (0.2m) at z=[-2.4,-2.0].
    -6.0f, 0.2f, -2.0f, 6.0f, 0.2f, -2.0f, 6.0f, 0.2f, -2.4f,
    -6.0f, 0.2f, -2.0f, 6.0f, 0.2f, -2.4f, -6.0f, 0.2f, -2.4f,
};
const uint32_t kIndices[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

constexpr float kDt = 1.0f / 60.0f;
constexpr float kRestEyeY = 1.62f;

void fail(const char *reason) {
  std::printf("character_motion_probe=failed reason=%s\n", reason);
}

} // namespace

int main() {
  const octaryn::character_motion::MeshCollision mesh{
      kSoup, sizeof(kSoup) / sizeof(kSoup[0]), kIndices,
      sizeof(kIndices) / sizeof(kIndices[0])};

  // Idle drop: spawn slightly above the floor and settle onto it.
  octaryn::character_motion::Input input{};
  octaryn::character_motion::State state{};
  state.y = kRestEyeY + 0.4f;
  for (int frame = 0; frame < 60; ++frame) {
    octaryn::character_motion::step_on_mesh(input, kDt, state, mesh);
  }
  if (state.is_on_ground != 1u) {
    fail("settle_grounded");
    return 1;
  }
  if (std::abs(state.y - kRestEyeY) > 0.05f) {
    std::printf("character_motion_probe=failed reason=settle_y y=%.3f\n",
                state.y);
    return 1;
  }

  // Walk forward over the lip: must cross it and return to floor height.
  input.move_z = 1.0f;
  float peak_y = state.y;
  for (int frame = 0; frame < 90; ++frame) {
    octaryn::character_motion::step_on_mesh(input, kDt, state, mesh);
    peak_y = std::fmax(peak_y, state.y);
  }
  if (state.is_on_ground != 1u) {
    fail("walk_grounded");
    return 1;
  }
  if (state.z > -3.5f) {
    std::printf("character_motion_probe=failed reason=lip_crossed z=%.3f\n",
                state.z);
    return 1;
  }
  if (std::abs(state.y - kRestEyeY) > 0.05f) {
    std::printf("character_motion_probe=failed reason=walk_y y=%.3f\n",
                state.y);
    return 1;
  }

  // Jump: one held frame must lift and land back on the floor.
  input.move_z = 0.0f;
  input.flags = 1u; // JumpFlag
  octaryn::character_motion::step_on_mesh(input, kDt, state, mesh);
  input.flags = 0u;
  float jump_peak = state.y;
  for (int frame = 0; frame < 90; ++frame) {
    octaryn::character_motion::step_on_mesh(input, kDt, state, mesh);
    jump_peak = std::fmax(jump_peak, state.y);
  }
  if (state.is_on_ground != 1u) {
    fail("jump_landed");
    return 1;
  }
  if (jump_peak < kRestEyeY + 0.6f) {
    std::printf("character_motion_probe=failed reason=jump_peak y=%.3f\n",
                jump_peak);
    return 1;
  }

  octaryn::character_motion::release_mesh_collision(mesh);
  std::printf(
      "character_motion_probe=passed settle_y=%.3f lip_peak=%.3f z=%.3f "
      "jump_peak=%.3f backend=box3d\n",
      kRestEyeY, peak_y, state.z, jump_peak);
  return 0;
}
