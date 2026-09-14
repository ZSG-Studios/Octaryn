#include "AtlasInternal.h"
#include <cstdio>
#include <slang-rhi/shader-cursor.h>
namespace octaryn::client::rendering {
namespace {
bool create_texture(WorldAtlas& atlas,unsigned index,const char* file) {
  std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> image(load_atlas_rgba(file),SDL_DestroySurface);
  if (!image || image->w!=32*29 || image->h!=32) { std::fprintf(stderr,"Invalid atlas dimensions: %s\n",file); return false; }
  std::vector<unsigned char> bytes;
  for (unsigned layer=0;layer<29;++layer) {
    const auto pixels=atlas_layer_pixels(image.get(),layer,static_cast<atlas_mip_kind_t>(index));
    bytes.insert(bytes.end(),pixels.begin(),pixels.end());
  }
  auto data=atlas_subresources(bytes,29);
  rhi::TextureDesc desc{};
  desc.type=rhi::TextureType::Texture2DArray;
  desc.format=index==0?rhi::Format::RGBA8UnormSrgb:rhi::Format::RGBA8Unorm;
  desc.size={32,32,1}; desc.arrayLength=29; desc.mipCount=6;
  desc.sampleCount=1;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  desc.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopyDestination;
  desc.memoryType=rhi::MemoryType::DeviceLocal;
  if (SLANG_FAILED(atlas.device->createTexture(desc,data.data(),atlas.textures[index].writeRef()))) return false;
  rhi::TextureViewDesc view{};
  view.format=desc.format;
  view.aspect=rhi::TextureAspect::All;
  view.subresourceRange.mipCount=6; view.subresourceRange.layerCount=29;
  return SLANG_SUCCEEDED(atlas.device->createTextureView(atlas.textures[index],view,atlas.views[index].writeRef()));
}
bool samplers(WorldAtlas& atlas) {
  rhi::SamplerDesc desc{};
  desc.minLOD=0; desc.maxLOD=5;
  desc.addressU=desc.addressV=rhi::TextureAddressingMode::Wrap;
  desc.addressW=rhi::TextureAddressingMode::ClampToEdge;
  desc.minFilter=desc.magFilter=desc.mipFilter=rhi::TextureFilteringMode::Point;
  if (SLANG_FAILED(atlas.device->createSampler(desc,atlas.cutout.writeRef()))) return false;
  desc.maxLOD=0;
  if (SLANG_FAILED(atlas.device->createSampler(desc,atlas.nearest.writeRef()))) return false;
  desc.maxLOD=5; desc.maxAnisotropy=8;
  desc.minFilter=desc.magFilter=desc.mipFilter=rhi::TextureFilteringMode::Linear;
  if(SLANG_FAILED(atlas.device->createSampler(desc,atlas.linear.writeRef()))) return false;
  desc.addressU=desc.addressV=rhi::TextureAddressingMode::ClampToEdge;
  return SLANG_SUCCEEDED(atlas.device->createSampler(desc,atlas.sprite.writeRef()));
}
}
WorldAtlas* create_world_atlas(rhi::IDevice* device) {
  if (!device) return nullptr;
  auto atlas=std::make_unique<WorldAtlas>(); atlas->device=device;
  if (!create_texture(*atlas,0,"Atlases/basegame-color.png") ||
      !create_texture(*atlas,1,"Atlases/basegame-normal.png") ||
      !create_texture(*atlas,2,"Atlases/basegame-specular.png") ||
      !samplers(*atlas) || !load_atlas_materials(*atlas) || !load_atlas_animations(*atlas)) return nullptr;
  std::printf("world_atlas layers=29 mips=6 textures=3 animations=%zu source=basegame_catalog\n",atlas->animations.size());
  return atlas.release();
}
unsigned world_atlas_preview_layer(WorldAtlas* atlas,unsigned block) {
  return atlas && block<atlas->preview_layers.size()?atlas->preview_layers[block]:0;
}
void destroy_world_atlas(WorldAtlas* atlas) { delete atlas; }
rhi::IBuffer* world_atlas_materials(WorldAtlas* atlas) { return atlas?atlas->materials.get():nullptr; }
rhi::ITextureView* world_atlas_albedo(WorldAtlas* atlas) { return atlas?atlas->views[0].get():nullptr; }
rhi::ISampler* world_atlas_nearest(WorldAtlas* atlas) { return atlas?atlas->nearest.get():nullptr; }
rhi::ISampler* world_atlas_sprite(WorldAtlas* atlas) { return atlas?atlas->sprite.get():nullptr; }
bool bind_world_atlas(WorldAtlas* atlas,rhi::IShaderObject* root,uint32_t first) {
  if (!atlas || !root) return false;
  bool result=SLANG_SUCCEEDED(root->setBinding({0,first,0},rhi::Binding(atlas->materials)));
  for (uint32_t i=0;i<3;++i) result &= SLANG_SUCCEEDED(root->setBinding({0,first+1+i,0},rhi::Binding(atlas->views[static_cast<std::size_t>(i)])));
  result &= SLANG_SUCCEEDED(root->setBinding({0,first+4,0},rhi::Binding(atlas->cutout)));
  result &= SLANG_SUCCEEDED(root->setBinding({0,first+5,0},rhi::Binding(atlas->linear)));
  const auto sprite=rhi::ShaderCursor(root)["atlasSprite"];
  if(sprite.isValid())result &= SLANG_SUCCEEDED(sprite.setBinding(rhi::Binding(atlas->sprite)));
  return result;
}
}
