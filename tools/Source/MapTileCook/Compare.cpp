#include "TileCook.h"
#include "MapTextureCache.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace octaryn::tools::tiles {
namespace {
using Digest=std::array<std::uint8_t,32>;
using Bounds=std::array<double,6>;
struct Cook {std::vector<Digest> triangles;std::vector<Bounds> bounds;std::string identity,digest;};
void word(std::vector<std::uint8_t>& bytes,std::uint32_t value) {
  for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(i*8)));
}
void scalar(std::vector<std::uint8_t>& bytes,float value) {
  if(!std::isfinite(value))throw std::runtime_error("nonfinite triangle/material attribute");
  word(bytes,std::bit_cast<std::uint32_t>(value==0?0.f:value));
}
template<size_t N> void floats(std::vector<std::uint8_t>& bytes,const float (&values)[N]) {
  for(auto value:values)scalar(bytes,value);
}
Digest digest(std::span<const std::uint8_t> bytes) {
  const auto hex=map_texture_digest(bytes);Digest result{};
  const auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};
  for(size_t i=0;i<result.size();++i)result[i]=static_cast<std::uint8_t>(digit(hex[i*2])*16+digit(hex[i*2+1]));
  return result;
}
Digest material_digest(const MapMaterial& material,const std::vector<std::string>& images) {
  std::vector<std::uint8_t> bytes;
  floats(bytes,material.base_color);floats(bytes,material.emissive);
  for(float value:{material.metallic,material.roughness,material.alpha_cutoff,material.normal_scale,material.occlusion_strength})scalar(bytes,value);
  word(bytes,material.layer_count);word(bytes,material.unlit?1u:0u);word(bytes,unsigned(material.alpha_mode));word(bytes,material.double_sided?1u:0u);
  for(const auto& texture:material.textures) {
    word(bytes,texture.image>=0?1u:0u);if(texture.image<0)continue;
    const auto& image=images.at(texture.image);bytes.insert(bytes.end(),image.begin(),image.end());
    for(auto value:{texture.texcoord,texture.wrap_s,texture.wrap_t,texture.min_filter,texture.mag_filter})word(bytes,value);
    floats(bytes,texture.transform);
  }
  return digest(bytes);
}
Cook read_cook(const std::filesystem::path& directory) {
  Cook cook;std::vector<std::filesystem::path> files;std::string error;
  const bool source=std::filesystem::is_regular_file(directory) && directory.extension()==".glb";
  if(source)files.push_back(directory);
  else for(const auto& entry:std::filesystem::directory_iterator(directory/"tiles"))
      if(entry.path().extension()==".glb")files.push_back(entry.path());
  std::sort(files.begin(),files.end());if(files.empty())throw std::runtime_error("empty cooked tile set");
  std::vector<std::uint8_t> identity;
  for(const auto& file:files) {
    MapModel model;if(!load_map_model(file,model,error))throw std::runtime_error(error);
    if(model.indices.size()%3 || (!source && model.indices.size()/3>16384))throw std::runtime_error("cooked triangle bound");
    const auto file_hash=map_texture_file_digest(file,error);if(file_hash.empty())throw std::runtime_error(error);
    const auto name=file.filename().generic_string();identity.insert(identity.end(),name.begin(),name.end());identity.push_back(0);
    identity.insert(identity.end(),file_hash.begin(),file_hash.end());
    std::vector<std::string> images;for(const auto& image:model.images)images.push_back(map_texture_digest(image.bytes));
    Bounds bounds{INFINITY,INFINITY,INFINITY,-INFINITY,-INFINITY,-INFINITY};
    size_t count=0;
    for(const auto& primitive:model.primitives) {
      const auto material=material_digest(primitive.material,images);
      if(primitive.index_count%3)throw std::runtime_error("invalid primitive index range");
      for(unsigned i=0;i<primitive.index_count;i+=3) {
        std::vector<std::uint8_t> bytes(material.begin(),material.end());bytes.reserve(248);
        for(unsigned corner=0;corner<3;++corner) {
          const auto& vertex=model.vertices.at(model.indices.at(primitive.first_index+i+corner));
          floats(bytes,vertex.position);floats(bytes,vertex.normal);floats(bytes,vertex.uv);floats(bytes,vertex.uv1);
          floats(bytes,vertex.tangent);floats(bytes,vertex.color);
          for(unsigned axis=0;axis<3;++axis) {
            bounds[axis]=std::min(bounds[axis],double(vertex.position[axis]));
            bounds[axis+3]=std::max(bounds[axis+3],double(vertex.position[axis]));
          }
        }
        cook.triangles.push_back(digest(bytes));++count;
      }
    }
    if(count!=model.indices.size()/3 || !count)throw std::runtime_error("primitive triangle coverage differs");
    cook.bounds.push_back(bounds);
  }
  for(const char* name:{"map.json","cook.json"})if(!source && std::filesystem::exists(directory/name)) {
    const auto hash=map_texture_file_digest(directory/name,error);if(hash.empty())throw std::runtime_error(error);
    identity.insert(identity.end(),hash.begin(),hash.end());
  }
  cook.identity=map_texture_digest(identity);std::sort(cook.triangles.begin(),cook.triangles.end());
  const auto* bytes=reinterpret_cast<const std::uint8_t*>(cook.triangles.data());
  cook.digest=map_texture_digest(std::span(bytes,cook.triangles.size()*sizeof(Digest)));return cook;
}
std::string overlap(const Cook& cook) {
  std::vector<unsigned> neighbors(cook.bounds.size());std::uint64_t pairs=0,strong=0;
  const auto volume=[](const Bounds& b){return (b[3]-b[0])*(b[4]-b[1])*(b[5]-b[2]);};
  for(size_t i=0;i<cook.bounds.size();++i)for(size_t j=0;j<i;++j) {
    const auto& a=cook.bounds[i];const auto& b=cook.bounds[j];double intersection=1;
    for(unsigned k=0;k<3;++k)intersection*=std::max(0.,std::min(a[k+3],b[k+3])-std::max(a[k],b[k]));
    if(intersection>0) {++pairs;++neighbors[i];++neighbors[j];}
    const auto smaller=std::min(volume(a),volume(b));if(smaller>0 && intersection>=.8*smaller)++strong;
  }
  std::sort(neighbors.begin(),neighbors.end());auto out=json_stream();
  out<<"{\"tiles\":"<<cook.bounds.size()<<",\"positive_volume_pairs\":"<<pairs<<",\"overlap80_pairs\":"<<strong
      <<",\"median_neighbors\":"<<(neighbors[(neighbors.size()-1)/2]+neighbors[neighbors.size()/2])/2.
      <<",\"max_neighbors\":"<<neighbors.back()<<",\"bounds\":[";
  for(size_t i=0;i<cook.bounds.size();++i) {
    out<<(i?",":"")<<'[';for(unsigned k=0;k<6;++k)out<<(k?",":"")<<cook.bounds[i][k];out<<']';
  }
  out<<"]}";return out.str();
}
}
bool compare_tile_cooks(const std::filesystem::path& reference,const std::filesystem::path& candidate,
    const std::filesystem::path& destination) {
  auto a=read_cook(reference),b=read_cook(candidate);
  if(a.triangles!=b.triangles)throw std::runtime_error("cooked oriented triangle/material/texture-content SHA256 multiset differs");
  auto receipt=json_stream();receipt<<"{\"version\":1,\"scope\":\"oriented_triangle_all_attributes_material_texture_content_sha256_multiset\",\"triangles\":"
      <<a.triangles.size()<<",\"multiset_sha256\":\""<<a.digest<<"\",\"reference_identity\":\""<<a.identity
      <<"\",\"candidate_identity\":\""<<b.identity<<"\",\"reference_bounds\":"<<overlap(a)<<",\"candidate_bounds\":"<<overlap(b)<<"}\n";
  std::ofstream output(destination.empty()?candidate/"compare.json":destination,std::ios::binary);
  output<<receipt.str();output.close();
  if(!output)throw std::runtime_error("candidate comparison receipt write failed");
  std::printf("map_tile_compare passed=1 triangles=%zu sha256=%s\n",a.triangles.size(),a.digest.c_str());return true;
}
}
