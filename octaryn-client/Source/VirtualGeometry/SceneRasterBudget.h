#pragma once
#include <cstdint>

namespace octaryn::client::rendering::virtual_geometry {
struct SceneRasterCapacity {
  std::uint64_t clusters{},pages{},instances{},draws{};
};
inline std::uint64_t scene_raster_bytes(SceneRasterCapacity c) {
  constexpr std::uint64_t maximum=(UINT32_MAX/2-1)/128;
  if(!c.clusters || !c.pages || !c.instances || !c.draws || c.clusters>maximum || c.pages>1048576 ||
      c.instances>1048576 || c.draws>maximum)return UINT64_MAX;
  // Every nonempty domain has at least one cluster; this also bounds its 48-byte record.
  return 2*(c.clusters*(56+48)+c.pages*16+c.instances*112+c.draws*48+72);
}
}
