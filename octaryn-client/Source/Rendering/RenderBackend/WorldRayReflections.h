#pragma once
namespace rhi { class ICommandEncoder; }
namespace octaryn::client::rendering {
struct WorldRenderer;
bool world_ray_reflections_initialize(WorldRenderer&);
}
