#include "CookedAsset.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <stdexcept>

namespace octaryn::client::animation {
namespace {
std::array<unsigned,3> canonical(std::array<unsigned,3> triangle) {
  const auto smallest=std::min_element(triangle.begin(),triangle.end());std::rotate(triangle.begin(),smallest,triangle.end());return triangle;
}
void require(bool value,const std::string& error) {if(!value)throw std::runtime_error(error);}
}
bool validate_animation_correspondence(const CookedAsset& asset,std::string& error) {
  try {
    using namespace rendering::virtual_geometry;
    if(asset.geometry.payloads.empty())return true;
    std::vector<std::uint8_t> decoded;unsigned page=invalid_id;
    for(unsigned p=0;p<asset.animation.primitives.size();++p) {
      const auto& primitive=asset.animation.primitives[p];const auto& group=asset.geometry.groups[p];
      std::map<std::array<unsigned,3>,std::int64_t> coverage;
      for(size_t i=0;i<primitive.indices.size();i+=3)++coverage[canonical({primitive.indices[i],primitive.indices[i+1],primitive.indices[i+2]})];
      for(unsigned i=0;i<group.cluster_count;++i) {
        const auto id=group.first_cluster+i;const auto& cluster=asset.geometry.clusters[id];const auto& mapping=asset.cluster_sources[id];
        if(page!=cluster.page) {require(decode_geometry_page(asset.geometry,cluster.page,decoded,error),error);page=cluster.page;}
        for(unsigned local=0;local<cluster.vertex_count;++local) {
          const auto& source=primitive.vertices[asset.source_vertices[mapping.first_vertex+local]];rendering::MapVertex vertex;
          std::memcpy(&vertex,decoded.data()+cluster.vertex_offset+local*sizeof(vertex),sizeof(vertex));
          require(std::equal(std::begin(vertex.position),std::end(vertex.position),source.position.begin()) &&
              std::equal(std::begin(vertex.normal),std::end(vertex.normal),source.normal.begin()) &&
              std::equal(std::begin(vertex.uv),std::end(vertex.uv),source.uv.begin()) &&
              std::equal(std::begin(vertex.uv1),std::end(vertex.uv1),source.uv.begin()+2) &&
              std::equal(std::begin(vertex.tangent),std::end(vertex.tangent),source.tangent.begin()) &&
              std::equal(std::begin(vertex.color),std::end(vertex.color),source.color.begin()),"animated cluster attributes do not match deformation source");
        }
        for(unsigned t=0;t<cluster.triangle_count;++t) {
          unsigned packed;std::memcpy(&packed,decoded.data()+cluster.triangle_offset+t*4,4);std::array<unsigned,3> triangle;
          for(unsigned c=0;c<3;++c)triangle[c]=asset.source_vertices[mapping.first_vertex+((packed>>(c*8))&255)];
          --coverage[canonical(triangle)];
        }
      }
      for(const auto& [triangle,count]:coverage)require(count==0,"animated cluster source topology/winding mismatch");
    }
    return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
