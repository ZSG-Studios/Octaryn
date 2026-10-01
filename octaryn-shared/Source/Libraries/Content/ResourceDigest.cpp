#include "ResourceDigest.h"
#include "FilePath.h"
#include "WindowsResourceHash.h"
#include <vector>
#include <array>
#include <bit>
#include <cstdio>
#include <fstream>
#include <algorithm>

namespace octaryn::content {
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
class PortableHash {
  std::array<std::uint32_t,8> hash_{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
      0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  std::array<std::uint8_t,64> pending_{};
  std::uint64_t length_{};
  size_t used_{};
  void block(const std::uint8_t* bytes) {
    std::array<std::uint32_t,64> w{};
    for(unsigned i=0;i<64;++i)w[i/4]|=std::uint32_t(bytes[i])<<(24-(i%4)*8);
    for(unsigned i=16;i<64;++i) {
      const auto a=w[i-15],b=w[i-2];
      w[i]=w[i-16]+(std::rotr(a,7)^std::rotr(a,18)^(a>>3))+w[i-7]+(std::rotr(b,17)^std::rotr(b,19)^(b>>10));
    }
    auto state=hash_;
    for(unsigned i=0;i<64;++i) {
      const auto a=state[0],b=state[1],c=state[2],e=state[4],f=state[5],g=state[6];
      const auto t1=state[7]+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^(~e&g))+constants[i]+w[i];
      const auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));
      for(unsigned j=7;j>0;--j)state[j]=state[j-1];
      state[4]+=t1;state[0]=t1+t2;
    }
    for(unsigned i=0;i<8;++i)hash_[i]+=state[i];
  }
public:
  void append(std::span<const std::uint8_t> bytes) {
    length_+=bytes.size();
    while(!bytes.empty()) {
      if(!used_ && bytes.size()>=64) {block(bytes.data());bytes=bytes.subspan(64);continue;}
      const auto count=std::min(bytes.size(),64-used_);
      std::copy_n(bytes.data(),count,pending_.data()+used_);used_+=count;bytes=bytes.subspan(count);
      if(used_==64) {block(pending_.data());used_=0;}
    }
  }
  std::string finish() {
    const auto bits=length_*8;
    pending_[used_++]=0x80;
    if(used_>56) {std::fill(pending_.begin()+used_,pending_.end(),0);block(pending_.data());used_=0;}
    std::fill(pending_.begin()+used_,pending_.end(),0);
    for(unsigned i=0;i<8;++i)pending_[63-i]=static_cast<std::uint8_t>(bits>>(i*8));
    block(pending_.data());
    std::string text;char word[9]{};
    for(auto value:hash_) {std::snprintf(word,sizeof(word),"%08x",value);text+=word;}
    return text;
  }
};
}
std::string resource_digest_parts(std::span<const std::span<const std::uint8_t>> parts) {
#if defined(_WIN32) && !defined(OCTARYN_RESOURCE_HASH_PORTABLE_TEST)
  PlatformResourceHash accelerated;
  bool valid=bool(accelerated);
  for(const auto part:parts)if(valid)valid=accelerated.append(part);
  if(valid) {const auto hash=accelerated.finish();if(!hash.empty())return hash;}
#endif
  PortableHash hash;for(const auto part:parts)hash.append(part);return hash.finish();
}
std::string resource_digest(std::span<const std::uint8_t> input) {
  return resource_digest_parts(std::span(&input,1));
}
std::string resource_file_digest(const std::filesystem::path& path,std::string& error,
    std::uint64_t maximum_bytes,const std::atomic_bool* cancel) {
  error.clear();std::error_code ec;const auto io=file_io_path(path);const auto size=std::filesystem::file_size(io,ec);
  if(ec || size>maximum_bytes) {error="hash input unreadable or exceeds caller size limit";return {};}
  const auto modified=std::filesystem::last_write_time(io,ec);
  if(ec) {error="hash input metadata unavailable";return {};}
  if(cancel && cancel->load(std::memory_order_relaxed)) {error="hash operation canceled";return {};}
  std::ifstream file(file_io_path(path),std::ios::binary);
#if defined(_WIN32) && !defined(OCTARYN_RESOURCE_HASH_PORTABLE_TEST)
  PlatformResourceHash accelerated;
#endif
  PortableHash portable;std::vector<std::uint8_t> buffer(1u<<20);std::uint64_t consumed{};
  while(consumed<size) {
    if(cancel && cancel->load(std::memory_order_relaxed)) {error="hash operation canceled";return {};}
    const auto count=static_cast<size_t>(std::min<std::uint64_t>(buffer.size(),size-consumed));
    if(!file.read(reinterpret_cast<char*>(buffer.data()),static_cast<std::streamsize>(count))) {
      error="hash input read failed";return {};
    }
    const auto bytes=std::span(buffer.data(),count);
#if defined(_WIN32) && !defined(OCTARYN_RESOURCE_HASH_PORTABLE_TEST)
    if(accelerated) {
      if(!accelerated.append(bytes)) {error="platform SHA256 update failed";return {};}
    } else
#endif
    portable.append(bytes);
    consumed+=count;
  }
  if(!file || file.peek()!=std::ifstream::traits_type::eof()) {error="hash input size changed or unreadable";return {};}
  if(std::filesystem::last_write_time(io,ec)!=modified || ec || std::filesystem::file_size(io,ec)!=size || ec) {
    error="hash input changed while reading";return {};
  }
#if defined(_WIN32) && !defined(OCTARYN_RESOURCE_HASH_PORTABLE_TEST)
  if(accelerated) {auto hash=accelerated.finish();if(hash.empty())error="platform SHA256 finish failed";return hash;}
#endif
  return portable.finish();
}
}
