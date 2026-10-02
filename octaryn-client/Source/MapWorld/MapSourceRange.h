#pragma once
#include "GltfSourceRange.h"
#include "MapModel.h"
namespace octaryn::client::rendering {
inline std::vector<std::byte> read_map_source_range(const std::filesystem::path& path,const MapLoadLimits& limits) {
  return assets::read_gltf_source_range(path,limits.source_offset,limits.source_length,limits.source_bytes,limits.cancel);
}
}
