#include "MapTextureCache.h"
#include <array>
#include <bit>
#include <cstdio>
#include <fstream>
#include <algorithm>

namespace octaryn::client::rendering {
namespace {
constexpr std::array<std::uint32_t,64> constants{
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
template<class Read> std::string digest(size_t length,Read read) {
  std::array<std::uint32_t,8> hash{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
      0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  const auto padded=(length+9+63)/64*64;
  const auto bits=static_cast<std::uint64_t>(length)*8;
  for(size_t offset=0;offset<padded;offset+=64) {
    std::array<std::uint32_t,64> w{};
    std::array<std::uint8_t,64> block{};
    if(offset<length)read(offset,block.data(),std::min<size_t>(64,length-offset));
    for(unsigned i=0;i<64;++i) {
      const auto pos=offset+i;std::uint8_t value{};
      if(pos<length)value=block[i];else if(pos==length)value=0x80;
      else if(pos>=padded-8)value=static_cast<std::uint8_t>(bits>>((padded-1-pos)*8));
      w[i/4]|=static_cast<std::uint32_t>(value)<<(24-(i%4)*8);
    }
    for(unsigned i=16;i<64;++i) {
      const auto a=w[i-15],b=w[i-2];
      w[i]=w[i-16]+(std::rotr(a,7)^std::rotr(a,18)^(a>>3))+w[i-7]+(std::rotr(b,17)^std::rotr(b,19)^(b>>10));
    }
    auto state=hash;
    for(unsigned i=0;i<64;++i) {
      const auto a=state[0],b=state[1],c=state[2],e=state[4],f=state[5],g=state[6];
      const auto t1=state[7]+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^(~e&g))+constants[i]+w[i];
      const auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));
      for(unsigned j=7;j>0;--j)state[j]=state[j-1];
      state[4]+=t1;state[0]=t1+t2;
    }
    for(unsigned i=0;i<8;++i)hash[i]+=state[i];
  }
  std::string text;char word[9]{};
  for(auto value:hash) {std::snprintf(word,sizeof(word),"%08x",value);text+=word;}
  return text;
}
}
std::string map_texture_digest(std::span<const std::uint8_t> input) {
  return digest(input.size(),[&](size_t offset,std::uint8_t* block,size_t count) {std::copy_n(input.data()+offset,count,block);});
}
std::string map_texture_file_digest(const std::filesystem::path& path,std::string& error) {
  std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
  if(ec || size>512u*1024*1024) {error="hash input unreadable or exceeds 512MiB";return {};}
  std::ifstream file(path,std::ios::binary);
  const auto hash=digest(static_cast<size_t>(size),[&](size_t,std::uint8_t* block,size_t count) {
    file.read(reinterpret_cast<char*>(block),static_cast<std::streamsize>(count));
  });
  if(!file) {error="hash input read failed";return {};}
  return hash;
}
std::string map_texture_cache_key(const MapModelImage& image,const MapMipOptions& options) {
  // Version covers filtering, encoder settings, quality gates and DDS contract.
  std::vector<std::uint8_t> bytes;
  const auto append=[&](std::uint32_t value) {for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(i*8)));};
  append(0x5a534754);append(map_texture_cache_version);append(static_cast<unsigned>(options.role));
  append(options.alpha_weighted?1:0);append(options.preserve_coverage?1:0);
  append(std::bit_cast<std::uint32_t>(options.alpha_cutoff));append(std::bit_cast<std::uint32_t>(options.alpha_factor));
  bytes.insert(bytes.end(),image.bytes.begin(),image.bytes.end());
  return map_texture_digest(bytes);
}
}
