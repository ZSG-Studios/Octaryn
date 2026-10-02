#pragma once
#include "FilePath.h"
#include <glaze/glaze.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace octaryn::content {
struct TileCatalogArrays {
  int version{1};
  std::vector<std::array<float,6>> tiles;
  std::vector<std::string> tile_files,tile_ids;
  std::vector<bool> tile_collision;
  std::vector<std::array<std::uint64_t,2>> tile_ranges;
};
inline bool tile_catalog_confined(const std::filesystem::path& root,const std::string& value,
    std::filesystem::path& result) {
  if(value.empty() || value.size()>1024 || value.find('\0')!=std::string::npos || value.find(':')!=std::string::npos)return false;
  const auto relative=std::filesystem::u8path(value);
  if(relative.is_absolute() || relative.has_root_name())return false;
  for(const auto& part:relative)if(part==".." || part==".")return false;
  std::error_code error;
  result=std::filesystem::canonical(file_io_path(root/relative),error);
  if(error || !std::filesystem::is_regular_file(result))return false;
  const auto parent=std::filesystem::canonical(file_io_path(root),error);
  if(error)return false;
  const auto resolved=result.lexically_relative(parent);
  if(resolved.empty() || resolved.is_absolute())return false;
  for(const auto& part:resolved)if(part=="..")return false;
  return true;
}
inline bool append_tile_catalog(TileCatalogArrays& target,TileCatalogArrays&& source) {
  const auto count=source.tiles.size(),first=target.tiles.size();
  if(source.version!=1 || count>65536-first || source.tile_files.size()!=count ||
      (!source.tile_ids.empty() && source.tile_ids.size()!=count) ||
      (!source.tile_collision.empty() && source.tile_collision.size()!=count) ||
      (!source.tile_ranges.empty() && source.tile_ranges.size()!=count))return false;
  if(source.tile_ids.empty())for(std::size_t i=0;i<count;++i)source.tile_ids.push_back("tile/"+std::to_string(first+i));
  if(source.tile_collision.empty())source.tile_collision.resize(count,true);
  if(source.tile_ranges.empty())source.tile_ranges.resize(count);
  target.tiles.insert(target.tiles.end(),source.tiles.begin(),source.tiles.end());
  for(auto& file:source.tile_files)target.tile_files.push_back(std::move(file));
  for(auto& id:source.tile_ids)target.tile_ids.push_back(std::move(id));
  target.tile_collision.insert(target.tile_collision.end(),source.tile_collision.begin(),source.tile_collision.end());
  target.tile_ranges.insert(target.tile_ranges.end(),source.tile_ranges.begin(),source.tile_ranges.end());
  return true;
}
// Shard names and payload paths are relative to the immutable root manifest,
// independent of the shard's subdirectory. Global tile indices append in order.
inline bool expand_tile_catalogs(const std::filesystem::path& manifest,
    const std::vector<std::string>& names,TileCatalogArrays& arrays) {
  try {
    if(names.size()>256)return false;
    TileCatalogArrays merged;
    if(!append_tile_catalog(merged,std::move(arrays)))return false;
    std::uint64_t bytes{};std::set<std::filesystem::path> unique;
    for(const auto& name:names) {
      std::filesystem::path path;
      if(!tile_catalog_confined(manifest.parent_path(),name,path) || !unique.insert(path).second)return false;
      const auto size=std::filesystem::file_size(path);
      const auto stamp=std::filesystem::last_write_time(path);
      if(!size || size>4ull*1024*1024 || size>16ull*1024*1024-bytes)return false;
      bytes+=size;
      std::ifstream input(path,std::ios::binary);std::string text(static_cast<std::size_t>(size),'\0');
      if(!input.read(text.data(),static_cast<std::streamsize>(size)))return false;
      if(stamp!=std::filesystem::last_write_time(path) || size!=std::filesystem::file_size(path))return false;
      TileCatalogArrays shard;shard.version=0;
      constexpr glz::opts options{.error_on_unknown_keys=true,.error_on_missing_keys=false};
      if(glz::read<options>(shard,text) || shard.tiles.empty() || !append_tile_catalog(merged,std::move(shard)))return false;
    }
    std::set<std::string> ids;std::map<std::string,std::uint64_t> payloadSizes;
    for(std::size_t i=0;i<merged.tiles.size();++i) {
      const auto& id=merged.tile_ids[i];
      if(id.empty() || id.size()>128 || !ids.insert(id).second)return false;
      for(unsigned char c:id)if(c<33 || c>126)return false;
      const auto& bounds=merged.tiles[i];
      for(float value:bounds)if(!std::isfinite(value))return false;
      for(unsigned axis=0;axis<3;++axis)if(bounds[axis]>bounds[axis+3])return false;
      const auto& file=merged.tile_files[i];
      auto found=payloadSizes.find(file);
      if(found==payloadSizes.end()) {
        std::filesystem::path payload;
        if(!tile_catalog_confined(manifest.parent_path(),file,payload))return false;
        found=payloadSizes.emplace(file,std::filesystem::file_size(payload)).first;
      }
      const auto& range=merged.tile_ranges[i];
      if(!range[1]) {if(range[0])return false;continue;}
      if(range[1]<20 || range[1]>64ull*1024*1024 || range[0]>UINT64_MAX-range[1])return false;
      const auto size=found->second;
      if(range[0]>size || range[1]>size-range[0])return false;
    }
    arrays=std::move(merged);return true;
  }catch(...) {return false;}
}
}
