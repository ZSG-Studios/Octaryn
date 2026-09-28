#pragma once
#include <slang-rhi.h>
#include <atomic>
#include <vector>
namespace octaryn::client::rendering {
struct MapRenderer;
bool map_ray_inputs(MapRenderer&,std::vector<rhi::AccelerationStructureBuildInput>&);
// May run on the exclusive allocation worker before this map is published.
bool allocate_map_ray_resources(MapRenderer&,const std::atomic_bool* cancel);
}
