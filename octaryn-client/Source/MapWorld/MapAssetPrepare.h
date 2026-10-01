#pragma once
#include "MapTextureCache.h"
#include "MapLodCache.h"
#include "MapMeshlets.h"
#include "MapTextureReuse.h"
#include "MapForwardGeometry.h"
#include "../VirtualGeometry/MapGeometryCache.h"
#include <array>
#include <atomic>
namespace octaryn::client::rendering {
struct PreparedMapTexture {
  std::string key;MapCachedTexture texture;bool cached{};
  std::shared_ptr<const MapTextureLease> resident;
};
struct PreparedMapImages {
  std::vector<PreparedMapTexture> textures;
  std::vector<std::array<size_t,5>> slots;
  unsigned variants{},shared{},decoded{},promoted{};
  unsigned resident_reuses{};
  std::uint64_t avoided_payload_bytes{};
};
struct PreparedMapAsset {
  std::filesystem::path source;
  std::filesystem::path texture_cache;
  MapModel model;
  MapForwardGeometry forward;
  virtual_geometry::MapGeometryCache geometry_cache;
  PreparedMapImages images;
  MapLodData lods;
  MapMeshletData meshlets;
  float lod_pixels{};
};
// Workers retain opaque tickets only; no GPU interfaces, allocation, or submission.
bool prepare_map_images(MapModel&,const std::filesystem::path& cache,PreparedMapImages&,std::string& error,
    const std::atomic_bool* cancel=nullptr,std::uint64_t budget=256u*1024*1024,bool cooked_required=false,
    const MapTextureReuseIndex* reuse=nullptr);
bool prepare_map_asset(const std::filesystem::path&,PreparedMapAsset&,std::string& error,const std::atomic_bool* cancel=nullptr,
    const std::filesystem::path& shared_cache={},const MapTextureReuseIndex* reuse=nullptr);
}
