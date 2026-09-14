#pragma once
#include "LocalLight.h"
#include "ColumnBlocks.h"
#include <map>
#include <vector>
namespace octaryn::client::world_presentation { struct StreamColumn; }
namespace octaryn::client::rendering {
struct BlockLightColumn {
  world_presentation::ColumnBlocks blocks;
  int min_y{},height{};
  std::vector<WorldLocalLight> lights;
};
struct BlockLights {
  std::map<std::pair<int,int>,BlockLightColumn> columns;
  std::vector<WorldLocalLight> explicit_lights;
  std::uint64_t source_count{};
  unsigned selected_count{};
  bool dirty{true};
  std::array<int,3> selected_cell{0x7fffffff,0,0};
};
void world_block_lights_store(WorldRenderer&,const world_presentation::StreamColumn&);
void world_block_lights_remove(WorldRenderer&,std::pair<int,int>);
void world_block_lights_update(WorldRenderer&);
}
