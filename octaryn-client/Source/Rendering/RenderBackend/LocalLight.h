#pragma once
#include <array>
#include <cstdint>
namespace octaryn::client::rendering {
// Point/spot intensity is radiance times square metres; rectangles use radiance.
// Rectangle axes include half extent. Direction points out of the light.
struct WorldLocalLight {
  std::array<float,4> position_range{0,0,0,16};
  std::array<float,4> color_intensity{1,1,1,1};
  std::array<float,4> direction_outer{0,-1,0,.7f};
  std::array<float,4> axis_u_inner{.5f,0,0,.9f};
  std::array<float,4> axis_v_type{0,0,.5f,0}; // 0 point, 1 spot, 2 rectangle, 3 resident voxel point
};
static_assert(sizeof(WorldLocalLight)==80);
// Compare radiometric inputs semantically instead of bytewise.  A producer can
// legitimately publish +0/-0 for a direction or extent; treating those bytes
// as different needlessly bumps the light revision and invalidates history.
inline bool world_local_light_equal(const WorldLocalLight& a,const WorldLocalLight& b) {
  return a.position_range==b.position_range && a.color_intensity==b.color_intensity &&
    a.direction_outer==b.direction_outer && a.axis_u_inner==b.axis_u_inner &&
    a.axis_v_type==b.axis_v_type;
}
struct WorldRenderer;
bool open_world_renderer_set_lights(WorldRenderer*,const WorldLocalLight*,std::uint32_t count);
}
