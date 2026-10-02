#include "TileCook.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <unordered_map>

namespace octaryn::tools::tiles {
namespace {
struct Primitive {unsigned source{},vertex{},vertices{},index{},indices{};std::array<float,6> bounds;};
void bounds_add(std::array<float,6>& bounds,const float* position) {
  for(unsigned axis=0;axis<3;++axis) {bounds[axis]=std::min(bounds[axis],position[axis]);bounds[axis+3]=std::max(bounds[axis+3],position[axis]);}
}
std::array<float,6> empty_bounds() {
  const auto maximum=std::numeric_limits<float>::max();return {maximum,maximum,maximum,-maximum,-maximum,-maximum};
}
void write_u32(std::ofstream& file,std::uint32_t value) {file.write(reinterpret_cast<const char*>(&value),4);}
}
bool write_tile(const MapModel& source,std::span<const Triangle> triangles,const TextureFiles& texture_files,
    const std::filesystem::path& path,TileResult& result,std::string& error) {
  if(triangles.empty()) {error="cannot write empty tile";return false;}
  std::map<unsigned,std::vector<unsigned>> groups;
  for(const auto triangle:triangles)groups[triangle.primitive].push_back(triangle.first);
  std::vector<MapVertex> vertices;std::vector<unsigned> indices;std::vector<Primitive> primitives;
  result.bounds=empty_bounds();result.triangles=static_cast<unsigned>(triangles.size());
  for(const auto& [primitive,starts]:groups) {
    Primitive item{primitive,static_cast<unsigned>(vertices.size()),0,static_cast<unsigned>(indices.size()),0,empty_bounds()};
    std::unordered_map<unsigned,unsigned> remap;
    for(const auto first:starts)for(unsigned corner=0;corner<3;++corner) {
      const auto vertex=source.indices.at(first+corner);auto found=remap.find(vertex);
      if(found==remap.end()) {
        found=remap.emplace(vertex,static_cast<unsigned>(vertices.size())-item.vertex).first;
        vertices.push_back(source.vertices.at(vertex));bounds_add(item.bounds,vertices.back().position);bounds_add(result.bounds,vertices.back().position);
      }
      indices.push_back(found->second);
    }
    item.vertices=static_cast<unsigned>(vertices.size())-item.vertex;item.indices=static_cast<unsigned>(indices.size())-item.index;primitives.push_back(item);
  }
  auto json=json_stream();json<<"{\"asset\":{\"version\":\"2.0\",\"generator\":\"Octaryn exact tile cooker v1\"},"
      "\"extensionsUsed\":[\"KHR_texture_transform\",\"KHR_materials_emissive_strength\",\"KHR_materials_unlit\"],\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],";
  const auto vertex_bytes=vertices.size()*sizeof(MapVertex),index_bytes=indices.size()*4,total=vertex_bytes+index_bytes;
  json<<"\"buffers\":[{\"byteLength\":"<<total<<"}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":"<<vertex_bytes
      <<",\"byteStride\":112,\"target\":34962},{\"buffer\":0,\"byteOffset\":"<<vertex_bytes<<",\"byteLength\":"<<index_bytes<<",\"target\":34963}],\"accessors\":[";
  constexpr unsigned offsets[]{0,12,24,32,48,64,80,96};constexpr const char* types[]{"VEC3","VEC3","VEC2","VEC2","VEC4","VEC4","VEC4","VEC4"};
  for(size_t p=0;p<primitives.size();++p) {
    const auto& item=primitives[p];if(p)json<<',';
    for(unsigned attribute=0;attribute<8;++attribute) {
      if(attribute)json<<',';
      json<<"{\"bufferView\":0,\"byteOffset\":"<<item.vertex*112+offsets[attribute]<<",\"componentType\":5126,\"count\":"<<item.vertices<<",\"type\":\""<<types[attribute]<<'"';
      if(attribute==0)json<<",\"min\":["<<item.bounds[0]<<','<<item.bounds[1]<<','<<item.bounds[2]<<"],\"max\":["<<item.bounds[3]<<','<<item.bounds[4]<<','<<item.bounds[5]<<']';
      json<<'}';
    }
    json<<",{\"bufferView\":1,\"byteOffset\":"<<item.index*4<<",\"componentType\":5125,\"count\":"<<item.indices<<",\"type\":\"SCALAR\"}";
  }
  json<<"],\"meshes\":[{\"primitives\":[";
  constexpr const char* attributes[]{"POSITION","NORMAL","TEXCOORD_0","TEXCOORD_1","TANGENT","COLOR_0","_OCTARYN_BLEND0","_OCTARYN_BLEND1"};
  for(size_t p=0;p<primitives.size();++p) {
    if(p)json<<',';json<<"{\"attributes\":{";
    const auto& item=primitives[p];bool tangent=false;
    for(unsigned i=0;i<item.vertices;++i)tangent|=vertices[item.vertex+i].tangent[3]!=0;
    for(unsigned attribute=0;attribute<(source.primitives[item.source].material.layer_count?8u:6u);++attribute) {
      if(attribute==4 && !tangent)continue;
      json<<(attribute?",":"")<<'"'<<attributes[attribute]<<"\":"<<p*9+attribute;
    }
    json<<"},\"indices\":"<<p*9+8<<",\"material\":"<<p<<",\"mode\":4}";
  }
  json<<"]}],\"materials\":[";std::vector<MapTexture> textures;std::map<int,unsigned> images;
  for(size_t p=0;p<primitives.size();++p) {
    const auto& material=source.primitives[primitives[p].source].material;std::array<int,21> slots;slots.fill(-1);
    for(unsigned role=0;role<21;++role)if(material.textures[role].image>=0) {
      slots[role]=static_cast<int>(textures.size());textures.push_back(material.textures[role]);
      if(!images.contains(material.textures[role].image))images.emplace(material.textures[role].image,static_cast<unsigned>(images.size()));
    }
    if(p)json<<',';json<<material_json(material,slots);
  }
  json<<']';
  if(!textures.empty()) {
  json<<",\"textures\":[";
  for(size_t i=0;i<textures.size();++i)json<<(i?",":"")<<"{\"source\":"<<images.at(textures[i].image)<<",\"sampler\":"<<i<<'}';
  json<<"],\"samplers\":[";
  for(size_t i=0;i<textures.size();++i) {
    const auto& texture=textures[i];json<<(i?",":"")<<"{\"wrapS\":"<<texture.wrap_s<<",\"wrapT\":"<<texture.wrap_t
        <<",\"minFilter\":"<<texture.min_filter<<",\"magFilter\":"<<texture.mag_filter<<'}';
  }
  json<<"],\"images\":[";std::vector<int> image_order(images.size());for(const auto& [image,index]:images)image_order[index]=image;
  for(size_t i=0;i<image_order.size();++i)json<<(i?",":"")<<"{\"uri\":\""<<texture_files.images.at(image_order[i]).uri<<"\"}";
  json<<']';
  }
  json<<'}';auto text=json.str();while(text.size()%4)text+=' ';
  if(total+text.size()+28>std::numeric_limits<unsigned>::max()) {error="tile GLB exceeds size limit";return false;}
  std::ofstream file(path,std::ios::binary);write_u32(file,0x46546c67);write_u32(file,2);write_u32(file,static_cast<unsigned>(total+text.size()+28));
  write_u32(file,static_cast<unsigned>(text.size()));write_u32(file,0x4e4f534a);file.write(text.data(),text.size());
  write_u32(file,static_cast<unsigned>(total));write_u32(file,0x004e4942);
  file.write(reinterpret_cast<const char*>(vertices.data()),vertex_bytes);file.write(reinterpret_cast<const char*>(indices.data()),index_bytes);
  if(!file) {error="failed writing tile GLB";return false;}return true;
}
}
