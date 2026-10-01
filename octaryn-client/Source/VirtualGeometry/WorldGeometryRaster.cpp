#include "WorldGeometryRaster.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include "../Rendering/RenderBackend/SlangShaderPath.h"
#include <filesystem>

namespace octaryn::client::rendering::virtual_geometry {
bool WorldGeometryRaster::initialize(WorldRenderer& renderer,bool scene) {
  if(scene?scene_initialized:initialized)return true;
  const auto shader=resolve_slang_shader_path("octaryn-client/Shaders/VirtualGeometry/Selection.slang");
  if(shader.empty())return false;
  const auto directory=std::filesystem::path(shader).parent_path().generic_string();
  const auto formats=std::span(world_gbuffer_formats).first(world_gbuffer_attachment_count(renderer.device));
  for(auto& frame:scene?scene_frames:frames)if(!frame.initialize(renderer.device,directory.c_str(),formats,rhi::Format::D32Float,scene))return false;
  (scene?scene_initialized:initialized)=true;return true;
}
std::uint64_t WorldGeometryRaster::gpu_bytes() const {
  std::uint64_t bytes{};
  for(const auto& frame:frames)if(auto* buffer=frame.visibility_buffer())bytes+=buffer->getDesc().size;
  for(const auto& frame:scene_frames)if(auto* buffer=frame.visibility_buffer())bytes+=buffer->getDesc().size;
  return bytes;
}
}
