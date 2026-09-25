#pragma once
#include <cstdint>

namespace b3 { class World; }

namespace octaryn::client::physics {

struct PhysicsBodyDesc {
  float position[3]{};
  float rotation[4]{}; // XYZW quaternion; identity is {0,0,0,1}.
  float half_extents[3]{0.5f, 0.5f, 0.5f};
  bool dynamic{true};
};

// Thin owner wrapper over Box3D: one world, fixed 8ms steps, box bodies.
// Games built on the engine own their gameplay; this is the raw backend.
class PhysicsWorld {
public:
  PhysicsWorld();
  ~PhysicsWorld();
  PhysicsWorld(const PhysicsWorld&) = delete;
  PhysicsWorld& operator=(const PhysicsWorld&) = delete;

  void set_gravity(float x, float y, float z);
  std::uint32_t create_body(const PhysicsBodyDesc& desc);
  bool body_position(std::uint32_t body, float (&position)[3]) const;
  void step(double elapsed_seconds);
  unsigned step_count() const { return steps_; }

private:
  b3World* world_{};
  unsigned steps_{};
  double accumulator_{};
};

}
