#include "MeshCollisionSoup.h"
#include "MeshCollisionWorld.h"
#include <cmath>
#include <cstdio>
#include <future>
#include <stdexcept>

using octaryn::client::app::local_session::MeshCollisionSoup;
using namespace octaryn::character_motion;

static void check(bool result, const char* message) {
  if (!result) throw std::runtime_error(message);
}

static MeshCollisionSoup make_floor(float y) {
  return {{-10,y,-10, -10,y,10, 10,y,10, 10,y,-10}, {0,1,2, 0,2,3}};
}

int main() {
  // The prediction owner must retain A while startup replaces its own handle
  // with B. Both collision worlds must be released by the last owning handle.
  for (int cycle = 0; cycle < 20; ++cycle) {
    auto startup = make_floor(0);
    auto prediction = startup;
    const auto a = acquire_mesh_world(startup.view())->world;
    startup = make_floor(10);
    const auto b = acquire_mesh_world(startup.view())->world;
    check(b3World_IsValid(a), "replacement invalidated prediction collision");
    const auto hit = b3World_CastRayClosest(b, {0,20,0}, {0,-30,0}, b3DefaultQueryFilter());
    check(hit.hit && std::abs(hit.fraction - 1.0f/3.0f) < 0.001f,
          "replacement reused old geometry or appended old positions");
    prediction = startup;
    check(!b3World_IsValid(a), "old collision world retained after last owner");
    startup = make_floor(0);
    const auto nextA = acquire_mesh_world(startup.view())->world;
    prediction = {};
    check(!b3World_IsValid(b), "replacement collision world leaked");
    const auto back = b3World_CastRayClosest(nextA, {0,20,0}, {0,-30,0}, b3DefaultQueryFilter());
    check(back.hit && std::abs(back.fraction - 2.0f/3.0f) < 0.001f,
          "return to A has incorrect collision geometry");
    startup = {};
    check(!b3World_IsValid(nextA), "final collision world leaked");
  }
  MeshCollisionScene scene;
  auto ground = make_floor(0);
  check(scene.set_tile(0, ground.view()), "initial tile failed");
  const auto world = acquire_mesh_world(scene.view())->world;
  for (int cycle = 0; cycle < 20; ++cycle) {
    auto upper = make_floor(10);
    auto job = std::async(std::launch::async, [&] { return MeshCollisionScene::prepare_tile(upper.view()); });
    const auto before = b3World_CastRayClosest(world, {0,20,0}, {0,-30,0}, b3DefaultQueryFilter());
    check(before.hit && std::abs(before.fraction - 2.0f/3.0f) < 0.001f, "preparation changed resident world");
    check(scene.set_tile(1, job.get()), "prepared tile publication failed");
    const auto upperHit = b3World_CastRayClosest(world, {0,20,0}, {0,-30,0}, b3DefaultQueryFilter());
    check(upperHit.hit && std::abs(upperHit.fraction - 1.0f/3.0f) < 0.001f, "added tile collision missing");
    scene.remove_tile(1);
    check(scene.contains(0) && !scene.contains(1) && b3World_IsValid(world), "tile eviction invalidated neighbors");
  }
  std::puts("collision_lifetime_probe=passed cycles=20 transitions=60 prepared_tile_cycles=20");
}
