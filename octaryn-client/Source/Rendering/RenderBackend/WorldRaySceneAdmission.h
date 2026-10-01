#pragma once
#include "../../VirtualGeometry/SceneMemoryLedger.h"
namespace octaryn::client::rendering {
struct WorldRenderer;
std::shared_ptr<virtual_geometry::SceneMemoryLease> world_ray_scene_credit(WorldRenderer&,std::uint64_t);
void world_ray_scene_admission_complete(WorldRenderer&);
}
