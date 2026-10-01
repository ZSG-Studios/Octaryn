#include "CharacterMotion.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

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
constexpr int kSequence = 240;

void fail(const char *reason) {
  std::printf("character_motion_probe=failed reason=%s\n", reason);
}

// Scripted command mix: walk, turn, sprint, jump and a short fly segment.
octaryn::character_motion::Input scripted_input(int frame) {
  octaryn::character_motion::Input input{};
  input.controller = 1;
  input.move_z = (frame % 120) < 90 ? 1.0f : -0.5f;
  input.move_x = (frame % 60) < 30 ? 0.25f : -0.25f;
  input.camera_yaw = 0.6f + 0.01f * static_cast<float>(frame);
  input.camera_pitch = -0.25f;
  input.relative_mouse = 1;
  if (frame % 45 == 0) input.flags |= 1u; // JumpFlag
  if ((frame % 80) < 40) input.flags |= 2u; // SprintFlag
  if (frame >= 180 && frame < 200) input.flags |= 4u; // FlyModeFlag
  return input;
}

// Bitwise field comparison: reconciliation relies on identical bits, not
// approximate equality. Padding is never compared.
bool identical(const octaryn::character_motion::State &a,
               const octaryn::character_motion::State &b) {
  const auto same = [](float x, float y) {
    std::uint32_t xb, yb;
    std::memcpy(&xb, &x, sizeof(xb));
    std::memcpy(&yb, &y, sizeof(yb));
    return xb == yb;
  };
  return same(a.x, b.x) && same(a.y, b.y) && same(a.z, b.z) &&
         same(a.pitch, b.pitch) && same(a.yaw, b.yaw) &&
         same(a.velocity_x, b.velocity_x) && same(a.velocity_y, b.velocity_y) &&
         same(a.velocity_z, b.velocity_z) && a.is_on_ground == b.is_on_ground &&
         a.control_mode == b.control_mode && a.jump_held == b.jump_held;
}

octaryn::character_motion::State run_script(
    octaryn::character_motion::State state,
    const octaryn::character_motion::MeshCollision &mesh, int from, int to) {
  for (int frame = from; frame < to; ++frame) {
    auto input = scripted_input(frame);
    octaryn::character_motion::step_on_mesh(input, kDt, state, mesh);
  }
  return state;
}

// The property network reconciliation spends: same seed plus same commands
// must give the same bits, whether stepped continuously or replayed from a
// mid-stream snapshot after a collision-world cache rebuild.
bool verify_determinism(const octaryn::character_motion::MeshCollision &mesh) {
  octaryn::character_motion::State seed{};
  seed.y = kRestEyeY;
  seed.yaw = 0.6f;
  seed.pitch = -0.25f;

  const auto continuous = run_script(seed, mesh, 0, kSequence);
  const auto repeated = run_script(seed, mesh, 0, kSequence);
  if (!identical(continuous, repeated)) {
    fail("determinism_repeat");
    return false;
  }

  const auto snapshot = run_script(seed, mesh, 0, kSequence / 2);
  const auto resumed = run_script(snapshot, mesh, kSequence / 2, kSequence);
  if (!identical(continuous, resumed)) {
    fail("determinism_resume");
    return false;
  }

  // A rebuilt collision cache (the reconcile path re-acquires on map swaps)
  // must not change a single bit of the replayed outcome.
  octaryn::character_motion::release_mesh_collision(mesh);
  const auto after_cache_rebuild = run_script(snapshot, mesh, kSequence / 2, kSequence);
  if (!identical(continuous, after_cache_rebuild)) {
    fail("determinism_cache_rebuild");
    return false;
  }
  return true;
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

  if (!verify_determinism(mesh)) {
    return 1;
  }

  octaryn::character_motion::release_mesh_collision(mesh);
  std::printf(
      "character_motion_probe=passed settle_y=%.3f lip_peak=%.3f z=%.3f "
      "jump_peak=%.3f determinism=bit_exact backend=box3d\n",
      kRestEyeY, peak_y, state.z, jump_peak);
  return 0;
}
