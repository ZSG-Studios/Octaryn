#include "MapTextureCache.h"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace octaryn::client::rendering;
namespace {
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
std::vector<std::uint8_t> data(size_t length) {
  std::vector<std::uint8_t> bytes(length);
  for(size_t i=0;i<length;++i)bytes[i]=std::uint8_t(i*37+11);
  return bytes;
}
void vectors(const std::filesystem::path& root) {
  std::ifstream input(root/"vectors.txt");size_t length;std::string expected;
  unsigned cases{};
  while(input>>length>>expected) {
    const auto bytes=data(length);const std::span<const std::uint8_t> all(bytes);
    require(map_texture_digest(all)==expected,"contiguous known digest");
    for(size_t split=0;split<=(length>257?2:length);++split) {
      const std::array<std::span<const std::uint8_t>,4> parts{all.first(split),{},all.subspan(split),{}};
      require(map_texture_digest_parts(parts)==expected,"split digest");
    }
    std::vector<std::span<const std::uint8_t>> parts;
    for(size_t offset=0;offset<length;) {
      const auto count=std::min<size_t>((offset%73)+1,length-offset);
      parts.push_back(all.subspan(offset,count));offset+=count;
    }
    require(map_texture_digest_parts(parts)==expected,"uneven multipart digest");
    std::string error;
    require(map_texture_file_digest(root/(std::to_string(length)+".bin"),error)==expected && error.empty(),"streamed file digest");
    ++cases;
  }
  require(cases>=14,"missing known vectors");
  require(map_texture_digest_parts({})==map_texture_digest({}),"empty parts");
  std::ifstream keys(root/"keys.txt");unsigned role,weighted,coverage;float cutoff,factor;cases=0;
  while(keys>>role>>weighted>>coverage>>cutoff>>factor>>expected) {
    MapMipOptions options{MapMipRole(role),weighted!=0,coverage!=0,cutoff,factor};
    MapModelImage image;image.bytes=data(257);
    require(map_texture_cache_key(image,options)==expected,"cache key identity");++cases;
  }
  require(cases==5,"missing key vectors");
}
void cache(const std::filesystem::path& path) {
  std::ifstream source(path,std::ios::binary);std::vector<std::uint8_t> bytes(
      (std::istreambuf_iterator<char>(source)),std::istreambuf_iterator<char>());
  require(bytes.size()>148,"fixture truncated");
  const bool srgb=bytes[128]==99 || bytes[128]==29;
  auto receipt=path;receipt+=".sha256";std::ifstream receipt_file(receipt);std::string expected;receipt_file>>expected;
  require(map_texture_digest(bytes)==expected,"existing asset receipt identity");
  MapCachedTexture texture;std::string error;
  require(read_map_texture_cache(path,0,0,srgb,texture,error,bytes.size()-149)==MapCacheResult::Invalid &&
      error.find("remaining preparation budget")!=std::string::npos && texture.levels.empty(),"preallocation payload bound");
  require(read_map_texture_cache(path,0,0,srgb,texture,error)==MapCacheResult::Ready,"existing asset load");
  auto roundtrip=path;roundtrip+=".roundtrip.dds";
  require(write_map_texture_cache(roundtrip,texture,error),"roundtrip write");
  require(map_texture_file_digest(roundtrip,error)==expected,"writer exact original bytes");
  bytes.back()^=1;
  std::ofstream corrupt(path,std::ios::binary|std::ios::trunc);
  corrupt.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());corrupt.close();
  require(read_map_texture_cache(path,0,0,srgb,texture,error)==MapCacheResult::Invalid,"corruption rejected");
}
}
int main(int argc,char** argv) {
  try {
    require(argc==2,"expected fixture directory");const std::filesystem::path root(argv[1]);
    vectors(root);cache(root/"rgba.dds");cache(root/"bc7.dds");
    std::cout<<"map_hash_tests passed=1 vectors=14 keys=5 cached_assets=2 corruption_rejections=2\n";
  } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

