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
  std::array<float,4> axis_v_type{0,0,.5f,0}; // 0 point, 1 spot, 2 rectangle
};
static_assert(sizeof(WorldLocalLight)==80);
struct WorldRenderer;
bool open_world_renderer_set_lights(WorldRenderer*,const WorldLocalLight*,std::uint32_t count);
}
