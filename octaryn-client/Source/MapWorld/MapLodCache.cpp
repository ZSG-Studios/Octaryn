#include "MapLodCache.h"
#include "MapTextureCache.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>

namespace octaryn::client::rendering {
namespace {
static_assert(std::endian::native==std::endian::little);
struct Header {std::uint32_t magic,version,vertices,indices,primitives,lod_indices,reserved[2];char source_hash[64];};
static_assert(sizeof(Header)==96);
bool valid(const MapModel& model,const MapLodData& lods) {
  if(lods.primitives.size()!=model.primitives.size() || lods.indices.size()>model.indices.size()*2)return false;
  std::size_t expected=0;
  for(size_t p=0;p<lods.primitives.size();++p) {
    const auto& primitive=model.primitives[p];
    const auto begin=model.indices.begin()+primitive.first_index,end=begin+primitive.index_count;
    if(begin==end)return false;
    const auto range=std::minmax_element(begin,end);
    for(const auto& level:lods.primitives[p]) {
      if(!level.count) {if(level.first || level.error!=0)return false;continue;}
      if(!std::isfinite(level.error) || level.error<0 || level.count%3 || level.count>=primitive.index_count ||
          level.first!=expected || level.count>lods.indices.size()-expected ||
          primitive.material.alpha_mode!=MapAlphaMode::Opaque)return false;
      for(size_t i=expected;i<expected+level.count;++i)
        if(lods.indices[i]<*range.first || lods.indices[i]>*range.second)return false;
      expected+=level.count;
    }
  }
  return expected==lods.indices.size();
}
}
bool read_map_lods(const std::filesystem::path& path,const std::string& source_hash,const MapModel& model,
    MapLodData& output,std::string& error) {
  output={};error.clear();std::error_code ec;
  const auto size=std::filesystem::file_size(path,ec);
  const auto fail=[&](const char* reason) {error=reason;return false;};
  if(ec || size<sizeof(Header) || size>512u*1024*1024)return fail("LOD cache missing or outside size limit");
  std::ifstream file(path,std::ios::binary);Header header{};
  if(!file.read(reinterpret_cast<char*>(&header),sizeof(header)))return fail("LOD header truncated");
  if(header.magic!=0x444f4c4d || header.version!=map_lod_cache_version || header.reserved[0] || header.reserved[1] ||
      header.vertices!=model.vertices.size() || header.indices!=model.indices.size() || header.primitives!=model.primitives.size() ||
      header.lod_indices>model.indices.size()*2 || std::string(header.source_hash,64)!=source_hash)
    return fail("LOD geometry/source/version mismatch");
  if(size!=sizeof(Header)+std::uint64_t(header.primitives)*sizeof(std::array<MapLodLevel,2>)+std::uint64_t(header.lod_indices)*4)
    return fail("LOD payload size mismatch");
  auto receipt_path=path;receipt_path+=".sha256";
  if(std::filesystem::file_size(receipt_path,ec)!=64 || ec)return fail("LOD checksum missing");
  std::ifstream receipt(receipt_path,std::ios::binary);std::string checksum(64,'\0');receipt.read(checksum.data(),64);
  if(!receipt || map_texture_file_digest(path,error)!=checksum)return fail("LOD checksum mismatch");
  MapLodData result;result.primitives.resize(header.primitives);result.indices.resize(header.lod_indices);
  file.read(reinterpret_cast<char*>(result.primitives.data()),result.primitives.size()*sizeof(result.primitives.front()));
  file.read(reinterpret_cast<char*>(result.indices.data()),result.indices.size()*4);
  if(!file || !valid(model,result))return fail("LOD ranges or attributes invalid");
  output=std::move(result);return true;
}
bool write_map_lods(const std::filesystem::path& path,const std::string& source_hash,const MapModel& model,
    const MapLodData& lods,std::string& error) {
  if(source_hash.size()!=64 || !valid(model,lods)) {error="invalid LOD cook";return false;}
  Header header{0x444f4c4d,map_lod_cache_version,static_cast<unsigned>(model.vertices.size()),
      static_cast<unsigned>(model.indices.size()),static_cast<unsigned>(model.primitives.size()),static_cast<unsigned>(lods.indices.size())};
  std::copy(source_hash.begin(),source_hash.end(),header.source_hash);
  auto temporary=path;temporary+=".tmp";
  std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
  file.write(reinterpret_cast<const char*>(&header),sizeof(header));
  file.write(reinterpret_cast<const char*>(lods.primitives.data()),lods.primitives.size()*sizeof(lods.primitives.front()));
  file.write(reinterpret_cast<const char*>(lods.indices.data()),lods.indices.size()*4);file.close();
  if(!file) {error="LOD write failed";return false;}
  const auto checksum=map_texture_file_digest(temporary,error);if(checksum.empty())return false;
  auto receipt_path=path;receipt_path+=".sha256";auto receipt_temporary=receipt_path;receipt_temporary+=".tmp";
  std::ofstream receipt(receipt_temporary,std::ios::binary|std::ios::trunc);receipt<<checksum;receipt.close();
  if(!receipt) {error="LOD receipt write failed";return false;}
  std::error_code ec;std::filesystem::rename(temporary,path,ec);
  if(!ec)std::filesystem::rename(receipt_temporary,receipt_path,ec);
  if(ec) {error=ec.message();return false;}return true;
}
}
