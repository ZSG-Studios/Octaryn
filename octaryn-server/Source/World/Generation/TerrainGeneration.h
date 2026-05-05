#pragma once

#include <cstdint>

#if defined(_WIN32)
#define OCTARYN_SERVER_TERRAIN_GENERATION_API __declspec(dllexport)
#else
#define OCTARYN_SERVER_TERRAIN_GENERATION_API __attribute__((visibility("default")))
#endif

extern "C" {

struct OctarynServerTerrainColumnSample {
  int32_t world_x;
  int32_t world_z;
  int32_t local_x;
  int32_t local_z;
  int32_t local_width;
  int32_t local_depth;
  int32_t max_terrain_y;
  float height_noise;
  float lowland_noise;
  float biome_noise;
};

struct OctarynServerTerrainColumnPlan {
  int32_t world_x;
  int32_t world_z;
  int32_t local_x;
  int32_t local_z;
  int32_t local_width;
  int32_t local_depth;
  int32_t terrain_height;
  int32_t decoration_y;
  uint16_t surface_block;
  uint16_t fill_block;
  uint32_t is_lowland;
  uint32_t has_grass_surface;
};

using octaryn_server_terrain_plan_column_fn =
    int32_t (*)(void *context,
                const OctarynServerTerrainColumnSample *sample,
                OctarynServerTerrainColumnPlan *plan);

OCTARYN_SERVER_TERRAIN_GENERATION_API int32_t
octaryn_server_terrain_generated_block(
    int32_t x, int32_t y, int32_t z, int32_t water_height,
    uint16_t water_block, octaryn_server_terrain_plan_column_fn plan_column,
    void *context, uint16_t *block);

OCTARYN_SERVER_TERRAIN_GENERATION_API uint16_t
octaryn_server_empty_world_generated_block(int32_t x, int32_t y, int32_t z);

}
