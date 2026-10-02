#pragma once
#include "FilePath.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace octaryn::assets {
inline std::vector<std::byte> read_gltf_source_range(const std::filesystem::path& path,
    std::uint64_t offset,std::uint64_t length,std::uint64_t maximum=64ull*1024*1024,
    const std::atomic_bool* cancel=nullptr) {
  const auto fail=[](const char* error){throw std::runtime_error(error);};
  const auto size=std::filesystem::file_size(content::file_io_path(path));
  if(length<20 || length>64ull*1024*1024 || length>maximum || offset>size || length>size-offset ||
      offset>std::uint64_t(std::numeric_limits<std::streamoff>::max()))fail("packed GLB range exceeds bounded source admission");
  std::ifstream input(content::file_io_path(path),std::ios::binary);
  if(!input || !input.seekg(static_cast<std::streamoff>(offset)))fail("packed GLB source cannot be opened");
  std::vector<std::byte> bytes(static_cast<std::size_t>(length));
  for(std::size_t at=0;at<bytes.size();) {
    if(cancel && cancel->load(std::memory_order_relaxed))fail("GLB preparation cancelled");
    const auto count=std::min<std::size_t>(1024*1024,bytes.size()-at);
    if(!input.read(reinterpret_cast<char*>(bytes.data()+at),static_cast<std::streamsize>(count)))fail("packed GLB range truncated");
    at+=count;
  }
  const auto u32=[&](unsigned at) {
    std::uint32_t value{};
    for(unsigned byte=0;byte<4;++byte)value|=std::uint32_t(std::to_integer<unsigned char>(bytes[at+byte]))<<(8*byte);
    return value;
  };
  if(u32(0)!=0x46546c67 || u32(4)!=2 || u32(8)!=length)fail("packed range must contain exactly one GLB version 2");
  return bytes;
}
}
