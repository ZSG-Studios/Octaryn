#include "MapTextureCache.h"
#include <algorithm>
#include <array>
#include <fstream>

namespace octaryn::client::rendering {
namespace {
constexpr unsigned header_bytes=148;
unsigned mip_bytes(unsigned width,unsigned height,bool compressed) {return compressed?((width+3)/4)*((height+3)/4)*16:width*height*4;}
std::uint32_t word(const std::array<std::uint8_t,header_bytes>& header,unsigned offset) {
  return header[offset]|(std::uint32_t(header[offset+1])<<8)|(std::uint32_t(header[offset+2])<<16)|(std::uint32_t(header[offset+3])<<24);
}
void put(std::array<std::uint8_t,header_bytes>& header,unsigned offset,std::uint32_t value) {
  for(unsigned i=0;i<4;++i)header[offset+i]=static_cast<std::uint8_t>(value>>(i*8));
}
}
MapCacheResult read_map_texture_cache(const std::filesystem::path& path,unsigned width,unsigned height,
    bool srgb,MapCachedTexture& output,std::string& error) {
  output={};error.clear();std::error_code ec;
  const auto bytes=std::filesystem::file_size(path,ec);
  if(ec) {
    if(!std::filesystem::exists(path,ec))return MapCacheResult::Missing;
    error="cache file size unavailable";return MapCacheResult::Invalid;
  }
  const auto invalid=[&](const char* reason) {output={};error=reason;return MapCacheResult::Invalid;};
  if(!width || !height || width>4096 || height>4096 || bytes<header_bytes || bytes>86u*1024*1024)
    return invalid("DDS size outside bounded texture limits");
  std::ifstream file(path,std::ios::binary);std::array<std::uint8_t,header_bytes> header{};
  if(!file.read(reinterpret_cast<char*>(header.data()),header.size()))return invalid("DDS header truncated");
  const auto format=word(header,128);const bool compressed=format==98 || format==99;
  if(format!=(srgb?99u:98u) && format!=(srgb?29u:28u))return invalid("DDS unsupported or mismatched format");
  unsigned mip_count=1,w=width,h=height;
  while(w>1 || h>1) {w=std::max(1u,w/2);h=std::max(1u,h/2);++mip_count;}
  if(word(header,0)!=0x20534444 || word(header,4)!=124 || word(header,76)!=32 ||
      word(header,8)!=(compressed?0xa1007u:0x2100fu) || word(header,20)!=(compressed?mip_bytes(width,height,true):width*4) || word(header,108)!=0x401008 ||
      word(header,80)!=4 || word(header,84)!=0x30315844 || word(header,12)!=height || word(header,16)!=width ||
      word(header,24)!=0 || word(header,28)!=mip_count ||
      word(header,132)!=3 || word(header,136)!=0 || word(header,140)!=1 || word(header,112)!=0 || word(header,144)!=1)
    return invalid("DDS format, dimensions, alpha convention or complete mip chain mismatch");
  std::uint64_t expected=header_bytes;w=width;h=height;
  for(unsigned i=0;i<mip_count;++i) {expected+=mip_bytes(w,h,compressed);w=std::max(1u,w/2);h=std::max(1u,h/2);}
  if(bytes!=expected)return invalid("DDS payload size mismatch");
  auto digest_path=path;digest_path+=".sha256";
  if(std::filesystem::file_size(digest_path,ec)!=64 || ec)return invalid("DDS integrity companion missing or invalid");
  std::ifstream digest_file(digest_path,std::ios::binary);std::string digest(64,'\0');
  if(!digest_file.read(digest.data(),digest.size()))return invalid("DDS integrity companion unreadable");
  std::vector<std::uint8_t> digest_input(header.begin(),header.end());digest_input.reserve(static_cast<size_t>(bytes));
  MapCachedTexture texture;texture.srgb=srgb;texture.compressed=compressed;w=width;h=height;
  for(unsigned i=0;i<mip_count;++i) {
    MapCachedMip level{w,h,{}};level.blocks.resize(mip_bytes(w,h,compressed));
    if(!file.read(reinterpret_cast<char*>(level.blocks.data()),level.blocks.size()))return invalid("DDS mip truncated");
    if(compressed)for(size_t block=0;block<level.blocks.size();block+=16)
      if(level.blocks[block]==0)return invalid("DDS invalid BC7 block mode");
    digest_input.insert(digest_input.end(),level.blocks.begin(),level.blocks.end());
    texture.levels.push_back(std::move(level));w=std::max(1u,w/2);h=std::max(1u,h/2);
  }
  if(map_texture_digest(digest_input)!=digest)return invalid("DDS content digest mismatch");
  output=std::move(texture);return MapCacheResult::Ready;
}
bool write_map_texture_cache(const std::filesystem::path& path,const MapCachedTexture& texture,std::string& error) {
  if(texture.levels.empty()) {error="no DDS mip levels";return false;}
  const auto& first=texture.levels.front();unsigned w=first.width,h=first.height;
  if(!w || !h || w>4096 || h>4096) {error="DDS dimensions invalid";return false;}
  for(size_t i=0;i<texture.levels.size();++i) {
    const auto& mip=texture.levels[i];
    if(mip.width!=w || mip.height!=h || mip.blocks.size()!=mip_bytes(w,h,texture.compressed) || (i+1<texture.levels.size() && w==1 && h==1)) {
      error="DDS mip layout invalid";return false;
    }
    if(i+1==texture.levels.size() && (w!=1 || h!=1)) {error="DDS mip chain incomplete";return false;}
    w=std::max(1u,w/2);h=std::max(1u,h/2);
  }
  std::array<std::uint8_t,header_bytes> header{};
  put(header,0,0x20534444);put(header,4,124);put(header,8,texture.compressed?0xa1007:0x2100f);
  put(header,12,first.height);put(header,16,first.width);put(header,20,texture.compressed?mip_bytes(first.width,first.height,true):first.width*4);
  put(header,28,static_cast<unsigned>(texture.levels.size()));put(header,76,32);put(header,80,4);put(header,84,0x30315844);
  put(header,108,0x401008);put(header,128,texture.compressed?(texture.srgb?99:98):(texture.srgb?29:28));
  put(header,132,3);put(header,140,1);put(header,144,1);
  // Write a distinct staging file; never expose a partially written DDS as ready.
  auto temporary=path;temporary+=".tmp";
  std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
  file.write(reinterpret_cast<const char*>(header.data()),header.size());
  std::vector<std::uint8_t> digest_input(header.begin(),header.end());
  for(const auto& mip:texture.levels) {
    file.write(reinterpret_cast<const char*>(mip.blocks.data()),mip.blocks.size());
    digest_input.insert(digest_input.end(),mip.blocks.begin(),mip.blocks.end());
  }
  file.close();if(!file) {error="DDS write failed";return false;}
  const auto digest=map_texture_digest(digest_input);
  auto digest_path=path;digest_path+=".sha256";auto digest_temporary=digest_path;digest_temporary+=".tmp";
  std::ofstream digest_file(digest_temporary,std::ios::binary|std::ios::trunc);
  digest_file.write(digest.data(),digest.size());digest_file.close();
  if(!digest_file) {error="DDS integrity write failed";return false;}
  std::error_code ec;std::filesystem::rename(temporary,path,ec);
  if(ec) {error="DDS publish failed: "+ec.message();return false;}
  std::filesystem::rename(digest_temporary,digest_path,ec);
  if(ec) {error="DDS integrity publish failed: "+ec.message();return false;}
  return true;
}
MapCachedTexture lossless_map_texture_cache(const std::vector<MapDecodedImage>& levels,bool srgb) {
  MapCachedTexture output;output.srgb=srgb;output.compressed=false;
  for(const auto& level:levels)output.levels.push_back({level.width,level.height,level.rgba});
  return output;
}
}
