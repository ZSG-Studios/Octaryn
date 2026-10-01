#pragma once
#include <slang-rhi.h>
#include <filesystem>
#include <memory>
#include <string>
#include <span>
#include "GeometryStream.h"
#include "SelectionGpu.h"
namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldCamera;
struct MapRenderer;
namespace virtual_geometry {
struct SceneGeometryContext;
// Scene assets share pages/selection; standalone maps own their bounded resources.
class WorldGeometry {
public:
  WorldGeometry();
  ~WorldGeometry();
  bool initialize(WorldRenderer&,MapRenderer&,std::shared_ptr<void> scheduler={},const SceneGeometryContext* context=nullptr);
  GeometryStream& stream();
  const GeometryAsset& asset() const;
  void request_ray_pages(std::span<const PageRequest>);
  bool stage_uploads(rhi::ICommandEncoder*);
  bool prepare(WorldRenderer&,rhi::ICommandEncoder*,const WorldCamera&,std::size_t instance=0,bool selection_only=false);
  bool complete_root_cut() const;
  const SelectionGpuFrame& selection_frame() const;
  bool resolve(WorldRenderer&,rhi::IRenderPassEncoder*);
  bool submitted(rhi::IFence*,std::uint64_t value);
  bool ready() const;
  float requested_error_pixels() const;
  std::uint64_t gpu_bytes() const;
  const std::string& error() const;
  bool admission_rejected() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
}
