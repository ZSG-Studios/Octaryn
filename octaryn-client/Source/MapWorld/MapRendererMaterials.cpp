#include "MapRendererInternal.h"
#include <algorithm>
#include <array>
#include <map>
namespace octaryn::client::rendering {
namespace {
rhi::TextureAddressingMode wrap(unsigned value) {
  return value==33071?rhi::TextureAddressingMode::ClampToEdge:
      value==33648?rhi::TextureAddressingMode::MirrorRepeat:rhi::TextureAddressingMode::Wrap;
}
}
bool upload_map_materials(MapRenderer& map) {
  std::map<std::array<unsigned,4>,rhi::ISampler*> samplers;
  std::vector<MapRayMaterial> records;
  for(size_t primitive_index=0;primitive_index<map.model.primitives.size();++primitive_index) {
    const auto& primitive=map.model.primitives[primitive_index];
    const auto& source=primitive.material;MapRayMaterial record;
    std::copy_n(source.base_color,4,record.base_color);std::copy_n(source.emissive,3,record.emissive);
    record.normal_scale=source.normal_scale;record.metallic=source.metallic;record.roughness=source.roughness;
    record.alpha_cutoff=source.alpha_cutoff;record.occlusion_strength=source.occlusion_strength;
    record.alpha_mode=static_cast<unsigned>(source.alpha_mode);record.double_sided=source.double_sided?1u:0u;
    record.padding[0]=primitive.first_index/3;
    for(unsigned i=0;i<5;++i) {
      const auto& t=source.textures[i];auto& target=record.textures[i];if(t.image<0)continue;
      const std::array<unsigned,4> key{t.wrap_s,t.wrap_t,t.min_filter,t.mag_filter};
      if(!samplers.contains(key)) {
        rhi::SamplerDesc desc{};desc.addressU=wrap(t.wrap_s);desc.addressV=wrap(t.wrap_t);
        desc.magFilter=t.mag_filter==9728?rhi::TextureFilteringMode::Point:rhi::TextureFilteringMode::Linear;
        desc.minFilter=(t.min_filter==9728 || t.min_filter==9984 || t.min_filter==9986)?rhi::TextureFilteringMode::Point:rhi::TextureFilteringMode::Linear;
        desc.mipFilter=(t.min_filter==9984 || t.min_filter==9985)?rhi::TextureFilteringMode::Point:rhi::TextureFilteringMode::Linear;
        desc.maxLOD=t.min_filter<9984?0:1000;
        if(t.min_filter==9987 && t.mag_filter==9729)desc.maxAnisotropy=8;
        Slang::ComPtr<rhi::ISampler> sampler;
        if(SLANG_FAILED(map.device->createSampler(desc,sampler.writeRef())))return false;
        samplers.emplace(key,sampler.get());map.material_samplers.push_back(sampler);
      }
      rhi::DescriptorHandle image{},sampler{};
      const auto slot=map.material_texture_slots[primitive_index][i];
      if(SLANG_FAILED(map.texture_views[slot]->getDescriptorHandle(rhi::DescriptorHandleAccess::Read,&image)) ||
          SLANG_FAILED(samplers.at(key)->getDescriptorHandle(&sampler)))return false;
      target.image=image.value;target.sampler=sampler.value;target.texcoord=t.texcoord;target.present=1;
      std::copy_n(t.transform,6,target.transform);
    }
    records.push_back(record);
  }
  rhi::BufferDesc desc{};desc.usage=rhi::BufferUsage::ShaderResource;desc.defaultState=rhi::ResourceState::ShaderResource;
  desc.size=records.size()*sizeof(MapRayMaterial);desc.elementSize=sizeof(MapRayMaterial);
  return SLANG_SUCCEEDED(map.device->createBuffer(desc,records.data(),map.ray_primitives.writeRef()));
}
}
