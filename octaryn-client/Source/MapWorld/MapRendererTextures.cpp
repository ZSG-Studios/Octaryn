#include "MapRendererInternal.h"
#include <cstdio>

namespace octaryn::client::rendering {
bool upload_map_images(MapRenderer& map) {
  map.textures.resize(map.model.images.size());
  map.texture_views.resize(map.model.images.size());
  for(size_t index=0;index<map.model.images.size();++index) {
    MapDecodedImage decoded;
    std::string error;
    if(!decode_map_image(map.model.images[index],decoded,error)) {
      std::fprintf(stderr,"map_image_decode_failed index=%zu: %s\n",index,error.c_str());
      return false;
    }
    // sRGB base color: the GPU converts the raw 8-bit texels on sample.
    rhi::TextureDesc desc{};
    desc.size={decoded.width,decoded.height,1};desc.format=rhi::Format::RGBA8UnormSrgb;
    desc.mipCount=1;desc.sampleCount=1;
    desc.defaultState=rhi::ResourceState::ShaderResource;
    desc.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopyDestination;
    desc.memoryType=rhi::MemoryType::DeviceLocal;
    const rhi::SubresourceData data{decoded.rgba.data(),
        static_cast<rhi::Size>(decoded.width*4),static_cast<rhi::Size>(decoded.rgba.size())};
    if(SLANG_FAILED(map.device->createTexture(desc,&data,map.textures[index].writeRef())) ||
        SLANG_FAILED(map.textures[index]->getDefaultView(map.texture_views[index].writeRef())))return false;
  }
  return true;
}
}
