#pragma once

#include <cstdint>

#if defined(_WIN32)
#define OCTARYN_SERVER_TERRAIN_GENERATION_API __declspec(dllexport)
#else
#define OCTARYN_SERVER_TERRAIN_GENERATION_API                                  \
  __attribute__((visibility("default")))
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

struct OctarynServerTerrainMaterialRules {
  int32_t water_height;
  uint16_t water_block;
  uint16_t sand_block;
  uint16_t grass_block;
  uint16_t dirt_block;
  uint16_t stone_block;
  uint16_t snow_block;
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

OCTARYN_SERVER_TERRAIN_GENERATION_API int32_t
octaryn_server_terrain_plan_column(
    int32_t x, int32_t z, const OctarynServerTerrainMaterialRules *rules,
    OctarynServerTerrainColumnPlan *plan);

OCTARYN_SERVER_TERRAIN_GENERATION_API int32_t
octaryn_server_terrain_generated_block(
    int32_t x, int32_t y, int32_t z,
    const OctarynServerTerrainMaterialRules *rules, uint16_t *block);

OCTARYN_SERVER_TERRAIN_GENERATION_API uint16_t
octaryn_server_empty_world_generated_block(int32_t x, int32_t y, int32_t z);

OCTARYN_SERVER_TERRAIN_GENERATION_API int32_t
octaryn_server_terrain_clear_matching_overrides(
    void *block_store, const OctarynServerTerrainMaterialRules *rules);

OCTARYN_SERVER_TERRAIN_GENERATION_API int32_t
octaryn_server_empty_world_clear_matching_overrides(void *block_store);
}
