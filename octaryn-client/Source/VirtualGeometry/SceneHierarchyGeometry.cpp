#include "SceneHierarchyInternal.h"
#include "GeometryCoarse.h"
#include <algorithm>
#include <set>

namespace octaryn::client::rendering::virtual_geometry {
SceneHierarchyGeometry describe_hierarchy_geometry(const std::string& relative,const GeometryAsset& asset) {
  SceneHierarchyGeometry result;result.file=relative;result.hash=asset.source_hash;result.triangles=asset.source_triangles;
  result.pages=unsigned(asset.pages.size());result.clusters=unsigned(asset.clusters.size());result.metadata_bytes=geometry_metadata_bytes(asset);
  result.page_used_bytes.resize(asset.pages.size());
  for(const auto& page:asset.pages)result.encoded_bytes+=page.encoded_size;
  for(const auto& c:asset.clusters) {
    result.error=std::max(result.error,c.bounds.error);
    const auto stride=(c.flags&geometry_position_only)?12u:unsigned(sizeof(MapVertex));
    const auto end=std::max(c.vertex_offset+c.vertex_count*stride,c.triangle_offset+c.triangle_count*4);
    auto& used=result.page_used_bytes.at(c.page);used=std::max(used,(end+15)&~15u);
  }
  std::set<unsigned> pinned;
  for(auto root:asset.roots) {
    const auto& group=asset.groups.at(root);
    for(unsigned i=0;i<group.page_count;++i)pinned.insert(asset.group_pages.at(group.first_page+i));
  }
  result.root_page_ids.assign(pinned.begin(),pinned.end());return result;
}
}
