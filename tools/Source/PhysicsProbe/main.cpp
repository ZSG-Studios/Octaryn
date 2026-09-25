#include "PhysicsWorld.h"
#include <cmath>
#include <cstdio>

int main() {
  octaryn::client::physics::PhysicsWorld physics;
  octaryn::client::physics::PhysicsBodyDesc floor;
  floor.dynamic = false;
  floor.half_extents[0] = 50.0f;
  floor.half_extents[2] = 50.0f;
  const auto static_body = physics.create_body(floor);
  octaryn::client::physics::PhysicsBodyDesc box;
  box.position[1] = 10.0f;
  const auto falling = physics.create_body(box);
  if (!static_body || !falling) { std::puts("physics_probe=failed reason=body_creation"); return 1; }
  for (int frame = 0; frame < 240; ++frame) physics.step(1.0 / 60.0);
  float position[3]{};
  if (!physics.body_position(falling, position)) { std::puts("physics_probe=failed reason=body_query"); return 1; }
  const bool rested = std::abs(position[1] - 1.0f) < 0.05f;
  std::printf("physics_probe=passed box_y=%.3f steps=%u rested=%d backend=box3d\n",
              position[1], physics.step_count(), rested ? 1 : 0);
  return rested ? 0 : 1;
}
