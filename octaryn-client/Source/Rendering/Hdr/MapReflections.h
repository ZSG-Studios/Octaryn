#pragma once
#include "TemporalCamera.h"
#include "MapReflectionQueue.h"
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <cstdint>

namespace octaryn::client::rendering {
struct ReflectionImage {
  Slang::ComPtr<rhi::ITexture> texture;
  Slang::ComPtr<rhi::ITextureView> view;
};
struct ReflectionHistory {
  ReflectionImage radiance,position,surface,material,moments;
};
struct MapReflections {
  MapReflectionQueue queue;
  Slang::ComPtr<rhi::IComputePipeline> resolve,filter,classify;
  ReflectionImage filtered,tiles;
  std::array<ReflectionHistory,2> history;
  TemporalCamera camera;
  WorldCamera pending_camera;
  unsigned width{},height{},source_width{},source_height{},index{},pending_index{},samples{},quality{2};
  unsigned allocation_width{},allocation_height{};
  bool valid{},pending{},enabled{true},reference{},sparse{},history_search{},full_fresh{},map_only{},deferred_material{};
  float range{};
  std::uint64_t scene_revision{},light_revision{};
  std::uint32_t gi_epoch{};bool gi_active{};
  std::array<float,4> last_sun{},last_lighting{};
};
struct WorldRenderer;
bool prepare_map_reflections(WorldRenderer&);
bool render_map_reflections(WorldRenderer&,rhi::ICommandEncoder*);
void commit_map_reflections(WorldRenderer&);
}
