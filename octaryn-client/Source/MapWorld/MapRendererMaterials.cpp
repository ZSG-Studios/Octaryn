#include "MapRendererInternal.h"
#include "MapSamplerCache.h"
#include <algorithm>
#include <array>
#include <map>
#include <cstdio>
namespace octaryn::client::rendering {
namespace {
rhi::TextureAddressingMode wrap(unsigned value) {
  return value==33071?rhi::TextureAddressingMode::ClampToEdge:
      value==33648?rhi::TextureAddressingMode::MirrorRepeat:rhi::TextureAddressingMode::Wrap;
}
}
bool prepare_map_materials(MapRenderer& map,std::vector<MapRayMaterial>& records,size_t maximum) {
  if(!map.sampler_cache)map.sampler_cache=std::make_shared<MapSamplerCache>();
  std::map<std::array<unsigned,4>,std::shared_ptr<MapSamplerResource>> samplers;
  const auto end=records.size()+std::min(maximum,map.model.primitives.size()-records.size());
  for(size_t primitive_index=records.size();primitive_index<end;++primitive_index) {
    const auto& primitive=map.model.primitives[primitive_index];
    const auto& source=primitive.material;
    if(source.layer_count>8 || (source.layer_count && (source.unlit || source.alpha_mode!=MapAlphaMode::Opaque))) {
      std::fprintf(stderr,"map_material_failed operation=layers primitive=%zu reason=invalid_contract\n",primitive_index);return false;
    }
    auto record=map_material_record(source,primitive.first_index/3);
    for(unsigned i=0;i<21;++i) {
      const auto& t=source.textures[i];auto& target=record.textures[i];if(t.image<0)continue;
      const std::array<unsigned,4> key{t.wrap_s,t.wrap_t,t.min_filter,t.mag_filter};
      if(!samplers.contains(key)) {
        rhi::SamplerDesc desc{};desc.addressU=wrap(t.wrap_s);desc.addressV=wrap(t.wrap_t);
        desc.magFilter=t.mag_filter==9728?rhi::TextureFilteringMode::Point:rhi::TextureFilteringMode::Linear;
        desc.minFilter=(t.min_filter==9728 || t.min_filter==9984 || t.min_filter==9986)?rhi::TextureFilteringMode::Point:rhi::TextureFilteringMode::Linear;
        desc.mipFilter=(t.min_filter==9984 || t.min_filter==9985)?rhi::TextureFilteringMode::Point:rhi::TextureFilteringMode::Linear;
        desc.maxLOD=t.min_filter<9984?0:1000;
        if(t.min_filter==9987 && t.mag_filter==9729)desc.maxAnisotropy=8;
        auto sampler=acquire_map_sampler(*map.sampler_cache,map.device.get(),desc);
        if(!sampler) {
          std::fprintf(stderr,"map_material_failed operation=sampler primitive=%zu role=%u\n",primitive_index,i);return false;
        }
        samplers.emplace(key,sampler);
        if(std::find(map.material_samplers.begin(),map.material_samplers.end(),sampler)==map.material_samplers.end())
          map.material_samplers.push_back(std::move(sampler));
      }
      rhi::DescriptorHandle image{};
      const auto slot=map.material_texture_slots[primitive_index][i];
      if(slot>=map.texture_views.size() || !map.texture_views[slot]) {
        std::fprintf(stderr,"map_material_failed operation=texture_view primitive=%zu role=%u slot=%zu reason=missing_view\n",
            primitive_index,i,slot);return false;
      }
      const auto result=map.texture_views[slot]->getDescriptorHandle(rhi::DescriptorHandleAccess::Read,&image);
      if(SLANG_FAILED(result)) {
        std::fprintf(stderr,"map_material_failed operation=texture_descriptor primitive=%zu role=%u slot=%zu result=0x%08x\n",
            primitive_index,i,slot,unsigned(result));return false;
      }
      target.image=image.value;target.sampler=samplers.at(key)->descriptor.value;target.texcoord=t.texcoord;target.present=1;
      std::copy_n(t.transform,6,target.transform);
    }
    records.push_back(record);
  }
  return true;
}
bool upload_map_materials(MapRenderer& map) {
  std::vector<MapRayMaterial> records;
  if(!prepare_map_materials(map,records,SIZE_MAX))return false;
  rhi::BufferDesc desc{};desc.usage=rhi::BufferUsage::ShaderResource;desc.defaultState=rhi::ResourceState::ShaderResource;
  desc.size=records.size()*sizeof(MapRayMaterial);desc.elementSize=sizeof(MapRayMaterial);
  return SLANG_SUCCEEDED(map.device->createBuffer(desc,records.data(),map.ray_primitives.writeRef()));
}
}
