#include "SceneHierarchyInternal.h"
#include "ResourceDigest.h"
#include "SceneMaterialJson.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void require(bool value,const char* error) {if(!value)throw std::runtime_error(error);}
bool hash(const std::string& value) {
  return value.size()==64 && std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
void bounds(const std::array<float,6>& b) {
  for(float v:b)require(std::isfinite(v),"hierarchy bounds nonfinite");
  for(unsigned a=0;a<3;++a)require(b[a]<=b[a+3],"hierarchy bounds inverted");
}
void geometry(const SceneHierarchyGeometry& g) {
  require(!g.file.empty() && hash(g.hash) && g.triangles && g.triangles<=262144 &&
      g.pages && g.pages<=4096 && g.clusters && g.clusters<=262144 && g.metadata_bytes && g.encoded_bytes &&
      std::isfinite(g.error) && g.error>=0,"hierarchy coarse geometry invalid");
  require(g.page_used_bytes.size()==g.pages && !g.root_page_ids.empty(),"hierarchy root payload summary missing");
  std::set<unsigned> roots;
  for(auto id:g.root_page_ids)require(id<g.pages && roots.insert(id).second,"hierarchy root page duplicated or out of range");
  for(auto bytes:g.page_used_bytes)require(bytes && bytes<=page_bytes && bytes%16==0,"hierarchy page used-byte summary invalid");
}
}
std::string hierarchy_digest(std::string_view text) {
  return content::resource_digest({reinterpret_cast<const std::uint8_t*>(text.data()),text.size()});
}
std::string hierarchy_layout_identity(const SceneCatalog& catalog,const SceneHierarchyRequest& request) {
  std::string identity="scene-hierarchy-v1:meshoptimizer-v1.2:index-only:locked-boundaries:morton-contiguous-v2:opaque-exact-faces-v3:"+catalog.source_hash+":"+
      std::to_string(request.target_triangles)+":"+std::to_string(request.fan_in)+":"+std::to_string(request.maximum_triangles);
  for(const auto& p:catalog.primitives) {
    identity+=":"+std::to_string(p.mesh)+":"+std::to_string(p.primitive)+":"+
        std::to_string(p.triangles)+":"+std::to_string(p.first_part)+":"+std::to_string(p.part_count)+":"+
        std::to_string(p.position_only)+":"+p.triangle_order_hash;
    std::string material;if(glz::write_json(p.surface,material))throw std::runtime_error("hierarchy material identity invalid");identity+=material;
  }
  for(const auto& p:catalog.parts)identity+=":"+std::to_string(p.primitive)+":"+std::to_string(p.first_triangle)+":"+std::to_string(p.triangle_count);
  for(const auto& node:catalog.instances) {
    identity+=":"+std::to_string(node.node)+":"+std::to_string(node.mesh)+":";
    identity.append(reinterpret_cast<const char*>(node.transform.data()),sizeof(node.transform));
  }
  return hierarchy_digest(identity);
}
bool validate_scene_hierarchy(const SceneHierarchy& h,std::string& error) {
  try {
    require(h.version==scene_hierarchy_version && hash(h.source_hash) && hash(h.identity) && !h.source.empty() && !h.catalog.empty(),
        "hierarchy version/source identity invalid");
    require(h.target_triangles && h.target_triangles<=65536 && h.fan_in>=2 && h.fan_in<=8 &&
        h.maximum_triangles>=65536 && h.maximum_triangles<=262144,"hierarchy cook limits invalid");
    require(!h.primitives.empty() && h.primitives.size()<=1000000 && !h.instances.empty() && h.instances.size()<=1000000,
        "hierarchy source metadata invalid");
    std::uint64_t triangles{},parts{};bool complete=true;std::set<std::pair<unsigned,unsigned>> ids;
    std::map<unsigned,std::uint64_t> mesh_triangles;
    for(std::size_t i=0;i<h.primitives.size();++i) {
      const auto& p=h.primitives[i];require(p.index==i && p.part_count && p.source_triangles,"hierarchy primitive counts invalid");
      require(ids.emplace(p.mesh,p.primitive).second,"hierarchy primitive duplicated");bounds(p.bounds);
      require(p.order_file.empty()==p.order_hash.empty() && (p.order_hash.empty() || hash(p.order_hash)),"hierarchy order identity invalid");
      std::uint64_t covered{},leaves{};std::set<unsigned> roots;
      for(const auto& n:p.roots) {
        geometry(n.coarse);bounds(n.bounds);require(hash(n.coverage_hash) && roots.insert(n.id).second,"hierarchy root duplicated");
        covered+=n.source_triangles;leaves+=n.leaf_count;
      }
      require(!p.complete || (covered==p.source_triangles && leaves==p.part_count && !p.shard.empty() && hash(p.shard_hash)),
          "hierarchy primitive roots do not cover its complete source");
      require(p.complete || p.roots.empty(),"pending hierarchy exposes incomplete coverage");
      complete=complete && p.complete;triangles+=p.source_triangles;parts+=p.part_count;mesh_triangles[p.mesh]+=p.source_triangles;
    }
    require(h.unique_triangles==triangles && h.leaf_parts==parts && h.complete==complete,"hierarchy total coverage differs");
    std::uint64_t instanced{};std::set<unsigned> nodes;
    for(const auto& n:h.instances) {
      require(nodes.insert(n.node).second,"hierarchy original node duplicated");bounds(n.bounds);
      for(float v:n.transform)require(std::isfinite(v),"hierarchy original node transform nonfinite");
      const auto found=mesh_triangles.find(n.mesh);require(found!=mesh_triangles.end(),"hierarchy original node mesh missing");
      instanced+=found->second;
    }
    require(instanced==h.instanced_triangles,"hierarchy original node coverage differs");
    std::string resources;std::set<std::string> paths;
    for(const auto& r:h.resources) {
      require(!r.path.empty() && paths.insert(r.path).second && hash(r.hash) && r.bytes,"hierarchy resource identity invalid");resources+=r.hash;
    }
    require(!h.resources.empty() && hierarchy_digest(resources)==h.source_hash,"hierarchy resource digest mismatch");
    error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
bool validate_scene_hierarchy_shard(const SceneHierarchy& h,const SceneHierarchyShard& shard,std::string& error) {
  try {
    require(shard.version==scene_hierarchy_version && shard.identity==h.identity && shard.source_hash==h.source_hash &&
        shard.primitive<h.primitives.size(),"hierarchy shard identity invalid");
    const auto& p=h.primitives[shard.primitive];
    require(shard.order_hash==p.order_hash && shard.nodes.size()<=std::uint64_t(p.part_count)*2,"hierarchy shard limit/order invalid");
    std::vector<unsigned> incoming(shard.nodes.size());std::set<unsigned> leaf_ids;std::uint64_t expected{};
    for(unsigned id=0;id<shard.nodes.size();++id) {
      const auto& n=shard.nodes[id];require(n.id==id && n.source_triangles && n.leaf_count && hash(n.coverage_hash),"hierarchy node metadata invalid");
      geometry(n.coarse);bounds(n.bounds);
      if(n.children.empty()) {
        require(n.leaf_part!=invalid_id && leaf_ids.insert(n.leaf_part).second && n.leaf_count==1 && n.first_triangle==expected,
            "hierarchy source leaf coverage has a hole or duplicate");
        expected+=n.source_triangles;
        require(n.coverage_hash==hierarchy_digest(h.identity+":"+std::to_string(p.index)+":"+std::to_string(n.leaf_part)+":"+
            std::to_string(n.first_triangle)+":"+std::to_string(n.source_triangles)),"hierarchy leaf coverage digest differs");
      } else {
        require(n.leaf_part==invalid_id && n.children.size()>=2 && n.children.size()<=h.fan_in,"hierarchy parent fan-in invalid");
        std::uint64_t triangles{},leaves{};std::string coverage=h.identity+":parent:";
        for(const auto child:n.children) {
          require(child<id && ++incoming[child]==1,"hierarchy replacement overlaps or cycles");
          const auto& c=shard.nodes[child];triangles+=c.source_triangles;leaves+=c.leaf_count;coverage+=c.coverage_hash;
          require(n.coarse.error>=c.coarse.error,"hierarchy error underestimates child");
          for(unsigned axis=0;axis<3;++axis)require(n.bounds[axis]<=c.bounds[axis] && n.bounds[axis+3]>=c.bounds[axis+3],"hierarchy parent bounds omit child");
        }
        require(triangles==n.source_triangles && leaves==n.leaf_count && n.coverage_hash==hierarchy_digest(coverage),
            "hierarchy parent source domain is incomplete");
      }
    }
    std::set<unsigned> roots;std::uint64_t covered{},leaves{};
    for(auto root:shard.roots) {
      require(root<shard.nodes.size() && incoming[root]==0 && roots.insert(root).second,"hierarchy forest root invalid");
      covered+=shard.nodes[root].source_triangles;leaves+=shard.nodes[root].leaf_count;
    }
    for(unsigned i=0;i<incoming.size();++i)require((incoming[i]==0)==roots.contains(i),"hierarchy unreachable coverage");
    require(!shard.complete || (covered==p.source_triangles && leaves==p.part_count && expected==p.source_triangles),
        "hierarchy complete forest omits source triangles");
    require(expected<=p.source_triangles && leaves<=p.part_count,"hierarchy forest exceeds source domain");
    error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
