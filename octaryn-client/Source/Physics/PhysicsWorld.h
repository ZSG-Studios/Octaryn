#pragma once
#include <cstdint>

namespace octaryn::client::physics {

struct PhysicsBodyDesc {
  float position[3]{};
  float rotation[4]{0, 0, 0, 1}; // XYZW quaternion.
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
  // Returns a non-zero opaque body handle, or 0 on rejection.
  std::uint32_t create_body(const PhysicsBodyDesc& desc);
  bool body_position(std::uint32_t body, float (&position)[3]) const;
  void step(double elapsed_seconds);
  unsigned step_count() const { return steps_; }

private:
  void* world_{}; // Opaque Box3D world handle.
  unsigned steps_{};
  double accumulator_{};
};

}
