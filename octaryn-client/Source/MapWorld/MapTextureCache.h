#pragma once
#include "MapMipmaps.h"
#include <span>

namespace octaryn::client::rendering {
struct MapCachedMip {
  unsigned width{},height{};
  std::vector<std::uint8_t> blocks;
};
inline constexpr unsigned map_texture_cache_version=3;
struct MapCachedTexture {bool srgb{},compressed{true},opaque{};std::vector<MapCachedMip> levels;};
MapCachedTexture lossless_map_texture_cache(const std::vector<MapDecodedImage>&,bool srgb);
enum class MapCacheResult { Missing,Invalid,Ready };
std::string map_texture_digest(std::span<const std::uint8_t>);
std::string map_texture_digest_parts(std::span<const std::span<const std::uint8_t>>);
std::string map_texture_file_digest(const std::filesystem::path&,std::string& error);
std::string map_texture_cache_key(const MapModelImage&,const MapMipOptions&);
MapCacheResult read_map_texture_cache(const std::filesystem::path&,unsigned width,unsigned height,
    bool srgb,MapCachedTexture&,std::string& error,std::uint64_t payload_budget=UINT64_MAX);
bool write_map_texture_cache(const std::filesystem::path&,const MapCachedTexture&,std::string& error);
}
