#include "InstanceSelection.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
struct Sphere {float center[3],radius;};
Sphere transformed(const InstanceSelectionView& view,const float* center,float radius) {
  Sphere sphere{};double padding_squared{};
  for(unsigned row=0;row<3;++row) {
    double value=view.world[row][3],magnitude=std::abs(value);
    for(unsigned column=0;column<3;++column) {
      const auto product=double(view.world[row][column])*center[column];value+=product;magnitude+=std::abs(product);
    }
    sphere.center[row]=float(value);
    const auto padding=magnitude*8*std::numeric_limits<float>::epsilon();padding_squared+=padding*padding;
  }
  sphere.radius=std::nextafter(float(double(radius)*view.error_scale[1]+std::sqrt(padding_squared)),
      std::numeric_limits<float>::infinity());
  return sphere;
}
bool visible(const InstanceSelectionView& view,const Sphere& sphere) {
  if(!std::isfinite(sphere.radius))return true;
  for(float value:sphere.center)if(!std::isfinite(value))return true;
  if(view.flags[0])for(const auto& plane:view.planes) {
    double dot=plane[3],length{};
    for(unsigned axis=0;axis<3;++axis) {dot+=double(plane[axis])*sphere.center[axis];length+=double(plane[axis])*plane[axis];}
    if(dot < -double(sphere.radius)*std::sqrt(length))return false;
  }
  return true;
}
float projected(const InstanceSelectionView& view,const Sphere& sphere,float error) {
  if(error==0)return 0;
  if(!std::isfinite(sphere.radius))return std::numeric_limits<float>::max();
  for(float value:sphere.center)if(!std::isfinite(value))return std::numeric_limits<float>::max();
  double distance{};
  for(unsigned axis=0;axis<3;++axis) {const auto delta=double(sphere.center[axis])-view.eye_focal[axis];distance+=delta*delta;}
  distance=std::max(.001,std::sqrt(distance)-sphere.radius);
  const auto result=double(error)*view.error_scale[1]*view.eye_focal[3]/distance;
  return result>=std::numeric_limits<float>::max()?std::numeric_limits<float>::max():
      std::nextafter(float(result),std::numeric_limits<float>::infinity());
}
}
InstanceSelectionView instance_selection_view(const SelectionView& camera,const GeometryTransform& transform) {
  InstanceSelectionView view;
  std::copy_n(camera.eye,3,view.eye_focal);view.eye_focal[3]=camera.focal_pixels;
  view.error_scale[0]=camera.error_pixels;
  double squared{};
  for(unsigned row=0;row<3;++row)for(unsigned column=0;column<4;++column) {
    view.world[row][column]=transform.world[row*4+column];
    if(column<3)squared+=double(view.world[row][column])*view.world[row][column];
  }
  // Frobenius norm bounds every singular value, including mirrored/sheared/nonuniform transforms.
  view.error_scale[1]=std::nextafter(float(std::sqrt(squared)),std::numeric_limits<float>::infinity());
  for(unsigned row=0;row<6;++row)std::copy_n(camera.planes[row],4,view.planes[row]);
  view.flags[0]=camera.frustum?1:0;return view;
}
float instance_selection_error(const InstanceSelectionView& view,const GeometryBounds& bounds) {
  return projected(view,transformed(view,bounds.center,bounds.radius),bounds.error);
}
bool valid_instance_selection(std::span<const InstanceSelectionView> views) {
  if(views.empty() || views.size()>1000000)return false;
  for(const auto& view:views) {
    for(const auto value:view.eye_focal)if(!std::isfinite(value))return false;
    if(view.eye_focal[3]<=0 || !std::isfinite(view.error_scale[0]) || view.error_scale[0]<0 ||
        !std::isfinite(view.error_scale[1]) || view.error_scale[1]<=0)return false;
    for(const auto& row:view.world)for(const auto value:row)if(!std::isfinite(value))return false;
    if(view.flags[0])for(const auto& plane:view.planes)for(const auto value:plane)if(!std::isfinite(value))return false;
  }
  return true;
}
bool select_geometry_instances(const SelectionTopology& topology,std::span<const GpuPage> pages,
    std::span<const InstanceSelectionView> views,std::uint32_t cluster_capacity,std::uint32_t feedback_capacity,
    SelectionResult& output,std::string& error) {
  output={};
  const auto fail=[&](const char* reason){error=reason;output.clusters.clear();return false;};
  if(!cluster_capacity || !feedback_capacity || !valid_instance_selection(views))return fail("invalid instance selection views or limits");
  for(auto page:topology.pages)if(page>=pages.size())return fail("incomplete geometry page table");
  std::vector<bool> active(topology.groups.size());
  std::vector<std::uint32_t> order(topology.groups.size());std::iota(order.begin(),order.end(),0);
  std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return topology.groups[a].depth>topology.groups[b].depth;});
  std::vector<std::uint32_t> requested(pages.size(),invalid_id);
  const auto request=[&](std::uint32_t page,float priority) {
    auto& previous=requested[page];
    if(previous!=invalid_id) {output.requests[previous].priority=std::max(output.requests[previous].priority,priority);return;}
    if(output.requests.size()<feedback_capacity) {
      previous=static_cast<std::uint32_t>(output.requests.size());output.requests.push_back({page,priority});
    } else ++output.feedback_overflow;
  };
  for(const auto id:order) {
    const auto& group=topology.groups[id];bool parents=true;
    for(unsigned parent=0;parent<group.parent_count;++parent)parents=parents && active[topology.parents[group.first_parent+parent]];
    if(!group.root && !parents)continue;
    bool refine=group.root!=0;float priority=refine?std::numeric_limits<float>::max():0;
    if(!refine)for(const auto& view:views) {
      const auto sphere=transformed(view,group.center,group.radius);
      const auto pixels=projected(view,sphere,group.error);
      if(pixels>view.error_scale[0] && visible(view,sphere)) {refine=true;priority=std::max(priority,pixels);}
    }
    if(!refine)continue;
    bool complete=true;
    for(unsigned p=0;p<group.page_count;++p) {
      const auto page=topology.pages[group.first_page+p];const auto& entry=pages[page];
      if(!entry.resident || entry.slot==invalid_page || !entry.generation) {complete=false;request(page,priority);}
    }
    if(group.root && !complete)++output.missing_roots;
    active[id]=complete;
  }
  if(output.missing_roots)return fail("coarse geometry roots are not resident");
  for(unsigned id=0;id<topology.clusters.size();++id) {
    const auto& cluster=topology.clusters[id];
    if(!active[cluster.group] || (cluster.refined_group!=invalid_id && active[cluster.refined_group]))continue;
    bool shown=false;for(const auto& view:views)shown=shown || visible(view,transformed(view,cluster.center,cluster.radius));
    if(!shown)continue;
    if(output.clusters.size()==cluster_capacity)return fail("geometry selected cluster capacity exceeded");
    output.clusters.push_back(id);
  }
  error.clear();return true;
}
}
