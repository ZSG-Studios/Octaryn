#include "GeometryCoarse.h"
#include "GeometryCache.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
bool append_geometry_roots(const std::filesystem::path& path,const GeometryAsset& asset,MapModel& model,
    std::uint64_t maximum_triangles,std::string& error,const std::atomic_bool* cancel) {
  try {
    if(asset.space!=GeometrySpace::Object || asset.material_count!=1 || model.primitives.size()!=1)
      throw std::runtime_error("hierarchy root source must have one object-space material");
    std::uint64_t count=model.indices.size()/3;
    for(const auto id:asset.roots)for(unsigned c=0;c<asset.groups.at(id).cluster_count;++c)
      count+=asset.clusters.at(asset.groups[id].first_cluster+c).triangle_count;
    if(count>maximum_triangles || maximum_triangles>262144)throw std::runtime_error("hierarchy root merge exceeds bounded triangle capacity");
    std::vector<std::uint8_t> page;unsigned loaded=invalid_id;
    for(const auto id:asset.roots)for(unsigned c=0;c<asset.groups.at(id).cluster_count;++c) {
      if(cancel && cancel->load(std::memory_order_relaxed))throw std::runtime_error("hierarchy root decode canceled");
      const auto& cluster=asset.clusters.at(asset.groups[id].first_cluster+c);
      if(loaded!=cluster.page) {
        if(!read_geometry_page(path,asset.pages.at(cluster.page),page,error))return false;
        loaded=cluster.page;
      }
      const auto first=unsigned(model.vertices.size());
      const auto stride=(cluster.flags&geometry_position_only)?12u:unsigned(sizeof(MapVertex));
      for(unsigned v=0;v<cluster.vertex_count;++v) {
        MapVertex vertex{};std::memcpy(&vertex,page.data()+cluster.vertex_offset+v*stride,stride);
        model.vertices.push_back(vertex);
      }
      for(unsigned t=0;t<cluster.triangle_count;++t) {
        unsigned packed;std::memcpy(&packed,page.data()+cluster.triangle_offset+t*4,4);
        for(unsigned corner=0;corner<3;++corner) {
          const auto index=(packed>>(corner*8))&255u;
          if(index>=cluster.vertex_count)throw std::runtime_error("hierarchy root index invalid");
          model.indices.push_back(first+index);
        }
      }
    }
    model.primitives.front().index_count=unsigned(model.indices.size());error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
