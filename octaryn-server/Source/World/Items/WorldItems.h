#pragma once
#include "WorldItemWire.h"
#if defined(_WIN32)
#if defined(OCTARYN_WORLD_ITEMS_EXPORTS)
#define OCTARYN_ITEMS_API __declspec(dllexport)
#else
#define OCTARYN_ITEMS_API __declspec(dllimport)
#endif
#else
#define OCTARYN_ITEMS_API __attribute__((visibility("default")))
#endif
extern "C" {
using octaryn_item_solid_fn=std::uint32_t(*)(void*,std::int32_t,std::int32_t,std::int32_t);
OCTARYN_ITEMS_API void octaryn_items_initialize(octaryn::world_items::State*);
OCTARYN_ITEMS_API int octaryn_items_validate(const octaryn::world_items::State*);
OCTARYN_ITEMS_API int octaryn_items_drop(octaryn::world_items::State*,std::uint64_t command,
    std::uint32_t block,std::uint32_t count,std::uint32_t placeable,
    float x,float y,float z,float yaw,float pitch);
OCTARYN_ITEMS_API int octaryn_items_acknowledge(octaryn::world_items::State*,std::uint64_t grant);
OCTARYN_ITEMS_API int octaryn_items_tick(octaryn::world_items::State*,double delta,
    float player_x,float player_y,float player_z,octaryn_item_solid_fn,void*);
}
