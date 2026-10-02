#include "MapMaterials.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace octaryn::client::rendering {
namespace {
MapTexture texture(const fastgltf::Asset& asset,const fastgltf::TextureInfo& info) {
  MapTexture result;
  if(info.textureIndex>=asset.textures.size())throw std::runtime_error("map texture out of range");
  const auto& source=asset.textures[info.textureIndex];
  if(!source.imageIndex || *source.imageIndex>=asset.images.size())
    throw std::runtime_error("map texture requires a supported PNG/JPEG image");
  result.image=static_cast<std::int32_t>(*source.imageIndex);
  result.texcoord=static_cast<std::uint32_t>(info.texCoordIndex);
  if(info.transform) {
    const auto& t=*info.transform;
    const float c=std::cos(t.rotation),s=std::sin(t.rotation);
    result.transform[0]=c*t.uvScale[0];result.transform[1]=-s*t.uvScale[1];result.transform[2]=t.uvOffset[0];
    result.transform[3]=s*t.uvScale[0];result.transform[4]=c*t.uvScale[1];result.transform[5]=t.uvOffset[1];
    if(t.texCoordIndex)result.texcoord=static_cast<std::uint32_t>(*t.texCoordIndex);
  }
  if(result.texcoord>1)throw std::runtime_error("map texture requires TEXCOORD_0 or TEXCOORD_1");
  if(source.samplerIndex) {
    const auto& sampler=asset.samplers.at(*source.samplerIndex);
    result.wrap_s=static_cast<std::uint32_t>(sampler.wrapS);
    result.wrap_t=static_cast<std::uint32_t>(sampler.wrapT);
    if(sampler.minFilter)result.min_filter=static_cast<std::uint32_t>(*sampler.minFilter);
    if(sampler.magFilter)result.mag_filter=static_cast<std::uint32_t>(*sampler.magFilter);
  }
  return result;
}
}
MapMaterial load_map_material(const fastgltf::Asset& asset,const fastgltf::Primitive& primitive,const MapLayerImport* layers) {
  MapMaterial result;
  if(!primitive.materialIndex)return result;
  const auto& source=asset.materials.at(*primitive.materialIndex);
  for(unsigned i=0;i<4;++i)result.base_color[i]=source.pbrData.baseColorFactor[i];
  result.metallic=source.pbrData.metallicFactor;result.roughness=source.pbrData.roughnessFactor;
  for(unsigned i=0;i<3;++i)result.emissive[i]=source.emissiveFactor[i]*source.emissiveStrength;
  result.alpha_mode=source.alphaMode==fastgltf::AlphaMode::Blend?MapAlphaMode::Blend:
      source.alphaMode==fastgltf::AlphaMode::Mask?MapAlphaMode::Mask:MapAlphaMode::Opaque;
  result.alpha_cutoff=source.alphaCutoff;result.double_sided=source.doubleSided;
  result.unlit=source.unlit;
  result.zero_basis=layers && layers->zero_basis(*primitive.materialIndex);
  if(result.zero_basis && (result.unlit || !source.normalTexture || result.alpha_mode!=MapAlphaMode::Opaque || layers->find(*primitive.materialIndex)))
    throw std::runtime_error("zero tangent plane requires lit opaque material and authored normal map");
  if(layers)layers->forward(*primitive.materialIndex,result.additive,result.view_fade,result.view_fade_parameters);
  if((result.additive || result.view_fade) && result.alpha_mode!=MapAlphaMode::Blend)
    throw std::runtime_error("view fade and additive blending require BLEND material");
  if(source.pbrData.baseColorTexture)result.textures[0]=texture(asset,*source.pbrData.baseColorTexture);
  if(result.unlit) {if(layers && layers->find(*primitive.materialIndex))throw std::runtime_error("weighted layers require lit opaque material");result.texture=result.textures[0].image;return result;}
  if(source.pbrData.metallicRoughnessTexture)result.textures[1]=texture(asset,*source.pbrData.metallicRoughnessTexture);
  if(source.normalTexture) {result.textures[2]=texture(asset,*source.normalTexture);result.normal_scale=source.normalTexture->scale;}
  if(source.occlusionTexture) {result.textures[3]=texture(asset,*source.occlusionTexture);result.occlusion_strength=source.occlusionTexture->strength;}
  if(source.emissiveTexture)result.textures[4]=texture(asset,*source.emissiveTexture);
  if(layers)if(const auto* declaration=layers->find(*primitive.materialIndex)) {
    if(result.unlit || result.alpha_mode!=MapAlphaMode::Opaque)
      throw std::runtime_error("weighted layers require lit opaque material");
    result.layer_count=unsigned(declaration->layers.size());
    for(unsigned i=0;i<result.layer_count;++i) {
      const auto& layer=declaration->layers[i];
      const auto load=[&](std::uint32_t index) {
        fastgltf::TextureInfo info;info.textureIndex=index;
        auto value=texture(asset,info);value.texcoord=layer.texcoord;
        std::copy(layer.transform.begin(),layer.transform.end(),value.transform);return value;
      };
      result.textures[5+i*2]=load(layer.texture);
      if(layer.normal_texture>=0)result.textures[6+i*2]=load(unsigned(layer.normal_texture));
    }
    result.textures[0]=result.textures[5];result.textures[2]=result.textures[6];
    result.textures[2].texcoord=result.textures[5].texcoord;
    std::copy_n(result.textures[5].transform,6,result.textures[2].transform);
  }
  result.texture=result.textures[0].image;
  return result;
}
}
