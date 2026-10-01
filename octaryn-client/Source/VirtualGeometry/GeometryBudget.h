#pragma once
#include <algorithm>
#include <cstdint>

namespace octaryn::client::rendering::virtual_geometry {
inline constexpr std::uint64_t geometry_instance_view_reservation(std::uint64_t nodes) {return nodes*192*2;}
inline constexpr std::uint64_t geometry_raster_reservation(std::uint32_t pages,std::uint32_t clusters) {
  return std::uint64_t(std::min(pages,6144u))*65536+std::uint64_t(clusters)*512+
      std::uint64_t(pages)*32+(1ull<<20);
}
inline constexpr std::uint64_t geometry_ray_reservation(std::uint32_t clusters) {
  return clusters?2*std::clamp<std::uint64_t>(std::uint64_t(clusters)*32768,1ull<<20,512ull<<20):0;
}
inline constexpr std::uint64_t geometry_ray_instance_reservation(std::uint32_t clusters,std::uint64_t nodes) {
  if(!clusters || !nodes)return 0;
  // Four retained TLAS, material-record and upload generations; descriptor ABI is 160 bytes.
  return nodes*((std::uint64_t(clusters)+127)/128)*4*(160+64+256+256)+(256ull<<10);
}
}
