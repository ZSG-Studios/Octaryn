#include "InstanceSelection.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

using namespace octaryn::client::rendering::virtual_geometry;
namespace {
unsigned checks{};
void require(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
SelectionTopology fixture() {
  // Both coarse parents must leave the cut together when their shared child is ready.
  SelectionTopology topology;topology.maximum_depth=1;topology.pages={0,1,2,3};topology.parents={1,2};
  topology.groups.resize(3);auto& fine=topology.groups[0];fine.radius=1;fine.error=1;fine.page_count=2;fine.parent_count=2;
  for(unsigned id=1;id<3;++id) {
    auto& root=topology.groups[id];root.radius=1;root.first_page=id+1;root.page_count=1;root.depth=1;root.root=1;
  }
  topology.clusters={{0,invalid_id,0,0},{0,invalid_id,1,0},{1,0,2,0},{2,0,3,0}};
  for(auto& cluster:topology.clusters)cluster.radius=1;
  return topology;
}
void coverage() {
  const auto topology=fixture();SelectionView camera{{0,0,0},100,1};GeometryTransform far,near;
  far.world[11]=1000;near.world[11]=10;
  std::array views{instance_selection_view(camera,far),instance_selection_view(camera,near)};
  std::array<GpuPage,4> pages{{{0,1,1,0},{1,1,1,0},{2,1,1,0},{3,1,1,0}}};SelectionResult selected;std::string error;
  const auto select=[&](auto input,unsigned capacity=4) {
    return select_geometry_instances(topology,pages,input,capacity,4,selected,error);
  };
  require(select(std::span(views).first(1)) && selected.clusters==std::vector<unsigned>{2,3},"far instance did not retain coarse cut");
  require(select(std::span(views)) && selected.clusters==std::vector<unsigned>{0,1},"near instance did not refine one shared cut");
  std::reverse(views.begin(),views.end());require(select(std::span(views)) && selected.clusters==std::vector<unsigned>{0,1},"union depends on instance ordering");
  pages[1]={};require(select(std::span(views)) && selected.clusters==std::vector<unsigned>{2,3} &&
      selected.requests.size()==1 && selected.requests[0].page==1,"missing child published partial DAG cut");
  pages[1]={1,1,1,0};pages[3]={};require(!select(std::span(views)) && selected.clusters.empty() && selected.missing_roots==1,"missing root emitted partial scene");
  pages[3]={3,1,1,0};require(!select(std::span(views),1) && selected.clusters.empty(),"overflow emitted partial scene");
  for(auto& view:views) {view.flags[0]=1;view.planes[0][0]=1;view.planes[0][3]=-10;}
  require(select(std::span(views)) && selected.clusters.empty(),"union did not cull offscreen geometry");
  views[0].flags[0]=0;require(select(std::span(views)) && selected.clusters==std::vector<unsigned>{0,1},"offscreen ray view lost full coverage");
  views[0].eye_focal[0]=std::numeric_limits<float>::quiet_NaN();require(!select(std::span(views)),"nonfinite camera accepted");
}
void affine_bounds() {
  SelectionView camera{{0,0,0},100,1};GeometryTransform transform;
  transform.world={-2,7,.5f,0,0,3,9,0,0,0,.25f,1000};
  const auto view=instance_selection_view(camera,transform);
  // Dense unit sphere directions verify the scale bound for mirror, shear and nonuniform scale.
  for(unsigned x=0;x<101;++x)for(unsigned y=0;y<101;++y) {
    const auto theta=x*3.141592653589793/100,phi=y*6.283185307179586/100;
    const double v[]{std::sin(theta)*std::cos(phi),std::sin(theta)*std::sin(phi),std::cos(theta)};
    double length{};
    for(unsigned row=0;row<3;++row) {
      double value{};for(unsigned column=0;column<3;++column)value+=transform.world[row*4+column]*v[column];length+=value*value;
    }
    require(std::sqrt(length)<=view.error_scale[1],"affine scale bound underestimated displacement");
  }
  auto topology=fixture();std::array<GpuPage,4> pages{{{0,1,1,0},{1,1,1,0},{2,1,1,0},{3,1,1,0}}};
  SelectionResult selected;std::string error;
  require(select_geometry_instances(topology,pages,std::span(&view,1),4,4,selected,error) &&
      selected.clusters==std::vector<unsigned>{0,1},"large affine displacement did not refine distant shared geometry");
  // Cancellation at large coordinates must not cull a surface touching a frustum plane.
  transform.world={1,1,0,-100000000,0,1,0,0,0,0,1,0};camera.frustum=true;camera.planes[0][0]=1;camera.planes[0][3]=-1;
  auto cancellation=instance_selection_view(camera,transform);
  for(auto& group:topology.groups) {group.center[0]=100000000;group.center[1]=1;group.radius=0;}
  for(auto& cluster:topology.clusters) {cluster.center[0]=100000000;cluster.center[1]=1;cluster.radius=0;}
  require(select_geometry_instances(topology,pages,std::span(&cancellation,1),4,4,selected,error) && !selected.clusters.empty(),
      "FP32 transform cancellation culled boundary geometry");
}
}
int main() {
  try {coverage();affine_bounds();std::printf("geometry_instance_selection passed=1 checks=%u union_cut=1 affine_bound=1 complete_roots=1 offscreen=1 fp32_padding=1\n",checks);return 0;}
  catch(const std::exception& error) {std::fprintf(stderr,"geometry_instance_selection failed=%s checks=%u\n",error.what(),checks);return 1;}
}
