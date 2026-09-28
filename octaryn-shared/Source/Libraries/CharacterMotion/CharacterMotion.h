#pragma once

#include <cstddef>
#include <cstdint>

namespace octaryn::character_motion {
class MeshCollisionScene;

struct Input {
 uint32_t flags;
 uint32_t controller;
 float move_x, move_y, move_z;
 float camera_x, camera_y, camera_z;
 float camera_pitch, camera_yaw;
 int32_t relative_mouse;
};

struct State {
 float x, y, z;
 float pitch, yaw;
 float velocity_x, velocity_y, velocity_z;
 uint32_t is_on_ground;
 uint32_t control_mode;
 uint16_t jump_held;
};

// World-space triangle soup: positions are xyz float triples in glTF +Y up space.
struct MeshCollision {
  const float *positions;
  size_t position_count;
  const uint32_t *indices;
  size_t index_count;
  MeshCollisionScene* scene{};
};

// Steps against one static triangle-soup map instead of voxel blocks. The
// collision backend is Box3D behind MeshCollisionWorld.
void step_on_mesh(const Input &input, float deltaSeconds, State &state,
                  const MeshCollision &mesh);

// Drop the cached collision world before the backing map soup is destroyed;
// prevents a later map allocation from reusing an address with stale geometry.
void release_mesh_collision(const MeshCollision &mesh);

} // namespace octaryn::character_motion
