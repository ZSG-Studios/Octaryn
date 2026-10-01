#include "SceneHierarchyInternal.h"
#include "SceneMaterialJson.h"
#include "ResourceDigest.h"
#include "FilePath.h"
#include <atomic>
#include <fstream>
#include <random>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace octaryn::client::rendering::virtual_geometry {
namespace {
constexpr std::uint64_t summary_limit=10ull*1024*1024,shard_limit=16ull*1024*1024;
void require(bool value,const char* error) {if(!value)throw std::runtime_error(error);}
template<class T> bool read(const std::filesystem::path& path,T& output,std::uint64_t limit,std::string& error) {
  try {
    const auto size=std::filesystem::file_size(content::file_io_path(path));require(size && size<=limit,"hierarchy metadata size exceeds bounded limit");
    std::ifstream file(content::file_io_path(path),std::ios::binary);std::string text(size,'\0');
    require(bool(file.read(text.data(),std::streamsize(size))),"hierarchy metadata read failed");
    T next;require(!glz::read_json(next,text),"hierarchy metadata JSON invalid");output=std::move(next);return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
template<class T> bool write(const std::filesystem::path& path,const T& value,std::uint64_t limit,std::string& error) {
  try {
    std::string text;require(!glz::write_json(value,text) && text.size()<=limit,"hierarchy metadata serialization exceeds limit");
    std::filesystem::create_directories(content::file_io_path(path.parent_path()));
    static std::atomic<std::uint64_t> sequence{};auto temporary=path;
    temporary+=".tmp-"+std::to_string(std::random_device{}())+"-"+std::to_string(++sequence);
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove(path,e);}} cleanup{content::file_io_path(temporary)};
    std::ofstream file(content::file_io_path(temporary),std::ios::binary|std::ios::trunc);
    file.write(text.data(),std::streamsize(text.size()));file.close();require(bool(file),"hierarchy checkpoint write failed");
#ifdef _WIN32
    require(MoveFileExW(content::file_io_path(temporary).c_str(),content::file_io_path(path).c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"hierarchy atomic checkpoint publication failed");
#else
    std::filesystem::rename(content::file_io_path(temporary),content::file_io_path(path));
#endif
    error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
std::filesystem::path work_file(const std::filesystem::path& path,unsigned primitive) {
  return path.parent_path()/"work"/("primitive-"+std::to_string(primitive)+".json");
}
}
bool scene_hierarchy_path(const std::filesystem::path& package,const std::string& relative,std::filesystem::path& output,std::string& error) {
  try {
    const auto name=preparation_path(relative);
    require(!name.empty() && !name.is_absolute() && !name.has_root_name(),"hierarchy resource must be package relative");
    for(const auto& component:name)require(component!="..","hierarchy resource escapes package");
    const auto root=content::canonical_file_path(package.parent_path()),path=content::canonical_file_path(root/name);
    const auto local=path.lexically_relative(root);require(!local.empty(),"hierarchy resource path invalid");
    for(const auto& component:local)require(component!="..","hierarchy resource follows an escaping link");
    output=path;error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool read_scene_hierarchy(const std::filesystem::path& path,SceneHierarchy& output,std::string& error) {
  SceneHierarchy next;if(!read(path,next,summary_limit,error) || !validate_scene_hierarchy(next,error))return false;
  output=std::move(next);return true;
}
bool write_scene_hierarchy(const std::filesystem::path& path,const SceneHierarchy& h,std::string& error) {
  if(!validate_scene_hierarchy(h,error))return false;
  try {
    const auto target=content::canonical_file_path(path);
    require(target!=content::canonical_file_path(preparation_path(h.catalog)),"hierarchy cannot overwrite canonical catalog");
    for(const auto& resource:h.resources) {
      const auto source=content::canonical_file_path(preparation_path(resource.path));std::error_code ec;
      require(target!=source && !std::filesystem::equivalent(content::file_io_path(target),content::file_io_path(source),ec),
          "hierarchy cannot overwrite original source");
    }
    return write(path,h,summary_limit,error);
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool read_scene_hierarchy_shard(const std::filesystem::path& package,const SceneHierarchy& h,
    std::uint32_t primitive,SceneHierarchyShard& output,std::string& error) {
  if(primitive>=h.primitives.size()) {error="hierarchy primitive out of range";return false;}
  const auto& p=h.primitives[primitive];std::filesystem::path path;
  if(!p.complete || !scene_hierarchy_path(package,p.shard,path,error)) {if(error.empty())error="hierarchy primitive is pending";return false;}
  if(content::resource_file_digest(path,error,shard_limit)!=p.shard_hash) {error="hierarchy shard checksum differs";return false;}
  SceneHierarchyShard shard;
  if(!read(path,shard,shard_limit,error) || !validate_scene_hierarchy_shard(h,shard,error))return false;
  if(!shard.complete || shard.roots.size()!=p.roots.size()) {error="hierarchy root summary differs";return false;}
  for(std::size_t i=0;i<p.roots.size();++i) {
    std::string a,b;
    if(glz::write_json(p.roots[i],a) || glz::write_json(shard.nodes.at(shard.roots[i]),b) || a!=b) {error="hierarchy root summary does not match shard";return false;}
  }
  output=std::move(shard);error.clear();return true;
}
bool read_hierarchy_work(const std::filesystem::path& package,const SceneHierarchy& h,
    std::uint32_t primitive,SceneHierarchyShard& output,std::string& error) {
  if(!read(work_file(package,primitive),output,shard_limit,error))return false;
  return validate_scene_hierarchy_shard(h,output,error) && output.primitive==primitive;
}
bool write_hierarchy_work(const std::filesystem::path& path,const SceneHierarchy& h,const SceneHierarchyShard& shard,std::string& error) {
  return validate_scene_hierarchy_shard(h,shard,error) && write(work_file(path,shard.primitive),shard,shard_limit,error);
}
bool write_scene_hierarchy_shard(const std::filesystem::path& package,SceneHierarchy& h,SceneHierarchyShard& shard,std::string& error) {
  if(!shard.complete || !validate_scene_hierarchy_shard(h,shard,error))return false;
  std::string text;if(glz::write_json(shard,text)) {error="hierarchy shard serialization failed";return false;}
  const auto digest=hierarchy_digest(text);const auto name="shards/"+digest+".json";
  if(!write(package.parent_path()/name,shard,shard_limit,error))return false;
  auto& primitive=h.primitives.at(shard.primitive);primitive.shard=name;primitive.shard_hash=digest;return true;
}
}
