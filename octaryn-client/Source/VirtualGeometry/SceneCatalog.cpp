#include "SceneCatalog.h"
#include "SceneMaterialJson.h"
#include "FilePath.h"
#include "../MapWorld/MapTextureCache.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <set>
#include <random>
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
bool hash_valid(const std::string& hash) {
  return hash.size()==64 && std::all_of(hash.begin(),hash.end(),[](char c){return (c>='0'&&c<='9') || (c>='a'&&c<='f');});
}
void bounds_valid(const std::array<float,6>& bounds) {
  for(float value:bounds)require(std::isfinite(value),"nonfinite scene bounds");
  for(unsigned axis=0;axis<3;++axis)require(bounds[axis]<=bounds[axis+3],"inverted scene bounds");
}
}
bool validate_scene_catalog(const SceneCatalog& scene,std::string& error) {
  try {
    require(scene.version==scene_catalog_version && scene.part_triangles>0 && scene.part_triangles<=65536,"invalid scene catalog version or part size");
    require(hash_valid(scene.source_hash) && !scene.source.empty() && !scene.resources.empty(),"scene source identity missing");
    require(scene.mesh_count && scene.mesh_count<=1000000 && !scene.primitives.empty() && !scene.instances.empty(),"scene catalog is empty or exceeds mesh limit");
    require(scene.primitives.size()<=1000000 && scene.parts.size()<=1000000 && scene.instances.size()<=1000000,"scene catalog metadata limit exceeded");
    std::uint64_t triangles{};std::size_t next_part{};std::vector<std::uint64_t> mesh_triangles(scene.mesh_count);
    std::set<std::pair<unsigned,unsigned>> primitive_ids;
    for(unsigned index=0;index<scene.primitives.size();++index) {
      const auto& primitive=scene.primitives[index];
      require(primitive.mesh<scene.mesh_count && (primitive.material==invalid_id || primitive.material<scene.material_count),"scene primitive reference invalid");
      require(primitive_ids.emplace(primitive.mesh,primitive.primitive).second,"duplicate scene primitive");
      require(primitive.triangles && primitive.vertices && primitive.first_part==next_part && primitive.part_count,"scene primitive part range invalid");
      require(primitive.part_count<=scene.parts.size()-next_part,"scene part range exceeds catalog");
      if(!primitive.triangle_order.empty()) {
        const auto path=std::filesystem::path(primitive.triangle_order);
        require(path.is_relative() && !path.has_root_name() && primitive.triangle_order.find("..")==std::string::npos &&
            hash_valid(primitive.triangle_order_hash),"invalid scene triangle order identity");
      } else require(primitive.triangle_order_hash.empty(),"source-order primitive contains a permutation digest");
      bounds_valid(primitive.bounds);std::uint64_t first{};
      for(unsigned part=0;part<primitive.part_count;++part) {
        const auto& value=scene.parts[next_part++];bounds_valid(value.bounds);
        require(value.primitive==index && value.first_triangle==first && value.triangle_count && value.triangle_count<=scene.part_triangles,
            "scene part has missing, duplicated or overlapping triangle coverage");
        first+=value.triangle_count;
        if(!value.geometry.empty()) {
          const auto path=std::filesystem::path(value.geometry);
          require(path.is_relative() && !path.has_root_name() && value.geometry.find("..") == std::string::npos,"invalid scene geometry path");
          require(hash_valid(value.hash) && value.pages && value.clusters && value.root_pages && value.root_pages<=value.pages,"cooked scene part metadata invalid");
        } else require(value.hash.empty() && !value.pages && !value.clusters && !value.root_pages,"uncooked scene part contains cooked metadata");
      }
      require(first==primitive.triangles,"scene primitive triangle coverage incomplete");
      triangles+=first;mesh_triangles[primitive.mesh]+=first;
    }
    require(next_part==scene.parts.size() && triangles==scene.unique_triangles,"scene source geometry coverage incomplete");
    std::set<unsigned> nodes;triangles=0;
    for(const auto& instance:scene.instances) {
      require(instance.mesh<scene.mesh_count && nodes.insert(instance.node).second,"invalid or repeated scene instance");
      bounds_valid(instance.bounds);
      for(float value:instance.transform)require(std::isfinite(value),"nonfinite scene transform");
      const auto& m=instance.transform;
      const double determinant=double(m[0])*(double(m[5])*m[10]-double(m[6])*m[9])-
          double(m[4])*(double(m[1])*m[10]-double(m[2])*m[9])+double(m[8])*(double(m[1])*m[6]-double(m[2])*m[5]);
      require(std::isfinite(determinant) && std::abs(determinant)>1e-20 && m[3]==0 && m[7]==0 && m[11]==0 && m[15]==1,
          "scene instance requires an invertible affine transform");
      triangles+=mesh_triangles[instance.mesh];
    }
    require(triangles==scene.instanced_triangles,"scene instance coverage incomplete");
    std::set<std::string> resources;std::string identity;
    for(const auto& resource:scene.resources) {
      require(!resource.path.empty() && hash_valid(resource.hash) && resources.insert(resource.path).second,"invalid or duplicate scene resource identity");
      identity+=resource.hash;
    }
    require(resources.contains(scene.source),"scene source resource is absent");
    require(map_texture_digest({reinterpret_cast<const std::uint8_t*>(identity.data()),identity.size()})==scene.source_hash,
        "scene source identity does not match its resource digests");
    error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool read_scene_catalog(const std::filesystem::path& path,SceneCatalog& output,std::string& error) {
  try {
    const auto size=std::filesystem::file_size(content::file_io_path(path));require(size>0 && size<=256ull*1024*1024,"scene catalog file size invalid");
    std::ifstream file(content::file_io_path(path),std::ios::binary);std::string text(size,'\0');
    require(bool(file.read(text.data(),std::streamsize(size))),"scene catalog read failed");
    SceneCatalog next;require(!glz::read_json(next,text),"scene catalog JSON invalid");
    if(!validate_scene_catalog(next,error))return false;
    output=std::move(next);return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool write_scene_catalog(const std::filesystem::path& path,const SceneCatalog& scene,std::string& error) {
  try {
    if(!validate_scene_catalog(scene,error))return false;
    const auto target=content::file_io_path(content::canonical_file_path(path));
    for(const auto& resource:scene.resources) {
      const auto resource_path=content::file_io_path(std::filesystem::path(reinterpret_cast<const char8_t*>(resource.path.c_str())));
      std::error_code ignored;
      require(target!=std::filesystem::weakly_canonical(resource_path) && !std::filesystem::equivalent(target,resource_path,ignored),
          "scene catalog output must not replace a source resource");
    }
    std::string text;require(!glz::write_json(scene,text),"scene catalog serialization failed");
    if(!path.parent_path().empty())std::filesystem::create_directories(content::file_io_path(path.parent_path()));
    static std::atomic<std::uint64_t> sequence{};auto temporary=path;
    temporary+=".tmp-"+std::to_string(std::random_device{}())+"-"+std::to_string(++sequence);
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}} cleanup{content::file_io_path(temporary)};
    std::ofstream file(content::file_io_path(temporary),std::ios::binary|std::ios::trunc);file.write(text.data(),std::streamsize(text.size()));file.close();
    require(bool(file),"scene catalog write failed");
#ifdef _WIN32
    require(MoveFileExW(content::file_io_path(temporary).c_str(),content::file_io_path(path).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"scene catalog atomic publication failed");
#else
    std::filesystem::rename(temporary,path);
#endif
    error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
