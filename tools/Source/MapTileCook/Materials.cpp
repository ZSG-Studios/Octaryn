#include "TileCook.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>

namespace octaryn::tools::tiles {
std::ostringstream json_stream() {
  std::ostringstream result;result.imbue(std::locale::classic());
  result<<std::setprecision(std::numeric_limits<float>::max_digits10);return result;
}
namespace {
std::string texture_json(const MapTexture& texture,int index,const char* extra,float value) {
  auto out=json_stream();const auto* m=texture.transform;
  const auto sx=std::hypot(m[0],m[3]);
  const auto angle=sx>0?std::atan2(m[3],m[0]):std::atan2(-m[1],m[4]);
  const auto sy=-std::sin(angle)*m[1]+std::cos(angle)*m[4];
  out<<"{\"index\":"<<index<<",\"texCoord\":"<<texture.texcoord;
  if(extra)out<<",\""<<extra<<"\":"<<value;
  out<<",\"extensions\":{\"KHR_texture_transform\":{\"offset\":["<<m[2]<<','<<m[5]
     <<"],\"scale\":["<<sx<<','<<sy<<"],\"rotation\":"<<angle<<"}}}";
  return out.str();
}
}
std::string material_json(const MapMaterial& material,const std::array<int,5>& textures) {
  auto out=json_stream();out<<"{\"pbrMetallicRoughness\":{\"baseColorFactor\":[";
  for(unsigned i=0;i<4;++i)out<<(i?",":"")<<material.base_color[i];
  out<<"],\"metallicFactor\":"<<material.metallic<<",\"roughnessFactor\":"<<material.roughness;
  if(textures[0]>=0)out<<",\"baseColorTexture\":"<<texture_json(material.textures[0],textures[0],nullptr,0);
  if(textures[1]>=0)out<<",\"metallicRoughnessTexture\":"<<texture_json(material.textures[1],textures[1],nullptr,0);
  out<<"},\"alphaMode\":\""<<(material.alpha_mode==MapAlphaMode::Mask?"MASK":material.alpha_mode==MapAlphaMode::Blend?"BLEND":"OPAQUE")
     <<"\",\"alphaCutoff\":"<<material.alpha_cutoff<<",\"doubleSided\":"<<(material.double_sided?"true":"false");
  const auto strength=std::max({1.f,material.emissive[0],material.emissive[1],material.emissive[2]});
  out<<",\"emissiveFactor\":["<<material.emissive[0]/strength<<','<<material.emissive[1]/strength<<','<<material.emissive[2]/strength
     <<"],\"extensions\":{\"KHR_materials_emissive_strength\":{\"emissiveStrength\":"<<strength<<"}}";
  if(textures[2]>=0)out<<",\"normalTexture\":"<<texture_json(material.textures[2],textures[2],"scale",material.normal_scale);
  if(textures[3]>=0)out<<",\"occlusionTexture\":"<<texture_json(material.textures[3],textures[3],"strength",material.occlusion_strength);
  if(textures[4]>=0)out<<",\"emissiveTexture\":"<<texture_json(material.textures[4],textures[4],nullptr,0);
  out<<'}';return out.str();
}
}
