#pragma once
#include "GltfBufferViews.h"
#include <fastgltf/core.hpp>

namespace octaryn::assets {
// Offline cook scratch. A view is decoded once and mapped until the mesh changes.
class GltfMappedViews {
public:
  GltfMappedViews(std::filesystem::path source_parent,const std::filesystem::path& scratch,
      std::size_t budget,const std::atomic_bool* cancel=nullptr);
  ~GltfMappedViews();
  GltfMappedViews(const GltfMappedViews&)=delete;
  GltfMappedViews& operator=(const GltfMappedViews&)=delete;
  fastgltf::span<const std::byte> operator()(const fastgltf::Asset&,std::size_t view) const;
  void clear();
private:
  std::filesystem::path parent_,directory_;
  std::size_t budget_;
  const std::atomic_bool* cancel_;
  mutable std::map<std::size_t,fastgltf::MappedGltfFile> views_;
  mutable std::vector<std::filesystem::path> files_;
};
}
