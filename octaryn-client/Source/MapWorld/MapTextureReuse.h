#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace octaryn::client::rendering {
struct MapTextureResource;
struct MapTextureReuseMetadata {
  bool srgb{},compressed{},opaque{};
  unsigned width{},height{},mips{};
  std::uint64_t bytes{};
};
// Opaque resource ownership only: preparation workers never call GPU interfaces.
struct MapTextureLease {
  std::shared_ptr<MapTextureResource> resource;
  MapTextureReuseMetadata metadata;
};
struct MapTextureReuseIndex {
  struct Entry {std::weak_ptr<MapTextureResource> resource;MapTextureReuseMetadata metadata;};
  std::filesystem::path cache_directory;
  std::map<std::string,Entry> entries;
  std::shared_ptr<const MapTextureLease> lookup(const std::filesystem::path& cache,const std::string& key) const {
    if(cache!=cache_directory)return {};
    const auto found=entries.find(key);if(found==entries.end())return {};
    auto resource=found->second.resource.lock();if(!resource)return {};
    return std::make_shared<const MapTextureLease>(MapTextureLease{std::move(resource),found->second.metadata});
  }
};
inline std::string map_texture_pool_key(const std::filesystem::path& cache,const std::string& key,bool compressed) {
  return cache.generic_string()+"/"+key+(compressed?"-bc7":"-rgba8");
}
}
