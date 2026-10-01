#include "ResourceDigest.h"
#include "FilePath.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace octaryn::content {
std::string resource_tree_digest(const std::filesystem::path& path,std::string& error,
    std::uint64_t maximum_bytes,const std::atomic_bool* cancel) {
  try {
    if(cancel && cancel->load(std::memory_order_relaxed))throw std::runtime_error("scene resource hashing canceled");
    const auto io=file_io_path(path);const auto size=std::filesystem::file_size(io);const auto modified=std::filesystem::last_write_time(io);
    if(size>maximum_bytes)throw std::runtime_error("scene resource exceeds caller source limit");
    std::ifstream input(file_io_path(path),std::ios::binary);if(!input)throw std::runtime_error("scene resource cannot be opened");
    std::vector<std::uint8_t> block(1u<<20);std::string leaves="scene-source-sha256-tree-v1:"+std::to_string(size)+":";
    for(std::uint64_t offset=0;offset<size;) {
      if(cancel && cancel->load(std::memory_order_relaxed))throw std::runtime_error("scene resource hashing canceled");
      const auto count=std::min<std::uint64_t>(block.size(),size-offset);
      if(!input.read(reinterpret_cast<char*>(block.data()),std::streamsize(count)))throw std::runtime_error("scene source read failed");
      leaves+=resource_digest(std::span(block.data(),std::size_t(count)));offset+=count;
    }
    if(input.peek()!=std::ifstream::traits_type::eof() || std::filesystem::last_write_time(io)!=modified ||
        std::filesystem::file_size(io)!=size)throw std::runtime_error("scene resource changed while hashing");
    error.clear();return resource_digest({reinterpret_cast<const std::uint8_t*>(leaves.data()),leaves.size()});
  }catch(const std::exception& failure) {error=failure.what();return {};}
}
}
