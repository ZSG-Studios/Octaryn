#include "MapGeometryCache.h"
#include "GeometryBudget.h"
#include "GeometryCache.h"
#include "GeometryCook.h"
#include "../MapWorld/MapTextureCache.h"
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <span>
#include <SDL3/SDL.h>
#include <mutex>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
template<class T> std::span<const std::uint8_t> bytes(std::span<const T> input) {
  return {reinterpret_cast<const std::uint8_t*>(input.data()),input.size_bytes()};
}
}
std::uint64_t map_geometry_reservation(const MapGeometryCache& cache) {
  return geometry_raster_reservation(cache.pages,cache.clusters);
}
bool prepare_map_geometry(const std::filesystem::path&,const MapModel& model,MapGeometryCache& output,std::string& error) {
  const auto start=std::chrono::steady_clock::now();
  std::vector<std::array<std::uint32_t,4>> materials;
  for(const auto& primitive:model.primitives)
    materials.push_back({primitive.first_index,primitive.index_count,unsigned(primitive.material.alpha_mode),primitive.material.double_sided?1u:0u});
  const std::array parts{bytes(std::span<const MapVertex>(model.vertices)),
      bytes(std::span<const std::uint32_t>(model.indices)),bytes(std::span<const std::array<std::uint32_t,4>>(materials))};
  MapGeometryCache result;result.hash=map_texture_digest_parts(parts);
  if(result.hash.empty()) {error="virtual geometry content digest failed";return false;}
  std::filesystem::path directory;
  if(const auto* override_path=SDL_getenv("OCTARYN_CLIENT_GEOMETRY_CACHE_PATH");override_path && *override_path)
    directory=std::filesystem::path(reinterpret_cast<const char8_t*>(override_path));
  else {
    char* preferences=SDL_GetPrefPath("ZSGStudios","Octaryn");
    if(!preferences) {error="virtual geometry cache directory unavailable";return false;}
    directory=std::filesystem::path(reinterpret_cast<const char8_t*>(preferences))/"geometry-cache";
    SDL_free(preferences);
  }
  result.path=directory/("v"+std::to_string(geometry_version))/(result.hash+".vgeom");
  static std::array<std::mutex,64> locks;
  std::lock_guard lock(locks[std::hash<std::string>{}(result.hash)%locks.size()]);
  GeometryAsset asset;
  const bool cached=read_geometry_cache(result.path,result.hash,asset,error,false);
  if(!cached) {
    if(!cook_geometry(model,result.hash,asset,error) || !write_geometry_cache(result.path,asset,error))return false;
  }
  if(asset.material_count!=model.primitives.size() || asset.space!=GeometrySpace::World) {
    error="virtual geometry cache has incompatible material table or coordinate space";return false;
  }
  result.pages=unsigned(asset.pages.size());result.clusters=unsigned(asset.clusters.size());
  std::vector<bool> pinned(asset.pages.size());
  for(const auto root:asset.roots) {
    const auto& group=asset.groups[root];
    for(unsigned i=0;i<group.page_count;++i)pinned[asset.group_pages[group.first_page+i]]=true;
  }
  for(bool value:pinned)result.root_pages+=value?1u:0u;
  std::printf("map_geometry_cache status=%s asset=%s clusters=%u pages=%u root_pages=%u ms=%.3f\n",cached?"reused":"cooked",result.hash.c_str(),
      result.clusters,result.pages,result.root_pages,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
  output=std::move(result);error.clear();return true;
}
}
