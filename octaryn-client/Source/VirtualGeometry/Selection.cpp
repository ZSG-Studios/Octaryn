#include "Selection.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace octaryn::client::rendering::virtual_geometry {
bool build_selection_topology(const GeometryAsset& asset,SelectionTopology& output,std::string& error) {
  SelectionTopology result;
  const auto fail=[&](const char* message){error=message;return false;};
  if(asset.groups.empty() || asset.roots.empty())return fail("geometry selection has no roots");
  std::vector<std::vector<std::uint32_t>> parents(asset.groups.size());
  std::vector<bool> roots(asset.groups.size());
  for(auto root:asset.roots) {
    if(root>=roots.size() || roots[root])return fail("invalid duplicate geometry root");
    roots[root]=true;
  }
  for(const auto& cluster:asset.clusters) {
    if(cluster.group>=asset.groups.size() || cluster.page>=asset.pages.size())return fail("invalid cluster owner/page");
    if(cluster.refined_group!=invalid_id) {
      if(cluster.refined_group>=parents.size() || asset.groups[cluster.refined_group].depth>=asset.groups[cluster.group].depth)
        return fail("cluster hierarchy depth is not acyclic");
      parents[cluster.refined_group].push_back(cluster.group);
    }
    SelectionCluster record{cluster.group,cluster.refined_group,cluster.page,0};
    std::copy_n(cluster.bounds.center,3,record.center);record.radius=cluster.bounds.radius;
    if(!std::isfinite(record.radius) || record.radius<0)return fail("invalid cluster sphere");
    for(float coordinate:record.center)if(!std::isfinite(coordinate))return fail("invalid cluster center");
    result.clusters.push_back(record);
  }
  for(std::uint32_t i=0;i<asset.groups.size();++i) {
    const auto& group=asset.groups[i];
    if(group.depth>=asset.groups.size() || group.first_page>asset.group_pages.size() || group.page_count>asset.group_pages.size()-group.first_page ||
       !group.page_count || !std::isfinite(group.simplified.radius) || group.simplified.radius<0 ||
       !std::isfinite(group.simplified.error) || group.simplified.error<0)
      return fail("invalid geometry group pages/bounds");
    for(float coordinate:group.simplified.center)if(!std::isfinite(coordinate))return fail("nonfinite group center");
    auto& incoming=parents[i];std::sort(incoming.begin(),incoming.end());
    incoming.erase(std::unique(incoming.begin(),incoming.end()),incoming.end());
    if(roots[i]!=incoming.empty())return fail("geometry roots do not match hierarchy");
    SelectionGroup record;
    std::copy_n(group.simplified.center,3,record.center);record.radius=group.simplified.radius;
    record.error=group.simplified.error;record.depth=group.depth;record.root=roots[i]?1u:0u;
    record.first_parent=static_cast<std::uint32_t>(result.parents.size());record.parent_count=static_cast<std::uint32_t>(incoming.size());
    result.parents.insert(result.parents.end(),incoming.begin(),incoming.end());
    record.first_page=static_cast<std::uint32_t>(result.pages.size());record.page_count=group.page_count;
    for(std::uint32_t p=0;p<group.page_count;++p) {
      auto page=asset.group_pages[group.first_page+p];if(page>=asset.pages.size())return fail("group page outside asset");
      result.pages.push_back(page);
    }
    result.maximum_depth=std::max(result.maximum_depth,record.depth);result.groups.push_back(record);
  }
  output=std::move(result);error.clear();return true;
}
bool select_geometry(const SelectionTopology& topology,std::span<const GpuPage> pages,const SelectionView& view,
    std::uint32_t cluster_capacity,std::uint32_t feedback_capacity,SelectionResult& output,std::string& error) {
  output={};
  const auto fail=[&](const char* message){error=message;output.clusters.clear();return false;};
  if(!cluster_capacity || !feedback_capacity || !std::isfinite(view.focal_pixels) || view.focal_pixels<=0 ||
      !std::isfinite(view.error_pixels) || view.error_pixels<0)return fail("invalid selection limits");
  for(float coordinate:view.eye)if(!std::isfinite(coordinate))return fail("invalid selection eye");
  if(view.frustum)for(const auto& plane:view.planes)for(float value:plane)
    if(!std::isfinite(value))return fail("invalid selection frustum");
  for(auto page:topology.pages)if(page>=pages.size())return fail("incomplete geometry page table");
  std::vector<bool> active(topology.groups.size());
  std::vector<std::uint32_t> order(topology.groups.size());std::iota(order.begin(),order.end(),0);
  std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return topology.groups[a].depth>topology.groups[b].depth;});
  const auto visible=[&](const float* center,float radius) {
    if(view.frustum)for(const auto& plane:view.planes) {
      const auto dot=plane[0]*center[0]+plane[1]*center[1]+plane[2]*center[2]+plane[3];
      const auto length=std::sqrt(plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2]);
      if(dot < -radius*length)return false;
    }
    return true;
  };
  const auto request=[&](std::uint32_t page,float priority) {
    auto previous=std::find_if(output.requests.begin(),output.requests.end(),[&](auto p){return p.page==page;});
    if(previous!=output.requests.end()) {previous->priority=std::max(previous->priority,priority);return;}
    if(output.requests.size()<feedback_capacity)output.requests.push_back({page,priority});
    else ++output.feedback_overflow;
  };
  for(auto id:order) {
    const auto& group=topology.groups[id];
    bool parents_active=true;
    for(std::uint32_t p=0;p<group.parent_count;++p)parents_active&=active[topology.parents[group.first_parent+p]];
    float distance_squared=0;
    for(unsigned axis=0;axis<3;++axis) {const auto delta=group.center[axis]-view.eye[axis];distance_squared+=delta*delta;}
    const auto distance=std::max(.001f,std::sqrt(distance_squared)-group.radius);
    const auto pixels=group.root?std::numeric_limits<float>::max():
        std::min(double(std::numeric_limits<float>::max()),double(group.error)*view.focal_pixels/distance);
    if(!group.root && (!parents_active || pixels<=view.error_pixels))continue;
    if(!group.root && !visible(group.center,group.radius))continue;
    bool complete=true;
    for(std::uint32_t p=0;p<group.page_count;++p) {
      const auto page=topology.pages[group.first_page+p];const auto& entry=pages[page];
      if(!entry.resident || entry.slot==invalid_page || !entry.generation) {complete=false;request(page,static_cast<float>(pixels));}
    }
    if(group.root && !complete)++output.missing_roots;
    active[id]=complete;
  }
  if(output.missing_roots)return fail("coarse geometry roots are not resident");
  for(std::uint32_t i=0;i<topology.clusters.size();++i) {
    const auto& cluster=topology.clusters[i];
    if(!active[cluster.group] || (cluster.refined_group!=invalid_id && active[cluster.refined_group]))continue;
    if(!visible(cluster.center,cluster.radius))continue;
    if(output.clusters.size()==cluster_capacity)return fail("geometry selected cluster capacity exceeded");
    output.clusters.push_back(i);
  }
  error.clear();return true;
}
}
