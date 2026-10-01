#include "SceneHierarchy.h"
#include "GeometryCache.h"
#include "GeometryCoarse.h"
#include "GeometryMesh.h"
#include <meshoptimizer.h>
#include <algorithm>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <unordered_map>

int audit_scene_hierarchy(const std::filesystem::path& package,std::uint32_t primitive) {
  using namespace octaryn::client::rendering;
  using namespace octaryn::client::rendering::virtual_geometry;
  SceneHierarchy header;std::string error;
  if(!read_scene_hierarchy(package,header,error))throw std::runtime_error(error);
  SceneHierarchyShard shard;
  if(!read_scene_hierarchy_shard(package,header,primitive,shard,error))throw std::runtime_error(error);
  const auto& p=header.primitives.at(primitive);
  for(const auto root:shard.roots) {
    const auto& node=shard.nodes.at(root);std::filesystem::path file;GeometryAsset asset;
    if(!scene_hierarchy_path(package,node.coarse.file,file,error) || !read_geometry_cache(file,node.coarse.hash,asset,error))
      throw std::runtime_error(error);
    MapModel model;model.primitives.emplace_back();model.primitives.front().material=p.surface;
    if(!append_geometry_roots(file,asset,model,262144,error))throw std::runtime_error(error);
    const auto mesh=geometry_mesh(model,model.primitives.front(),p.position_only);
    std::vector<unsigned> filtered(mesh.indices.size());
    const auto unique_indices=meshopt_filterIndexBuffer(filtered.data(),mesh.indices.data(),mesh.indices.size(),
        mesh.vertices.data(),mesh.vertices.size(),sizeof(MapVertex),sizeof(MapVertex));
    std::set<std::array<unsigned,3>> faces;
    std::uint64_t same_winding{},opposite_pairs{},degenerate{};
    for(std::size_t i=0;i<mesh.indices.size();i+=3) {
      auto a=mesh.indices[i],b=mesh.indices[i+1],c=mesh.indices[i+2];
      if(a==b || a==c || b==c) {++degenerate;continue;}
      if(b<a && b<c) {const auto old=a;a=b;b=c;c=old;}
      else if(c<a && c<b) {const auto old=a;a=c;c=b;b=old;}
      if(!faces.insert({a,b,c}).second)++same_winding;
      else opposite_pairs+=faces.contains({a,c,b});
    }
    if((mesh.indices.size()-unique_indices)/3!=same_winding+degenerate)
      throw std::runtime_error("hierarchy redundant-face audit differs from pinned filter");
    const auto locked=std::count_if(mesh.locks.begin(),mesh.locks.end(),[](auto flags){return flags!=0;});
    std::uint64_t all_locked{},any_locked{};
    struct Edge {unsigned count{};int winding{};};
    std::unordered_map<std::uint64_t,Edge> edges;edges.reserve(mesh.indices.size());
    for(std::size_t i=0;i<mesh.indices.size();i+=3) {
      const bool a=mesh.locks.at(mesh.indices[i])!=0,b=mesh.locks.at(mesh.indices[i+1])!=0,c=mesh.locks.at(mesh.indices[i+2])!=0;
      all_locked+=a&&b&&c;any_locked+=a||b||c;
      for(unsigned side=0;side<3;++side) {
        const auto x=mesh.indices[i+side],y=mesh.indices[i+(side+1)%3];
        const auto key=(std::uint64_t(std::min(x,y))<<32)|std::max(x,y);
        auto& edge=edges[key];++edge.count;edge.winding+=x<y?1:-1;
      }
    }
    std::uint64_t exterior{},nonmanifold{},inconsistent{};
    for(const auto& [key,edge]:edges) {
      exterior+=edge.count==1;nonmanifold+=edge.count>2;inconsistent+=edge.count==2 && edge.winding!=0;
    }
    std::printf("scene_hierarchy_root_audit primitive=%u node=%u source_triangles=%llu coarse_triangles=%llu vertices=%llu locked_vertices=%llu all_locked_triangles=%llu any_locked_triangles=%llu exterior_edges=%llu nonmanifold_edges=%llu inconsistent_edges=%llu same_winding_duplicates=%llu opposite_pairs=%llu degenerate_triangles=%llu pages=%u error=%.9g position_only=%u\n",
        primitive,root,static_cast<unsigned long long>(node.source_triangles),static_cast<unsigned long long>(mesh.indices.size()/3),
        static_cast<unsigned long long>(mesh.vertices.size()),static_cast<unsigned long long>(locked),
        static_cast<unsigned long long>(all_locked),static_cast<unsigned long long>(any_locked),
        static_cast<unsigned long long>(exterior),static_cast<unsigned long long>(nonmanifold),static_cast<unsigned long long>(inconsistent),
        static_cast<unsigned long long>(same_winding),static_cast<unsigned long long>(opposite_pairs),static_cast<unsigned long long>(degenerate),
        unsigned(asset.pages.size()),double(node.coarse.error),unsigned(p.position_only));
    std::fflush(stdout);
  }
  return 0;
}
