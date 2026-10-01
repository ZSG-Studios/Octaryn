#include "MapSource.h"
#include "MapSourceReader.h"
#include "GltfBufferViews.h"
#include "GltfTriangleReader.h"
#include <meshoptimizer.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

using namespace octaryn::client::rendering;
namespace {
void require(bool okay,const char* message) {if(!okay)throw std::runtime_error(message);}
void write(const std::filesystem::path& path,const std::string& text) {std::ofstream(path,std::ios::binary)<<text;}
std::string fixture(std::size_t vertices,std::size_t indices) {
  std::ostringstream text;
  text<<R"({"asset":{"version":"2.0"},"extensionsRequired":["EXT_meshopt_compression"],"extensionsUsed":["EXT_meshopt_compression"],
"scene":0,"scenes":[{"nodes":[0,1]}],"nodes":[{"mesh":0},{"mesh":0,"translation":[24,0,0]}],
"buffers":[{"uri":"geometry%20data.bin","byteLength":)"<<vertices+indices<<R"(},{"byteLength":72}],
"bufferViews":[{"buffer":1,"byteOffset":0,"byteLength":48,"byteStride":12,"extensions":{"EXT_meshopt_compression":
{"buffer":0,"byteOffset":0,"byteLength":)"<<vertices<<R"(,"byteStride":12,"mode":"ATTRIBUTES","count":4}}},
{"buffer":1,"byteOffset":48,"byteLength":24,"extensions":{"EXT_meshopt_compression":
{"buffer":0,"byteOffset":)"<<vertices<<R"(,"byteLength":)"<<indices<<R"(,"byteStride":4,"mode":"TRIANGLES","count":6}}}],
"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[-12,0,-12],"max":[12,0,12]},
{"bufferView":1,"componentType":5125,"count":6,"type":"SCALAR"}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}]})";
  return text.str();
}
void test_adapter_filters() {
  const std::array<float,12> input{1.25f,-2.5f,4.125f,5.25f,9.5f,-.125f,0.f,1.f,2.f,3.f,4.f,5.f};
  std::array<unsigned,12> filtered{};
  meshopt_encodeFilterExp(filtered.data(),4,12,24,input.data(),meshopt_EncodeExpSeparate);
  std::vector<unsigned char> encoded(meshopt_encodeVertexBufferBound(4,12));
  encoded.resize(meshopt_encodeVertexBuffer(encoded.data(),encoded.size(),filtered.data(),4,12));
  fastgltf::Asset asset;fastgltf::Buffer buffer;buffer.byteLength=encoded.size();
  fastgltf::sources::Vector data;for(auto byte:encoded)data.bytes.push_back(std::byte(byte));buffer.data=std::move(data);
  asset.buffers.push_back(std::move(buffer));
  fastgltf::BufferView view;view.bufferIndex=0;view.byteLength=48;
  // The decoded view may target the virtual fallback buffer separately from the compressed source.
  fastgltf::Buffer fallback;fallback.byteLength=48;fallback.data=fastgltf::sources::Fallback{};
  asset.buffers.push_back(std::move(fallback));view.bufferIndex=1;
  view.meshoptCompression=std::make_unique<fastgltf::CompressedBufferView>(fastgltf::CompressedBufferView{
      0,0,encoded.size(),4,fastgltf::MeshoptCompressionMode::Attributes,fastgltf::MeshoptCompressionFilter::Exponential,12});
  asset.bufferViews.push_back(std::move(view));
  octaryn::assets::GltfBufferViews buffers({},1024,nullptr);const auto bytes=buffers(asset,0);
  std::array<float,12> decoded{};std::memcpy(decoded.data(),bytes.data(),bytes.size());
  for(std::size_t i=0;i<input.size();++i)require(std::abs(decoded[i]-input[i])<.00001f,"meshopt exponential filter mismatch");
  bool rejected=false;try {octaryn::assets::GltfBufferViews tiny({},48,nullptr);tiny(asset,0);}catch(const std::exception&){rejected=true;}
  require(rejected,"decoder working-set bound omitted compressed input");
  std::atomic_bool cancel{true};rejected=false;
  try {octaryn::assets::GltfBufferViews canceled({},1024,&cancel);canceled(asset,0);}catch(const std::exception&){rejected=true;}
  require(rejected,"decoder ignored cancellation");
}
}
bool test_map_import(const std::filesystem::path& root) {
  test_adapter_filters();std::filesystem::create_directories(root);
  const std::array<float,12> positions{-12,0,-12,-12,0,12,12,0,12,12,0,-12};
  const std::array<unsigned,6> indices{0,1,2,0,2,3};
  std::vector<unsigned char> vertices(meshopt_encodeVertexBufferBound(4,12)),triangles(meshopt_encodeIndexBufferBound(6,4));
  vertices.resize(meshopt_encodeVertexBuffer(vertices.data(),vertices.size(),positions.data(),4,12));
  triangles.resize(meshopt_encodeIndexBuffer(triangles.data(),triangles.size(),indices.data(),indices.size()));
  std::ofstream binary(root/"geometry data.bin",std::ios::binary);
  binary.write(reinterpret_cast<const char*>(vertices.data()),static_cast<std::streamsize>(vertices.size()));
  binary.write(reinterpret_cast<const char*>(triangles.data()),static_cast<std::streamsize>(triangles.size()));binary.close();
  const auto source=root/"compressed.gltf";write(source,fixture(vertices.size(),triangles.size()));
  MapSourceInfo info;std::string error;require(inspect_map_source(source,info,error),error.c_str());
  require(info.mesh_count==1 && info.instances.size()==2 && info.unique_triangles==2 && info.instanced_triangles==4,
      "source catalog flattened repeated mesh identity");
  require(info.instances[1].transform[12]==24,"source catalog lost authored instance transform");
  MapModel model;require(load_map_model(source,model,error),error.c_str());
  require(model.primitives.size()==2 && model.indices.size()==12 && model.vertices.size()==12,"compressed default scene topology differs");
  for(const auto& vertex:model.vertices)require(vertex.normal[1]==1.f,"missing normals did not preserve glTF flat shading");
  require(model.primitives[1].bounds_min[0]==12 && model.primitives[1].bounds_max[0]==36,"compressed instance placement differs");
  MapModel selected;require(load_map_source_primitive(source,0,0,selected,error),error.c_str());
  require(selected.indices.size()==6 && selected.primitives.front().bounds_min[0]==-12,"selective primitive was transformed or duplicated");
  {
    MapSourceReader reader;require(reader.open(source,root/"scratch",error),error.c_str());
    for(unsigned triangle=0;triangle<2;++triangle) {
      MapModel range;require(reader.load(0,0,triangle,1,range,error),error.c_str());
      require(range.indices.size()==3 && range.vertices.size()==3,"triangle window changed topology");
      for(unsigned corner=0;corner<3;++corner) {
        const auto& expected=selected.vertices[selected.indices[triangle*3+corner]];
        const auto& actual=range.vertices[range.indices[corner]];
        require(std::memcmp(&expected,&actual,sizeof(MapVertex))==0,"triangle window changed source attributes");
      }
    }
    MapModel rejected;require(!reader.load(0,0,1,2,rejected,error),"triangle window exceeds primitive range");
  }
  require(std::filesystem::is_empty(root/"scratch"),"source reader retained its temporary decoded views");
  {
    octaryn::assets::GltfTriangleReader reader;require(reader.open(source,root/"collision-scratch",error),error.c_str());
    octaryn::assets::GltfTriangleWindow window;
    require(reader.read(0,0,1,1,window,error),error.c_str());
    require(window.indices.size()==3 && window.positions.size()==9,"shared collision window changed topology");
    for(unsigned corner=0;corner<3;++corner)for(unsigned axis=0;axis<3;++axis)
      require(window.positions[window.indices[corner]*3+axis]==selected.vertices[selected.indices[3+corner]].position[axis],
          "shared collision window changed source positions");
    std::array<float,6> exact{};require(reader.bounds(0,0,1,1,exact,error) && exact==window.bounds,"source bounds differ from collision window");
    const std::array<float,16> mirrored{-1,0,0,0,0,1,0,0,0,0,1,0,24,0,0,1};
    const auto original=window;
    require(octaryn::assets::transform_gltf_triangles(window,mirrored,error),error.c_str());
    require(window.indices[1]==original.indices[2] && window.indices[2]==original.indices[1] &&
        window.positions[0]==24-original.positions[0],"collision instance transform lost mirrored winding");
    require(!reader.read(0,0,1,2,window,error),"shared collision window exceeded source range");
  }
  require(std::filesystem::is_empty(root/"collision-scratch"),"shared collision reader retained decoded scratch");
  MapLoadLimits bounded;bounded.triangles=3;
  require(!load_map_model(source,selected,error,bounded) && error.find("streamed instance cook")!=std::string::npos,
      "instance count not bounded before preparation");
  require(selected.indices.size()==6,"rejected import overwrote output");
  auto invalid=fixture(vertices.size(),triangles.size());
  const auto count=invalid.find("\"componentType\":5126,\"count\":4");
  require(count!=std::string::npos,"invalid accessor fixture missing");
  invalid.replace(count,std::string("\"componentType\":5126,\"count\":4").size(),"\"componentType\":5126,\"count\":5");
  write(root/"invalid-range.gltf",invalid);
  require(!load_map_model(root/"invalid-range.gltf",selected,error) && error.find("accessor element count")!=std::string::npos,
      "accessor exceeded the decoded buffer view");
  std::fstream corrupt(root/"geometry data.bin",std::ios::binary|std::ios::in|std::ios::out);corrupt.put(0);corrupt.close();
  require(!load_map_model(source,selected,error) && error.find("meshopt compressed buffer")!=std::string::npos,
      "corrupt meshopt data was accepted");
  const MapMaterial material{};MapModel resources;
  require(load_map_material_resources(source,{&material,1},resources,error),error.c_str());
  require(resources.primitives.size()==1 && resources.vertices.empty() && resources.indices.empty(),
      "material-only preparation decoded geometry or expanded instances");
  binary.open(root/"geometry data.bin",std::ios::binary|std::ios::trunc);
  binary.write(reinterpret_cast<const char*>(vertices.data()),static_cast<std::streamsize>(vertices.size()));
  binary.write(reinterpret_cast<const char*>(triangles.data()),static_cast<std::streamsize>(triangles.size()));binary.close();
  std::printf("map_import_tests=passed meshopt_decode=1 filtered_attributes=1 corrupt_rejected=1 "
      "bounded_decode=1 instance_catalog=1 flat_normals=1 selective_primitive=1 fixture=%s\n",source.string().c_str());return true;
}
