#pragma once
#include <cstdint>
namespace octaryn::scene_geometry {
inline constexpr std::uint64_t collision_part_reservation(std::uint64_t triangles) {
  return triangles*(256ull+36ull)+65536;
}
}
