#include "AnimationCache.h"
#include "AnimationSerialization.h"
#include "../VirtualGeometry/GeometryCache.h"
#include "../MapWorld/MapTextureCache.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace octaryn::client::animation {
namespace {
struct Header {
  std::uint32_t magic{0x4147564fu},version{animation_cache_version};
  std::uint64_t bytes{};
  std::array<char,64> source_hash{},geometry_hash{},archive_hash{};
};
static_assert(sizeof(Header)==208);
void require(bool valid,const std::string& error) {if(!valid)throw std::runtime_error(error);}
std::string checksum(const Header& h,std::span<const std::uint8_t> payload) {
  const std::array<std::span<const std::uint8_t>,2> parts{{
      {reinterpret_cast<const std::uint8_t*>(&h),sizeof(h)},payload}};
  return rendering::map_texture_digest_parts(parts);
}
void replace(const std::filesystem::path& source,const std::filesystem::path& target) {
#ifdef _WIN32
  require(MoveFileExW(source.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"animated cache atomic replacement failed");
#else
  std::filesystem::rename(source,target);
#endif
}
std::filesystem::path geometry_path(const std::filesystem::path& path,const std::string& hash) {
  require(hash.size()==64 && std::all_of(hash.begin(),hash.end(),[](char c){return (c>='0'&&c<='9') || (c>='a'&&c<='f');}),
      "animated geometry digest invalid");
  auto result=path;result+="."+hash+".vgeom";return result;
}
}
bool write_animation_cache(const std::filesystem::path& path,const CookedAsset& cooked,std::string& error) {
  try {
    require(validate_cooked_animation(cooked,error),error);const auto payload=encode_animation_payload(cooked);
    auto intermediate=path;intermediate+=".geometry-pending";
    require(rendering::virtual_geometry::write_geometry_cache(intermediate,cooked.geometry,error),error);
    const auto hash=rendering::map_texture_file_digest(intermediate,error);require(!hash.empty(),error);
    const auto geometry=geometry_path(path,hash);
    if(std::filesystem::exists(geometry)) {
      require(rendering::map_texture_file_digest(geometry,error)==hash,"content-addressed animation geometry is corrupt");
      std::filesystem::remove(intermediate);
    }else std::filesystem::rename(intermediate,geometry);
    Header header;header.bytes=payload.size();std::copy(cooked.geometry.source_hash.begin(),cooked.geometry.source_hash.end(),header.source_hash.begin());
    std::copy(hash.begin(),hash.end(),header.geometry_hash.begin());const auto digest=checksum(header,payload);
    std::copy(digest.begin(),digest.end(),header.archive_hash.begin());
    auto temporary=path;temporary+=".tmp";std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
    require(bool(file),"animated cache open failed");file.write(reinterpret_cast<const char*>(&header),sizeof(header));
    file.write(reinterpret_cast<const char*>(payload.data()),std::streamsize(payload.size()));file.close();require(bool(file),"animated cache write failed");
    replace(temporary,path);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool read_animation_cache(const std::filesystem::path& path,CookedAsset& output,std::string& error,
    const std::string& expected_source_hash,bool load_geometry_pages) {
  try {
    const auto size=std::filesystem::file_size(path);require(size>=sizeof(Header) && size<=sizeof(Header)+1024ull*1024*1024,"animated cache size invalid");
    std::ifstream file(path,std::ios::binary);Header header;require(bool(file.read(reinterpret_cast<char*>(&header),sizeof(header))),"animated cache header truncated");
    require(header.magic==0x4147564fu && header.version==animation_cache_version && header.bytes==size-sizeof(header),"animated cache version/length invalid");
    const auto source=std::string(header.source_hash.data(),64);require(expected_source_hash.empty() || source==expected_source_hash,"animated cache source mismatch");
    std::vector<std::uint8_t> payload(size_t(header.bytes));require(bool(file.read(reinterpret_cast<char*>(payload.data()),std::streamsize(payload.size()))),"animated cache payload truncated");
    const auto expected=header.archive_hash;header.archive_hash={};require(checksum(header,payload)==std::string(expected.data(),64),"animated cache checksum mismatch");
    const auto geometry_hash=std::string(header.geometry_hash.data(),64);const auto geometry=geometry_path(path,geometry_hash);
    require(rendering::map_texture_file_digest(geometry,error)==geometry_hash,"animated geometry sidecar checksum mismatch");
    CookedAsset cooked;decode_animation_payload(payload,cooked);
    require(rendering::virtual_geometry::read_geometry_cache(geometry,source,cooked.geometry,error,load_geometry_pages),error);
    require(validate_cooked_animation(cooked,error),error);output=std::move(cooked);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
