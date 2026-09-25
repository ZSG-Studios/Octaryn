#include "PhysicsWorld.h"

#include <box3d/box3d.h>

#include <vector>

namespace octaryn::client::physics {
namespace {

constexpr double FixedStepSeconds = 8.0 / 1000.0;

struct WorldHandle {
  b3WorldId id{};
  std::vector<b3BodyId> bodies;
  ~WorldHandle() { if (b3World_IsValid(id)) b3DestroyWorld(id); }
};

b3WorldId world(void* handle) { return static_cast<WorldHandle*>(handle)->id; }

} // namespace

PhysicsWorld::PhysicsWorld() {
  b3WorldDef def = b3DefaultWorldDef();
  def.gravity = {0.0f, -9.81f, 0.0f};
  auto* handle = new WorldHandle();
  handle->id = b3CreateWorld(&def);
  world_ = handle;
}

PhysicsWorld::~PhysicsWorld() { delete static_cast<WorldHandle*>(world_); }

void PhysicsWorld::set_gravity(float x, float y, float z) {
  b3World_SetGravity(world(world_), {x, y, z});
}

std::uint32_t PhysicsWorld::create_body(const PhysicsBodyDesc& desc) {
  b3BodyDef def = b3DefaultBodyDef();
  def.type = desc.dynamic ? b3_dynamicBody : b3_staticBody;
  def.position = {desc.position[0], desc.position[1], desc.position[2]};
  def.rotation.v = {desc.rotation[0], desc.rotation[1], desc.rotation[2]};
  def.rotation.s = desc.rotation[3];
  const auto body = b3CreateBody(world(world_), &def);
  if (!b3Body_IsValid(body)) return 0;
  b3ShapeDef shape = b3DefaultShapeDef();
  const b3BoxHull box = b3MakeBoxHull(desc.half_extents[0], desc.half_extents[1], desc.half_extents[2]);
  b3CreateHullShape(body, &shape, &box.base);
  auto* handle = static_cast<WorldHandle*>(world_);
  handle->bodies.push_back(body);
  return static_cast<std::uint32_t>(handle->bodies.size());
}

bool PhysicsWorld::body_position(std::uint32_t body, float (&position)[3]) const {
  auto* handle = static_cast<const WorldHandle*>(world_);
  if (!body || body > handle->bodies.size()) return false;
  const auto id = handle->bodies[body - 1u];
  if (!b3Body_IsValid(id)) return false;
  const auto transform = b3Body_GetTransform(id);
  position[0] = transform.p.x;
  position[1] = transform.p.y;
  position[2] = transform.p.z;
  return true;
}

void PhysicsWorld::step(double elapsed_seconds) {
  accumulator_ += elapsed_seconds;
  while (accumulator_ >= FixedStepSeconds) {
    b3World_Step(world(world_), static_cast<float>(FixedStepSeconds), 1);
    accumulator_ -= FixedStepSeconds;
    ++steps_;
  }
}

}
