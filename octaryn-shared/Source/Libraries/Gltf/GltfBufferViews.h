#pragma once
#include <fastgltf/types.hpp>
#include <atomic>
#include <filesystem>
#include <map>
#include <span>
#include <vector>

namespace octaryn::assets {
// fastgltf accessor tools assume these byte ranges were checked by the caller.
void validate_gltf_accessors(const fastgltf::Asset&);
// Reads only the requested ranges; an external buffer may exceed addressable residency.
class GltfBufferViews {
public:
  GltfBufferViews(std::filesystem::path parent,std::size_t budget,const std::atomic_bool* cancel);
  fastgltf::span<const std::byte> operator()(const fastgltf::Asset&,std::size_t view) const;
  void clear();
private:
  std::vector<std::byte> read(const fastgltf::Asset&,std::size_t buffer,std::size_t offset,std::size_t count) const;
  std::filesystem::path parent_;
  std::size_t budget_;
  const std::atomic_bool* cancel_;
  mutable std::map<std::size_t,std::vector<std::byte>> views_;
  mutable std::size_t retained_{};
};
}
