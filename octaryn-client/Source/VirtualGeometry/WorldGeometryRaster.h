#pragma once
#include "HybridRenderer.h"
#include "SceneRasterTables.h"
#include <array>

namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldCamera;
namespace virtual_geometry {
struct WorldGeometryRaster {
  std::array<HybridRenderer,2> frames;
  std::array<HybridRenderer,2> scene_frames;
  std::array<HybridInputs,2> scene_inputs;
  std::shared_ptr<SceneRasterTables> scene_tables;
  bool initialized{},scene_initialized{},scene_recorded{};
  bool initialize(WorldRenderer&,bool scene=false);
  bool scene_visibility(WorldRenderer&,rhi::ICommandEncoder*,const WorldCamera&);
  bool scene_resolve(WorldRenderer&,rhi::IRenderPassEncoder*);
  bool submitted(rhi::IFence*,std::uint64_t);
  std::uint64_t gpu_bytes() const;
};
}
}
