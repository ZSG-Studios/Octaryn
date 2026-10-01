#include "MeshItemStep.h"

#include <box3d/box3d.h>

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace octaryn::item_motion {
namespace {

constexpr int MoveIterations = 3;
constexpr int PlaneCapacity = 8;
constexpr float MoveTolerance = 0.001f;
constexpr float GroundProbeDistance = 0.08f;
constexpr float MinSupportNormalY = 0.5f;

struct PlaneContext {
  b3CollisionPlane planes[PlaneCapacity]{};
  int count{};
};

bool collect_planes(b3ShapeId, const b3PlaneResult *results, int count,
                    void *context) {
  auto *planes = static_cast<PlaneContext *>(context);
  for (int i = 0; i < count && planes->count < PlaneCapacity; ++i) {
    planes->planes[planes->count] = {results[i].plane, FLT_MAX, 0.0f, true};
    planes->count += 1;
  }
  return true;
}

b3Capsule item_capsule(float radius) {
  const float half_segment = 0.5f * radius;
  return {{0.0f, -half_segment, 0.0f}, {0.0f, half_segment, 0.0f}, radius};
}

float length(b3Vec3 v) { return std::sqrt(b3Dot(v, v)); }

// Moves the capsule by delta, returning the achieved translation fraction.
float cast_mover(b3WorldId world, b3Pos &origin, const b3Capsule &capsule,
                 b3Vec3 delta) {
  if (b3Dot(delta, delta) < 1e-12f) {
    return 1.0f;
  }
  const float fraction = b3World_CastMover(world, origin, &capsule, delta,
                                           b3DefaultQueryFilter(), nullptr,
                                           nullptr);
  origin = origin + fraction * delta;
  return fraction;
}

// Gathers contact planes at the current origin. False when free.
bool gather_planes(b3WorldId world, b3Pos origin, const b3Capsule &capsule,
                   PlaneContext &context) {
  context = {};
  b3World_CollideMover(world, origin, &capsule, b3DefaultQueryFilter(),
                       collect_planes, &context);
  return context.count > 0;
}

// Reflects the velocity off the contact plane with restitution and bleeds
// tangential speed as friction. Sub-bounce-speed impacts absorb instead.
void bounce_velocity(b3Vec3 &velocity, b3Vec3 normal, float dt,
                     const ItemStepParams &params) {
  const float normal_speed = b3Dot(velocity, normal);
  if (normal_speed >= 0.0f) {
    return;
  }
  const float restitution =
      -normal_speed >= params.bounce_speed ? params.restitution : 0.0f;
  velocity = velocity - (1.0f + restitution) * normal_speed * normal;
  const float keep = std::max(0.0f, 1.0f - params.contact_friction * dt);
  const b3Vec3 tangential = velocity - b3Dot(velocity, normal) * normal;
  velocity = velocity - (1.0f - keep) * tangential;
}

bool supported(b3WorldId world, b3Pos origin, const b3Capsule &capsule) {
  // Reach past the capsule bottom so a resting contact registers support.
  const float half_segment = 0.5f * std::abs(capsule.center2.y - capsule.center1.y);
  const float reach = half_segment + capsule.radius + GroundProbeDistance;
  const b3Vec3 down{0.0f, -reach, 0.0f};
  const b3RayResult hit =
      b3World_CastRayClosest(world, origin, down, b3DefaultQueryFilter());
  return hit.hit && hit.normal.y >= MinSupportNormalY;
}

} // namespace

void step_item_substep(ItemState &state, float dt,
                       const ItemStepParams &params,
                       character_motion::MeshCollisionWorld *world) {
  const b3WorldId physics = world->world;
  const b3Capsule capsule = item_capsule(params.radius);
  b3Pos origin{state.x, state.y, state.z};
  b3Vec3 velocity{state.velocity_x, state.velocity_y, state.velocity_z};

  velocity.y -= params.gravity * dt;
  const float drag = std::max(0.0f, 1.0f - params.air_drag * dt);
  velocity = drag * velocity;
  const float speed = length(velocity);
  if (speed > params.max_speed) {
    velocity = (params.max_speed / speed) * velocity;
  }

  // Ballistic move with contact solving. Plane gathering runs every
  // iteration so b3SolvePlanes also depenetrates a resting contact; without
  // that an item at rest would slowly sink through the floor triangles.
  state.grounded = 0;
  float left = dt;
  for (int iteration = 0; iteration < MoveIterations; ++iteration) {
    PlaneContext context;
    gather_planes(physics, origin, capsule, context);
    if (context.count > 0) {
      const b3Vec3 normal = context.planes[0].plane.normal;
      bounce_velocity(velocity, normal, dt, params);
      if (normal.y >= MinSupportNormalY) {
        state.grounded = 1;
      }
    }

    b3Vec3 step{velocity.x * left, velocity.y * left, velocity.z * left};
    if (context.count > 0) {
      const b3PlaneSolverResult solved =
          b3SolvePlanes(step, context.planes, context.count);
      step = solved.delta;
    }
    if (b3Dot(step, step) < MoveTolerance * MoveTolerance) {
      break;
    }
    const b3Vec3 before{origin.x, origin.y, origin.z};
    const float fraction = cast_mover(physics, origin, capsule, step);
    left *= 1.0f - fraction;
    const b3Vec3 applied = b3SubPos(origin, before);
    if (fraction >= 1.0f ||
        (b3Dot(applied, applied) < MoveTolerance * MoveTolerance &&
         context.count > 0)) {
      break;
    }
  }

  if (supported(physics, origin, capsule)) {
    state.grounded = 1;
    if (velocity.y < 0.0f) {
      velocity.y = 0.0f;
    }
    // Static friction: kill crawl-speed motion outright so micro-bounces on
    // uneven floor geometry cannot jitter the item forever.
    if (length(velocity) < params.rest_speed) {
      velocity = {0.0f, 0.0f, 0.0f};
    }
    if (length(velocity) < params.sleep_speed) {
      state.sleep_timer += dt;
    } else {
      state.sleep_timer = 0.0f;
    }
  } else {
    state.sleep_timer = 0.0f;
  }
  if (state.sleep_timer >= params.sleep_delay) {
    state.sleeping = 1;
    velocity = {0.0f, 0.0f, 0.0f};
  }

  state.x = origin.x;
  state.y = origin.y;
  state.z = origin.z;
  state.velocity_x = velocity.x;
  state.velocity_y = velocity.y;
  state.velocity_z = velocity.z;
}

} // namespace octaryn::item_motion
