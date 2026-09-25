#pragma once
#include "WorldStream.h"
#include <vector>

namespace octaryn::client::rendering {
struct WorldRenderer;
void world_mesh_invalidate_preloaded_outside_window(WorldRenderer&);
void world_mesh_halo_into(const WorldRenderer&,const world_presentation::StreamColumn&,
    std::vector<std::uint32_t>&,world_presentation::ColumnOrigins*);
inline bool world_mesh_preload_matches(const std::shared_ptr<const world_presentation::ColumnOrigin>& expected,
    const world_presentation::StreamColumn& next) {
  using namespace world_presentation;
  return expected && next.origin && *expected==*next.origin &&
      next.min_y==StreamWorldMinY && next.height==StreamWorldHeight &&
      next.generated_blocks.size()==std::size_t(StreamWorldHeight)*32u*32u &&
      next.blocks.storage_identity()==next.generated_blocks.storage_identity();
}
}
