#include "MeshCharacterStep.h"

#include "CharacterGeometry.h"
#include "CharacterMotionLimits.h"
#include "PlayerMovement.h"
#include "CharacterBodyPressure.h"

#include <box3d/box3d.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>

namespace octaryn::character_motion {
namespace {

constexpr uint32_t JumpFlag = 1u << 0u;
constexpr uint32_t SprintFlag = 1u << 1u;
constexpr uint32_t WalkMode = 0u;

constexpr float AirAcceleration = 6.0f;
constexpr float MaxSlopeCosine = 0.70710678f;
constexpr float GroundProbeDistance = 0.15f;
constexpr int MoveIterations = 5;
constexpr int PlaneCapacity = 8;
constexpr float MoveTolerance = 0.005f;
constexpr float IdleContactTolerance = MoveTolerance + 0.0001f;

struct Vec3 {
  float x;
  float y;
  float z;
};

struct MoveContext {
  b3CollisionPlane planes[PlaneCapacity]{};
  int plane_count{};
};

bool collect_planes(b3ShapeId, const b3PlaneResult *results, int count,
                   void *context) {
  auto *planes = static_cast<MoveContext *>(context);
  for (int i = 0; i < count && planes->plane_count < PlaneCapacity; ++i) {
    planes->planes[planes->plane_count] = {results[i].plane, FLT_MAX, 0.0f,
                                           true};
    planes->plane_count += 1;
  }
  return true;
}

float horizontal_length(float x, float z) { return std::sqrt(x * x + z * z); }

Vec3 move_yaw_relative(float x, float z, float yaw) {
  const float yaw_sine = std::sin(yaw);
  const float yaw_cosine = std::cos(yaw);
  return Vec3{yaw_cosine * x + yaw_sine * z, 0.0f,
              -(yaw_cosine * z) + yaw_sine * x};
}

// Capsule spans feet (origin) to CollisionHeight; queries stay relative to
// the origin so the map soup keeps world-space precision.
b3Capsule character_capsule() {
  return {{0.0f, CollisionRadius, 0.0f},
          {0.0f, CollisionHeight - CollisionRadius, 0.0f},
          CollisionRadius};
}

// Cast the capsule by delta, moving origin by the achieved fraction.
float cast_mover(b3WorldId world, b3Pos &origin, const b3Capsule &capsule,
                 b3Vec3 delta) {
  if (b3Dot(delta, delta) < 1e-12f) {
    return 1.0f;
  }
  const b3QueryFilter filter = b3DefaultQueryFilter();
  const float fraction =
      b3World_CastMover(world, origin, &capsule, delta, filter, nullptr,
                        nullptr);
  origin = origin + fraction * delta;
  return fraction;
}

bool ground_probe(b3WorldId world, b3Pos origin, const b3Capsule &capsule,
                  float *separation = nullptr) {
  const b3Pos start = origin + capsule.center1;
  const b3Vec3 down{0.0f, -(capsule.radius + GroundProbeDistance), 0.0f};
  const b3QueryFilter filter = b3DefaultQueryFilter();
  const b3RayResult hit = b3World_CastRayClosest(world, start, down, filter);
  if (separation && hit.hit) {
    *separation = b3Dot(b3SubPos(start, hit.point), hit.normal) - capsule.radius;
  }
  return hit.hit && hit.normal.y >= MaxSlopeCosine;
}

bool idle_contact(b3WorldId world, b3Pos origin, const b3Capsule &capsule) {
  bool clear = true;
  b3World_CollideMover(
      world, origin, &capsule, b3DefaultQueryFilter(),
      [](b3ShapeId, const b3PlaneResult *planes, int count, void *value) {
        for (int i = 0; i < count; ++i) {
          if (planes[i].plane.offset > IdleContactTolerance) {
            *static_cast<bool *>(value) = false;
            return false;
          }
        }
        return true;
      }, &clear);
  return clear;
}

// Depenetrate through the plane solver, then slide toward the target.
void solve_move(b3WorldId world, b3Pos &origin, const b3Capsule &capsule,
                b3Pos target) {
  for (int iteration = 0; iteration < MoveIterations; ++iteration) {
    MoveContext context;
    const b3QueryFilter filter = b3DefaultQueryFilter();
    b3World_CollideMover(world, origin, &capsule, filter, collect_planes,
                         &context);
    const b3Vec3 to_target = b3SubPos(target, origin);
    if (b3Dot(to_target, to_target) < MoveTolerance * MoveTolerance) {
      return;
    }
    const b3PlaneSolverResult solved =
        b3SolvePlanes(to_target, context.planes, context.plane_count);
    const b3Pos before = origin;
    cast_mover(world, origin, capsule, solved.delta);
    const b3Vec3 applied = b3SubPos(origin, before);
    if (b3Dot(applied, applied) < MoveTolerance * MoveTolerance) {
      return;
    }
  }
}

// Lift over map stairs and curbs when the direct slide was blocked. Returns
// false with the origin restored when no standable landing exists.
bool try_step_up(b3WorldId world, b3Pos &origin, const b3Capsule &capsule,
                 b3Vec3 horizontal) {
  const b3Pos saved = origin;
  if (cast_mover(world, origin, capsule, {0.0f, StepUpHeight, 0.0f}) < 1.0f) {
    origin = saved;
    return false;
  }
  cast_mover(world, origin, capsule, horizontal);
  if (cast_mover(world, origin, capsule,
                 {0.0f, -(StepUpHeight + 0.05f), 0.0f}) >= 1.0f) {
    origin = saved;
    return false;
  }
  if (!ground_probe(world, origin, capsule)) {
    origin = saved;
    return false;
  }
  return true;
}

} // namespace

bool move_walk_on_mesh(const Input &input, float dt, State &state,
                       float pitch, float yaw, MeshCollisionWorld *world) {
  if (dt <= 0.0f || world == nullptr) {
    state.pitch = pitch;
    state.yaw = yaw;
    state.velocity_x = 0.0f;
    state.velocity_y = 0.0f;
    state.velocity_z = 0.0f;
    state.control_mode = WalkMode;
    return true;
  }
  const b3WorldId physics = world->world;

  const Vec3 position{state.x, state.y, state.z};
  const float speed = (input.flags & SprintFlag) != 0u
                          ? SprintWalkSpeedBlocksPerSecond
                          : WalkSpeedBlocksPerSecond;
  const float input_length = horizontal_length(input.move_x, input.move_z);
  const float input_scale = input_length > 1.0f ? 1.0f / input_length : 1.0f;
  const Vec3 horizontal_target =
      move_yaw_relative(input.move_x * input_scale * speed,
                        input.move_z * input_scale * speed, yaw);

  const b3Capsule capsule = character_capsule();
  b3Pos origin{state.x, state.y - EyeOffset, state.z};

  float support_separation = FLT_MAX;
  const bool was_grounded = state.velocity_y <= 0.1f &&
                            ground_probe(physics, origin, capsule, &support_separation);
  float velocity_x =
      was_grounded
          ? horizontal_target.x
          : state.velocity_x + (horizontal_target.x - state.velocity_x) *
                                   std::min(1.0f, AirAcceleration * dt);
  float velocity_z =
      was_grounded
          ? horizontal_target.z
          : state.velocity_z + (horizontal_target.z - state.velocity_z) *
                                   std::min(1.0f, AirAcceleration * dt);
  float velocity_y = state.velocity_y;
  const bool jump_held = (input.flags & JumpFlag) != 0u;
  const bool jump_requested =
      jump_held && state.jump_held == 0u && was_grounded;
  if (jump_requested) {
    velocity_y = JumpSpeed;
  }
  // Retain a settled idle pose while a fresh probe still finds walkable support.
  // Gravity projection creeps downhill; a redundant cast from contact can miss
  // an initially overlapping floor. Moving and newly landing capsules still solve.
  const bool idle_supported =
      state.is_on_ground && was_grounded && !jump_requested &&
      input_length == 0.0f && velocity_y <= 0.0f &&
      std::abs(support_separation) <= IdleContactTolerance &&
      idle_contact(physics, origin, capsule);
  if (idle_supported) {
    velocity_y = 0.0f;
  } else {
    velocity_y -= Gravity * dt;
  }

  const b3Vec3 desired{velocity_x * dt, velocity_y * dt, velocity_z * dt};
  const b3Vec3 desired_horizontal{desired.x, 0.0f, desired.z};
  const b3Pos start = origin;
  if (!idle_supported) {
    solve_move(physics, origin, capsule, origin + desired);
  }

  // A blocked slide on walkable stairs still has ground under a lifted move.
  const b3Vec3 slid = b3SubPos(origin, start);
  apply_character_body_pressure(physics,origin,capsule,{velocity_x,0,velocity_z},dt);
  if (was_grounded && !jump_requested &&
      horizontal_length(desired.x, desired.z) > MoveTolerance &&
      horizontal_length(slid.x, slid.z) <
          0.5f * horizontal_length(desired.x, desired.z)) {
    try_step_up(physics, origin, capsule, desired_horizontal);
  } else if (!idle_supported && was_grounded && !jump_requested && velocity_y <= 0.0f) {
    // Stick to floors and downhill slopes instead of skipping over them.
    cast_mover(physics, origin, capsule, {0.0f, -StepDownDepth, 0.0f});
  }

  state.x = origin.x;
  state.y = origin.y + EyeOffset;
  state.z = origin.z;
  state.pitch = pitch;
  state.yaw = yaw;
  // A wall-top contact can report support while the character is still
  // rising. Landing must not cancel upward jump momentum at that edge.
  const bool next_grounded =
      velocity_y <= 0.0f && ground_probe(physics, origin, capsule);
  const float delta_y = state.y - position.y;
  state.velocity_x = (state.x - position.x) / dt;
  if (next_grounded) {
    state.velocity_y = 0.0f;
  } else if (!jump_requested && velocity_y <= 0.0f && delta_y > 0.0f) {
    state.velocity_y = std::min(velocity_y, 0.0f);
  } else {
    state.velocity_y = velocity_y;
  }
  state.velocity_z = (state.z - position.z) / dt;
  state.is_on_ground = next_grounded ? 1u : 0u;
  state.control_mode = WalkMode;
  state.jump_held = jump_held ? 1u : 0u;
  return true;
}

} // namespace octaryn::character_motion
