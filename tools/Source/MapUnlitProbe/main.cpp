#include "../../../octaryn-client/Source/MapWorld/MapMaterials.h"
#include "../../../octaryn-client/Source/MapWorld/MapMaterialRecord.h"
#include <fastgltf/core.hpp>
#include "../../../octaryn-client/Source/Rendering/Hdr/WorldHdr.h"
#include "../../../octaryn-client/Source/VirtualGeometry/SceneMaterialJson.h"
#include <cstdio>
#include <stdexcept>
#include <string>
using namespace octaryn::client::rendering;
unsigned checks{};
void check(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
void material_case(const char* alpha,bool unlit) {
  const std::string json=std::string(R"({"asset":{"version":"2.0"},"extensionsUsed":["KHR_materials_unlit"],"extensionsRequired":["KHR_materials_unlit"],
    "images":[{"uri":"test.png"}],"textures":[{"source":0}],"materials":[{
    "pbrMetallicRoughness":{"baseColorFactor":[0.2,0.3,0.4,0.7],"baseColorTexture":{"index":0},"metallicFactor":0.9,"roughnessFactor":0.2},
    "emissiveFactor":[0.8,0.9,1],"normalTexture":{"index":0},"occlusionTexture":{"index":0},"emissiveTexture":{"index":0},
    "doubleSided":true,"alphaCutoff":0.3,"alphaMode":")")+alpha+"\""+
    (unlit?",\"extensions\":{\"KHR_materials_unlit\":{}}":"")+"}]}";
  auto data=fastgltf::GltfDataBuffer::FromBytes(reinterpret_cast<const std::byte*>(json.data()),json.size());
  check(data.error()==fastgltf::Error::None,"authored material source map failed");
  fastgltf::Parser unsupported;
  check(unsupported.loadGltf(data.get(),{},fastgltf::Options::None).error()!=fastgltf::Error::None,
      "required extension fixture failed to exercise parser admission");
  fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_unlit);
  auto source=parser.loadGltf(data.get(),{},fastgltf::Options::None);
  check(source.error()==fastgltf::Error::None,"enabled unlit extension did not parse");
  fastgltf::Primitive primitive;primitive.materialIndex=0;
  const auto material=load_map_material(source.get(),primitive);
  check(material.unlit==unlit,"parser lost unlit or changed lit fallback");
  check(material.double_sided && material.alpha_cutoff==.3f,"unlit lost authored alpha or sidedness");
  const auto mode=std::string(alpha)=="OPAQUE"?MapAlphaMode::Opaque:std::string(alpha)=="MASK"?MapAlphaMode::Mask:MapAlphaMode::Blend;
  check(material.alpha_mode==mode,"unlit changed alpha mode");
  check(material.base_color[0]==.2f && material.base_color[3]==.7f && material.textures[0].image==0,
      "unlit lost base color factor or original texture");
  check(material.textures[2].image==(unlit?-1:0) && material.textures[3].image==(unlit?-1:0) && material.textures[4].image==(unlit?-1:0),
      "lighting-only textures consumed for unlit or lost for lit");
  std::string encoded;
  check(!glz::write_json(material,encoded),"catalog material serialization failed");
  MapMaterial restored;check(!glz::read_json(restored,encoded),"catalog material deserialization failed");
  check(restored.unlit==unlit && restored.alpha_mode==mode && restored.textures[0].image==0,
      "catalog roundtrip lost unlit, alpha or texture identity");
  const auto record=map_material_record(material,12345);
  check(record.padding[0]==12345 && record.padding[1]==unsigned(unlit),"GPU material flags corrupted triangle identity");
  check(record.alpha_mode==unsigned(mode) && record.double_sided==1 && record.base_color[3]==.7f,
      "GPU upload lost alpha/sidedness/base color");
}
int main() {
  try {
    for(const char* alpha:{"OPAQUE","MASK","BLEND"})for(bool unlit:{false,true})material_case(alpha,unlit);
    const auto record=map_material_record(MapMaterial{},0);
    check(record.padding[1]==0 && record.padding[0]==0,"default material became unlit");
    MapMaterial legacy;check(!glz::read_json(legacy,std::string("{}")) && !legacy.unlit,"legacy catalog default changed");
    check(world_gbuffer_formats[3]==rhi::Format::RGBA8Unorm,"material encoding format contract changed");
    check(sizeof(MapRayMaterial)==1072 && offsetof(MapRayMaterial,textures)==64,"GPU material ABI changed");
    std::printf("map_unlit_checks passed=1 assertions=%u gpu=0\n",checks);return 0;
  }catch(const std::exception& error) {std::fprintf(stderr,"map_unlit_checks failed=%s\n",error.what());return 1;}
}
