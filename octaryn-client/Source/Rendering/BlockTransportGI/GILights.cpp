#include "GILights.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace octaryn::client::rendering {
namespace {
struct Leaf {std::array<float,3> low,high;double flux;std::uint32_t light;};
double flux(const WorldLocalLight& light) {
  const auto& color=light.color_intensity;
  double value=(.2126*color[0]+.7152*color[1]+.0722*color[2])*color[3];
  if(light.axis_v_type[3]==2) {
    const auto& u=light.axis_u_inner;const auto& v=light.axis_v_type;
    const double x=double(u[1])*v[2]-double(u[2])*v[1];
    const double y=double(u[2])*v[0]-double(u[0])*v[2];
    const double z=double(u[0])*v[1]-double(u[1])*v[0];
    value*=4*std::sqrt(x*x+y*y+z*z);
  }
  return std::isfinite(value)?value:0;
}
std::uint32_t branch(std::vector<GILightNode>& nodes,std::span<Leaf> leaves,double scale) {
  const auto index=static_cast<std::uint32_t>(nodes.size());nodes.emplace_back();
  auto low=leaves[0].low,high=leaves[0].high;
  for(const auto& leaf:leaves)for(unsigned axis=0;axis<3;++axis) {
    low[axis]=std::min(low[axis],leaf.low[axis]);high[axis]=std::max(high[axis],leaf.high[axis]);
  }
  auto& node=nodes[index];
  for(unsigned axis=0;axis<3;++axis) {node.bounds_min_flux[axis]=low[axis];node.bounds_max[axis]=high[axis];}
  node.links[3]=static_cast<std::uint32_t>(leaves.size());
  if(leaves.size()==1) {
    node.bounds_min_flux[3]=static_cast<float>(std::max(leaves[0].flux/scale,1e-20));
    node.links[2]=leaves[0].light;return index;
  }
  unsigned axis=0;
  for(unsigned i=1;i<3;++i)if(double(high[i])-low[i]>double(high[axis])-low[axis])axis=i;
  const auto middle=leaves.size()/2;
  std::nth_element(leaves.begin(),leaves.begin()+middle,leaves.end(),[axis](const Leaf& a,const Leaf& b) {
    const double ac=double(a.low[axis])+a.high[axis],bc=double(b.low[axis])+b.high[axis];
    return ac==bc?a.light<b.light:ac<bc;
  });
  const auto left=branch(nodes,leaves.first(middle),scale),right=branch(nodes,leaves.subspan(middle),scale);
  // The vector was reserved for the complete tree before recursion.
  nodes[index].links[0]=left;nodes[index].links[1]=right;
  nodes[index].bounds_min_flux[3]=nodes[left].bounds_min_flux[3]+nodes[right].bounds_min_flux[3];
  return index;
}
}
std::vector<GILightNode> build_gi_light_tree(std::span<const WorldLocalLight> lights) {
  std::vector<Leaf> leaves;leaves.reserve(lights.size());double scale=0;
  for(std::size_t i=0;i<lights.size();++i) {
    const auto& light=lights[i];const auto energy=flux(light);
    if(energy<=0 || light.position_range[3]<=0)continue;
    Leaf leaf{};leaf.flux=energy;leaf.light=static_cast<std::uint32_t>(i);
    bool finite=true;
    for(unsigned axis=0;axis<3;++axis) {
      const double radius=light.axis_v_type[3]==2?std::abs(double(light.axis_u_inner[axis]))+std::abs(double(light.axis_v_type[axis])):0;
      const double low=double(light.position_range[axis])-radius,high=double(light.position_range[axis])+radius;
      finite=finite && std::isfinite(low) && std::isfinite(high) &&
          low>=-std::numeric_limits<float>::max() && high<=std::numeric_limits<float>::max();
      leaf.low[axis]=static_cast<float>(low);leaf.high[axis]=static_cast<float>(high);
    }
    if(!finite)continue;
    scale=std::max(scale,energy);leaves.push_back(leaf);
  }
  std::vector<GILightNode> nodes;
  if(!leaves.empty()) {nodes.reserve(leaves.size()*2-1);branch(nodes,leaves,scale);}
  return nodes;
}
}
