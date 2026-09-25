#include "WorldAtlas.h"
#include <slang-rhi/shader-cursor.h>
#include <cstdio>
#include <memory>

namespace octaryn::client::rendering {

struct WorldAtlas {
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::ITexture> textures[3];
  Slang::ComPtr<rhi::ITextureView> views[3];
  Slang::ComPtr<rhi::ISampler> cutout, nearest, linear, sprite;
  Slang::ComPtr<rhi::IBuffer> materials, plant_mask_cells, plant_mask_layers, cube_mask_layers;
};

namespace {

constexpr unsigned LayerCount = 29;
constexpr unsigned LayerExtent = 32;
constexpr unsigned MipCount = 6;

bool create_textures(WorldAtlas& atlas) {
  rhi::TextureDesc desc{};
  desc.type = rhi::TextureType::Texture2DArray;
  desc.format = rhi::Format::RGBA8UnormSrgb;
  desc.size = {LayerExtent, LayerExtent, 1};
  desc.arrayLength = LayerCount;
  desc.mipCount = MipCount;
  desc.sampleCount = 1;
  desc.defaultState = rhi::ResourceState::ShaderResource;
  desc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::CopyDestination;
  desc.memoryType = rhi::MemoryType::DeviceLocal;
  desc.label = "material_atlas";
  for (unsigned index = 0; index < 3; ++index) {
    if (SLANG_FAILED(atlas.device->createTexture(desc, nullptr, atlas.textures[index].writeRef())))
      return false;
    rhi::TextureViewDesc view{};
    view.format = desc.format;
    view.aspect = rhi::TextureAspect::All;
    view.subresourceRange.mipCount = MipCount;
    view.subresourceRange.layerCount = LayerCount;
    if (SLANG_FAILED(atlas.device->createTextureView(atlas.textures[index], view, atlas.views[index].writeRef())))
      return false;
    desc.format = rhi::Format::RGBA8Unorm;
  }
  return true;
}

bool create_samplers(WorldAtlas& atlas) {
  rhi::SamplerDesc desc{};
  desc.minLOD = 0;
  desc.maxLOD = 5;
  desc.addressU = desc.addressV = rhi::TextureAddressingMode::Wrap;
  desc.addressW = rhi::TextureAddressingMode::ClampToEdge;
  desc.minFilter = desc.magFilter = desc.mipFilter = rhi::TextureFilteringMode::Point;
  if (SLANG_FAILED(atlas.device->createSampler(desc, atlas.cutout.writeRef()))) return false;
  desc.maxLOD = 0;
  if (SLANG_FAILED(atlas.device->createSampler(desc, atlas.nearest.writeRef()))) return false;
  desc.maxLOD = 5;
  desc.maxAnisotropy = 8;
  desc.minFilter = desc.magFilter = desc.mipFilter = rhi::TextureFilteringMode::Linear;
  if (SLANG_FAILED(atlas.device->createSampler(desc, atlas.linear.writeRef()))) return false;
  desc.addressU = desc.addressV = rhi::TextureAddressingMode::ClampToEdge;
  return SLANG_SUCCEEDED(atlas.device->createSampler(desc, atlas.sprite.writeRef()));
}

bool create_zero_buffer(WorldAtlas& atlas, std::uint64_t size, Slang::ComPtr<rhi::IBuffer>& buffer, const char* label) {
  rhi::BufferDesc desc{};
  desc.size = size;
  desc.defaultState = rhi::ResourceState::ShaderResource;
  desc.usage = rhi::BufferUsage::ShaderResource;
  desc.label = label;
  return SLANG_SUCCEEDED(atlas.device->createBuffer(desc, nullptr, buffer.writeRef()));
}

} // namespace

WorldAtlas* create_world_atlas(rhi::IDevice* device) {
  if (!device) return nullptr;
  auto atlas = std::make_unique<WorldAtlas>();
  atlas->device = device;
  const auto ok = create_textures(*atlas) && create_samplers(*atlas) &&
      create_zero_buffer(*atlas, LayerCount * 64ull, atlas->materials, "atlas_materials") &&
      create_zero_buffer(*atlas, WorldAtlasPlantMaskBytes, atlas->plant_mask_cells, "atlas_plant_cells") &&
      create_zero_buffer(*atlas, LayerCount * 32ull, atlas->plant_mask_layers, "atlas_plant_layers") &&
      create_zero_buffer(*atlas, LayerCount * 32ull, atlas->cube_mask_layers, "atlas_cube_layers");
  if (!ok) {
    std::fputs("world_atlas_initialize_failed stage=empty_resources\n", stderr);
    return nullptr;
  }
  std::printf("world_atlas layers=%u mips=%u source=empty\n", LayerCount, MipCount);
  return atlas.release();
}

void destroy_world_atlas(WorldAtlas* atlas) { delete atlas; }

bool update_world_atlas(WorldAtlas*, rhi::ICommandEncoder*, double) { return true; }
void commit_world_atlas(WorldAtlas*) {}
bool prepare_world_atlas_plant_masks(WorldAtlas*) { return true; }
std::uint64_t world_atlas_plant_mask_bytes(WorldAtlas* atlas) {
  return atlas ? WorldAtlasPlantMaskBytes : 0;
}

rhi::IBuffer* world_atlas_materials(WorldAtlas* atlas) { return atlas ? atlas->materials.get() : nullptr; }

bool bind_world_atlas(WorldAtlas* atlas, rhi::IShaderObject* root) {
  if (!atlas || !root) return false;
  rhi::ShaderCursor cursor(root);
  auto bind = [&](const char* name, rhi::Binding value) {
    auto field = cursor[name];
    return !field.isValid() || SLANG_SUCCEEDED(field.setBinding(value));
  };
  auto buffer_bind = [&](const char* name, rhi::IBuffer* value) {
    auto field = cursor[name];
    return !field.isValid() || (value && SLANG_SUCCEEDED(field.setBinding(rhi::Binding(value))));
  };
  const std::uint32_t count = LayerCount;
  auto trace_count = cursor["voxelTraceMaterialCount"];
  if (trace_count.isValid() && SLANG_FAILED(trace_count.setData(&count, sizeof(count)))) return false;
  return buffer_bind("btPlantMaskCells", atlas->plant_mask_cells) &&
      buffer_bind("btPlantMaskLayers", atlas->plant_mask_layers) &&
      buffer_bind("btCubeMaskLayers", atlas->cube_mask_layers) &&
      bind("blockMaterials", rhi::Binding(atlas->materials)) &&
      bind("atlasAlbedo", rhi::Binding(atlas->views[0])) && bind("atlasNormal", rhi::Binding(atlas->views[1])) &&
      bind("atlasSpecular", rhi::Binding(atlas->views[2])) &&
      bind("atlasCutout", rhi::Binding(atlas->cutout)) &&
      bind("atlasLinear", rhi::Binding(atlas->linear)) && bind("atlasSprite", rhi::Binding(atlas->sprite));
}

}
