#include "AtlasInternal.h"
#include <glaze/glaze.hpp>
#include <filesystem>
#include <fstream>
#include <cmath>
namespace octaryn::client::rendering {
namespace atlas_data {
struct Faces { unsigned north{},south{},east{},west{},up{},down{}; };
struct Block { std::string id; bool opaque{},sprite{},solid{},occlusion{},requiresSolidBase{}; std::string fluidKind; int fluidLevel{}; Faces atlas; std::array<float,4> emission{}; };
struct Catalog { std::string schema; std::vector<Block> blocks; };
struct Material { std::uint32_t layers[6],flags,fluid_level; std::array<float,4> emission{}; };
static_assert(sizeof(Material)==48);
}
bool load_atlas_materials(WorldAtlas& atlas) {
  using namespace atlas_data;
  const auto path=atlas_asset_path("Data/Blocks/octaryn.basegame.blocks.json",false);
  std::ifstream file(std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str())));
  if (!file) return false;
  const std::string text((std::istreambuf_iterator<char>(file)),{});
  Catalog catalog;
  constexpr glz::opts options{.error_on_unknown_keys=false};
  if (glz::read<options>(catalog,text) || catalog.schema!="octaryn.basegame.blocks.v1" ||
      catalog.blocks.empty() || catalog.blocks.size()>65536) return false;
  std::vector<Material> materials;
  for (const auto& block:catalog.blocks) {
    const auto& f=block.atlas;
    Material material{{f.west,f.east,f.down,f.up,f.south,f.north},0,static_cast<unsigned>(block.fluidLevel)};
    for (auto layer:material.layers) if (layer>=29) return false;
    for(float value:block.emission)if(!std::isfinite(value) || value<0)return false;
    material.emission=block.emission;
    // Preserve catalog render-pass/occlusion distinctions for the restored passes.
    material.flags=(block.occlusion?1u:0u)|(block.sprite?2u:0u)|
        (block.fluidKind!="none"?4u:0u)|(!block.opaque && block.fluidKind=="none"?8u:0u);
    material.flags |= (block.opaque?16u:0u)|(block.requiresSolidBase?32u:0u)|
        (block.id=="octaryn.basegame.block.glass"?64u:0u)|
        (block.id=="octaryn.basegame.block.cloud"?128u:0u)|
        (block.id=="octaryn.basegame.block.leaves"?256u:0u)|(block.fluidKind=="lava"?512u:0u)|(block.solid?1024u:0u);
    materials.push_back(material);
    atlas.preview_layers.push_back(f.north);
    atlas.emissions.push_back({block.emission,block.occlusion || block.fluidKind=="lava",block.sprite});
  }
  rhi::BufferDesc desc{};
  desc.size=materials.size()*sizeof(Material);
  desc.elementSize=sizeof(Material); desc.memoryType=rhi::MemoryType::DeviceLocal;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  desc.usage=rhi::BufferUsage::ShaderResource;
  return SLANG_SUCCEEDED(atlas.device->createBuffer(desc,materials.data(),atlas.materials.writeRef()));
}
}
