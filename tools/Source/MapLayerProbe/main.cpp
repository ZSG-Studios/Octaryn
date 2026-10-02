#include "../../../octaryn-client/Source/MapWorld/MapLayerImport.h"
#include "../../../octaryn-client/Source/MapWorld/MapMaterials.h"
#include "../../../octaryn-client/Source/MapWorld/MapMaterialRecord.h"
#include "../../../octaryn-client/Source/VirtualGeometry/SceneMaterialJson.h"
#include "../../../octaryn-client/Source/MapWorld/MapSourceReader.h"
#include "../MapTileCook/TileCook.h"
#include <cstdio>
#include <stdexcept>
using namespace octaryn::client::rendering;
unsigned assertions{};
void check(bool value,const char* why) {++assertions;if(!value)throw std::runtime_error(why);}
bool parse(const std::string& extras,MapLayerImport& layers) {
  const std::string source=R"({"asset":{"version":"2.0"},"materials":[{"extras":)"+extras+"}]}";
  auto bytes=fastgltf::GltfDataBuffer::FromBytes(reinterpret_cast<const std::byte*>(source.data()),source.size());
  fastgltf::Parser parser;layers.bind(parser);
  auto asset=parser.loadGltf(bytes.get(),{},fastgltf::Options::None);
  check(asset.error()==fastgltf::Error::None,"authored glTF did not parse");
  try {layers.validate();return true;}catch(const std::exception&) {return false;}
}
int main(int argc,char** argv) {
  try {
    MapLayerImport absent;check(parse("{}",absent) && absent.empty(),"ordinary material changed");
    MapLayerImport valid;
    check(parse(R"({"other":42,"octaryn_material_layers":{"version":1,"layers":[{"texture":2,"normal_texture":3},{"texture":4,"normal_texture":5}]}})",valid),"ordered pair failed");
    const auto* material=valid.find(0);
    check(material && material->layers.size()==2 && material->layers[0].texture==2 &&
        material->layers[1].normal_texture==5,"original ordered texture identity changed");
    check(material->layers[0].transform==std::array<float,6>{1,0,0,0,1,0},"default UV basis changed");
    for(const char* invalid:{
      R"({"version":2,"layers":[{"texture":0}]})",
      R"({"version":1,"layers":[]})",
      R"({"version":1,"layers":[{}]})",
      R"({"version":1,"layers":[{"texture":-1}]})",
      R"({"version":1,"layers":[{"texture":0,"texcoord":2}]})",
      R"({"version":1,"layers":[{"texture":0,"transform":[1,0,0,0,1]}]})",
      R"({"version":1,"layers":[{"texture":0},{"texture":1,"texcoord":1}]})",
      R"({"version":1,"layers":[{"texture":0,"unknown":1}]})",
      R"({"version":1,"layers":[{"texture":0}],"unknown":1})"}) {
      MapLayerImport failed;check(!parse(std::string("{\"octaryn_material_layers\":")+invalid+"}",failed),"unsupported layer declaration accepted");
    }
    check(sizeof(MapVertex)==112 && sizeof(MapRayMaterial)==1072,"weighted GPU ABI mismatch");
    const std::string source=R"({"asset":{"version":"2.0"},"images":[{"uri":"diffuse.png"},{"uri":"normal.png"}],"textures":[{"source":0},{"source":1}],"materials":[{"extras":{"octaryn_material_layers":{"version":1,"layers":[{"texture":0,"normal_texture":1},{"texture":1}]}}}]})";
    auto bytes=fastgltf::GltfDataBuffer::FromBytes(reinterpret_cast<const std::byte*>(source.data()),source.size());
    fastgltf::Parser parser;MapLayerImport import;import.bind(parser);auto asset=parser.loadGltf(bytes.get(),{},fastgltf::Options::None);
    check(asset.error()==fastgltf::Error::None,"layer material parse failed");import.validate();
    fastgltf::Primitive primitive;primitive.materialIndex=0;
    const auto loaded=load_map_material(asset.get(),primitive,&import);
    check(loaded.layer_count==2 && loaded.textures[5].image==0 && loaded.textures[6].image==1 && loaded.textures[7].image==1 && loaded.textures[8].image==-1,"ordered layer identities lost");
    check(loaded.textures[0].image==0 && loaded.textures[2].image==1,"authored tangent frame lost");
    check(map_material_record(loaded,37).padding[1]==512 && map_material_record(loaded,37).padding[0]==37,"layer count damaged unrelated GPU flags");
    std::string serialized;check(!glz::write_json(loaded,serialized),"layer catalog write failed");
    MapMaterial restored;check(!glz::read_json(restored,serialized) && restored.layer_count==2 && restored.textures[8].image==-1 && restored.textures[6].image==1,"layer catalog roundtrip lost source data");
    for(const auto mode:{MapAlphaMode::Mask,MapAlphaMode::Blend}) {
      asset.get().materials[0].alphaMode=mode==MapAlphaMode::Mask?fastgltf::AlphaMode::Mask:fastgltf::AlphaMode::Blend;
      bool rejected=false;try{load_map_material(asset.get(),primitive,&import);}catch(const std::exception&){rejected=true;}
      check(rejected,"transparent weighted layer admitted");
    }
    asset.get().materials[0].alphaMode=fastgltf::AlphaMode::Opaque;asset.get().materials[0].unlit=true;
    bool rejected=false;try{load_map_material(asset.get(),primitive,&import);}catch(const std::exception&){rejected=true;}
    check(rejected,"unlit weighted layer admitted");
    for(unsigned count:{8u,9u}) {
      std::string text=R"({"octaryn_material_layers":{"version":1,"layers":[)";
      for(unsigned i=0;i<count;++i) {if(i)text+=',';text+="{\"texture\":"+std::to_string(i)+"}";}
      text+="]}}";MapLayerImport layers;check(parse(text,layers)==(count<=8),"bounded layer admission failed");
    }
    const std::string changed=R"({"asset":{"version":"2.0"},"images":[{"uri":"diffuse.png"},{"uri":"normal.png"}],"textures":[{"source":0},{"source":1}],"materials":[{"extras":{"octaryn_material_layers":{"version":1,"layers":[{"texture":0,"texcoord":1,"transform":[2,0,0,0,-2,0]},{"texture":0,"normal_texture":1,"texcoord":1,"transform":[2,0,0,0,-2,0]}]}}}]})";
    auto changed_bytes=fastgltf::GltfDataBuffer::FromBytes(reinterpret_cast<const std::byte*>(changed.data()),changed.size());
    fastgltf::Parser changed_parser;MapLayerImport changed_import;changed_import.bind(changed_parser);
    auto changed_asset=changed_parser.loadGltf(changed_bytes.get(),{},fastgltf::Options::None);
    check(changed_asset.error()==fastgltf::Error::None,"layer normal UV fixture invalid");
    const auto changed_material=load_map_material(changed_asset.get(),primitive,&changed_import);
    check(changed_material.textures[2].image==-1 && changed_material.textures[2].texcoord==1 && changed_material.textures[2].transform[0]==2 && changed_material.textures[2].transform[4]==-2,"base without normal lost shared transformed UV frame");
    check(argc==2,"authored import fixtures required");
    const auto directory=std::filesystem::path(argv[1]);
    for(const auto* fixture:{"valid","missing","negative","unused","normalized","wrongtype"}) {
      const auto path=directory/(std::string(fixture)+".gltf");MapModel flattened;std::string error;
      const bool expected=std::string(fixture)=="valid";
      check(load_map_model(path,flattened,error)==expected,"flattened layer admission mismatch");
      MapSourceReader reader;MapModel streamed;
      const bool opened=reader.open(path,directory/"scratch",error);
      const bool admitted=opened && reader.load(0,0,0,1,streamed,error);
      check(admitted==expected,"streamed layer admission mismatch");
      if(expected) {
        check(flattened.primitives[0].material.layer_count==2 && streamed.primitives[0].material.layer_count==2,"layer count dropped during geometry import");
        check(flattened.vertices.size()==3 && streamed.vertices.size()==3,"layer import altered topology");
        for(const auto& vertex:flattened.vertices)check(vertex.blend0[0]==.25f && vertex.blend0[1]==.75f && vertex.blend1[3]==0 && vertex.color[3]==.5f,"layer weights conflated with vertex alpha");
        octaryn::tools::tiles::TextureFiles files;files.images.resize(1);files.images[0].uri="test.png";
        octaryn::tools::tiles::TileResult result;const std::array<octaryn::tools::tiles::Triangle,1> triangles{{{0,0}}};
        check(octaryn::tools::tiles::write_tile(flattened,triangles,files,directory/"roundtrip.glb",result,error),"weighted tile write failed");
        MapModel tiled;const auto tiled_ok=load_map_model(directory/"roundtrip.glb",tiled,error);
        if(!tiled_ok)std::fprintf(stderr,"tile import error=%s\n",error.c_str());
        check(tiled_ok && tiled.primitives[0].material.layer_count==2,"weighted tile roundtrip lost material contract");
        for(const auto& vertex:tiled.vertices)check(vertex.blend0[0]==.25f && vertex.blend0[1]==.75f && vertex.color[3]==.5f,"weighted tile stride or attributes corrupted");
        for(const auto& vertex:streamed.vertices)check(vertex.blend0[0]==.25f && vertex.blend0[1]==.75f && vertex.blend1[3]==0 && vertex.color[3]==.5f,"streamed layer weights changed");
      }
    }
    std::printf("map_layer_schema passed=1 assertions=%u gpu=0\n",assertions);return 0;
  }catch(const std::exception& error) {std::fprintf(stderr,"map_layer_schema failed=%s\n",error.what());return 1;}
}
