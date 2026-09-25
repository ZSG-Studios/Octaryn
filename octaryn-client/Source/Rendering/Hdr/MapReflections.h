#pragma once
#include "TemporalCamera.h"
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
  ReflectionImage radiance,position,surface,material;
};
struct MapReflections {
  Slang::ComPtr<rhi::IComputePipeline> resolve;
  std::array<ReflectionHistory,2> history;
  TemporalCamera camera;
  WorldCamera pending_camera;
  unsigned width{},height{},index{},pending_index{},samples{},quality{2};
  bool valid{},pending{},enabled{true};
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
