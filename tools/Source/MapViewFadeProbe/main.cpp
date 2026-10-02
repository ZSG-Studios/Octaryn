#include "../../../octaryn-client/Source/MapWorld/MapModel.h"
#include "../../../octaryn-client/Source/MapWorld/MapMaterialRecord.h"
#include "../../../octaryn-client/Source/VirtualGeometry/SceneMaterialJson.h"
#include "../../../octaryn-client/Source/MapWorld/MapMaterials.h"
#include "../MapTileCook/TileCook.h"
#include <cstdio>
#include <stdexcept>
using namespace octaryn::client::rendering;
unsigned assertions{};
void check(bool value,const char* message) {++assertions;if(!value)throw std::runtime_error(message);}
MapMaterial parse(const std::string& material) {
  const auto text=std::string(R"({"asset":{"version":"2.0"},"materials":[)")+material+"]}";
  auto bytes=fastgltf::GltfDataBuffer::FromBytes(reinterpret_cast<const std::byte*>(text.data()),text.size());
  fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_unlit | fastgltf::Extensions::KHR_materials_emissive_strength);MapLayerImport extras;extras.bind(parser);
  auto loaded=parser.loadGltf(bytes.get(),{},fastgltf::Options::None);check(loaded.error()==fastgltf::Error::None,"fixture parser failed");
  extras.validate();fastgltf::Primitive p;p.materialIndex=0;return load_map_material(loaded.get(),p,&extras);
}
int main() {
  try {
    check(sizeof(MapRayMaterial)==1072,"GPU material ABI changed");
    auto normal=parse(R"({"alphaMode":"BLEND"})");check(!normal.additive && !normal.view_fade,"normal alpha behavior changed");
    for(const char* parameters:{"[0,1,0,1]","[1,0,1,0]","[1,-4.37113883e-08,1,0]"}) {
      const auto material=parse(std::string(R"({"alphaMode":"BLEND","extensions":{"KHR_materials_unlit":{}},"extras":{"octaryn_blend":"additive","octaryn_view_fade":{"version":1,"parameters":)")+parameters+"}}}");
      check(material.unlit && material.additive && material.view_fade,"declared forward flags dropped");
      std::string encoded;check(!glz::write_json(material,encoded),"scene catalog serialization failed");MapMaterial restored;
      check(!glz::read_json(restored,encoded) && restored.additive && restored.view_fade && restored.view_fade_parameters==material.view_fade_parameters,"catalog lost fade parameters");
      std::array<int,21> textures;textures.fill(-1);
      const auto tile=parse(octaryn::tools::tiles::material_json(material,textures));
      check(tile.unlit && tile.additive && tile.view_fade && tile.view_fade_parameters==material.view_fade_parameters,"tile material lost forward flags");
    }
    for(const char* material:{
      R"({"alphaMode":"OPAQUE","extras":{"octaryn_blend":"additive"}})",
      R"({"alphaMode":"MASK","extras":{"octaryn_view_fade":{"version":1,"parameters":[0,1,0,1]}}})",
      R"({"alphaMode":"BLEND","extras":{"octaryn_blend":"multiply"}})",
      R"({"alphaMode":"BLEND","extras":{"octaryn_blend":1}})",
      R"({"alphaMode":"BLEND","extras":{"octaryn_view_fade":{"version":2,"parameters":[0,1,0,1]}}})",
      R"({"alphaMode":"BLEND","extras":{"octaryn_view_fade":{"version":1,"parameters":[0,1,0]}}})",
      R"({"alphaMode":"BLEND","extras":{"octaryn_view_fade":{"version":1,"parameters":[0,0,0,1]}}})",
      R"({"alphaMode":"BLEND","extras":{"octaryn_view_fade":{"version":1,"parameters":[-2,1,0,1]}}})",
      R"({"alphaMode":"BLEND","extras":{"octaryn_view_fade":{"version":1,"parameters":[0,1,0,2]}}})",
      R"({"alphaMode":"BLEND","extras":{"octaryn_view_fade":{"version":1,"parameters":[0,1,0,"bad"]}}})",
      R"({"alphaMode":"BLEND","extras":{"octaryn_view_fade":{"version":1,"parameters":[0,1,0,1],"unknown":1}}})"}) {
      bool rejected=false;try {parse(material);}catch(const std::exception&) {rejected=true;}check(rejected,"unsupported forward material accepted");
    }
    std::printf("map_view_fade passed=1 assertions=%u gpu=0\n",assertions);return 0;
  }catch(const std::exception& error) {std::fprintf(stderr,"map_view_fade failed=%s\n",error.what());return 1;}
}
