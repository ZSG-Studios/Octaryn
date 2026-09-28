#include "AnimationCook.h"
#include "../MapWorld/MapTextureCache.h"
#include <meshoptimizer.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <set>
#include <stdexcept>

namespace octaryn::client::animation {
namespace {
using namespace rendering;
using namespace rendering::virtual_geometry;
void require(bool value,const char* error) {if(!value)throw std::runtime_error(error);}
MapVertex raster_vertex(const SourceVertex& input) {
  MapVertex output{};std::copy_n(input.position.begin(),3,output.position);std::copy_n(input.normal.begin(),3,output.normal);
  std::copy_n(input.uv.begin(),2,output.uv);std::copy_n(input.uv.begin()+2,2,output.uv1);
  std::copy_n(input.tangent.begin(),4,output.tangent);std::copy_n(input.color.begin(),4,output.color);return output;
}
}
bool build_animation_geometry(CookedAsset& cooked,const std::string& hash,std::string& error) {
  try {
    using namespace rendering::virtual_geometry;GeometryAsset geometry;geometry.space=GeometrySpace::Object;
    require(!cooked.animation.primitives.empty() && cooked.materials.size()==cooked.animation.primitives.size(),"animated materials do not match primitives");
    geometry.source_hash=hash;geometry.material_count=unsigned(cooked.materials.size());
    std::vector<std::vector<std::uint8_t>> pages;unsigned used=page_bytes;
    std::vector<ClusterSource> sources;std::vector<unsigned> vertices;
    for(unsigned primitive=0;primitive<cooked.animation.primitives.size();++primitive) {
      const auto& source=cooked.animation.primitives[primitive];const auto& material=cooked.materials[primitive];
      require(!source.vertices.empty() && !source.indices.empty() && source.indices.size()%3==0,"animated cook requires indexed triangles");
      for(auto index:source.indices)require(index<source.vertices.size(),"animated cook vertex index invalid");
      for(const auto& vertex:source.vertices)for(float value:vertex.position)require(std::isfinite(value),"animated cook position nonfinite");
      geometry.source_triangles+=source.indices.size()/3;
      const auto bound=meshopt_buildMeshletsBound(source.indices.size(),cluster_vertices,cluster_triangles);
      std::vector<meshopt_Meshlet> meshes(bound);std::vector<unsigned> mesh_vertices(bound*cluster_vertices);
      std::vector<unsigned char> triangles(bound*cluster_triangles*3);
      const auto count=meshopt_buildMeshlets(meshes.data(),mesh_vertices.data(),triangles.data(),source.indices.data(),source.indices.size(),
          source.vertices.front().position.data(),source.vertices.size(),sizeof(SourceVertex),cluster_vertices,cluster_triangles,0);
      GeometryGroup group;group.first_cluster=unsigned(geometry.clusters.size());group.cluster_count=unsigned(count);
      group.simplified.error=FLT_MAX;std::set<unsigned> dependencies;
      for(size_t m=0;m<count;++m) {
        const auto& mesh=meshes[m];auto* local_vertices=mesh_vertices.data()+mesh.vertex_offset;auto* local_triangles=triangles.data()+mesh.triangle_offset;
        meshopt_optimizeMeshlet(local_vertices,local_triangles,mesh.triangle_count,mesh.vertex_count);
        const auto bounds=meshopt_computeMeshletBounds(local_vertices,local_triangles,mesh.triangle_count,
            source.vertices.front().position.data(),source.vertices.size(),sizeof(SourceVertex));
        GeometryCluster cluster;cluster.group=primitive;cluster.material=primitive;cluster.flags=unsigned(material.alpha_mode)|(material.double_sided?256u:0u);
        cluster.vertex_count=mesh.vertex_count;cluster.triangle_count=mesh.triangle_count;
        std::copy_n(bounds.center,3,cluster.bounds.center);cluster.bounds.radius=bounds.radius+std::max(1e-5f,bounds.radius*1e-5f);
        const auto bytes=mesh.vertex_count*sizeof(MapVertex)+mesh.triangle_count*4;used=(used+15)&~15u;
        if(used+bytes>page_bytes) {pages.emplace_back(page_bytes,0);used=0;}
        cluster.page=unsigned(pages.size()-1);cluster.vertex_offset=used;sources.push_back({primitive,unsigned(vertices.size())});
        for(unsigned i=0;i<mesh.vertex_count;++i) {
          const auto index=local_vertices[i];vertices.push_back(index);const auto vertex=raster_vertex(source.vertices[index]);
          std::memcpy(pages.back().data()+used,&vertex,sizeof(vertex));used+=sizeof(vertex);
        }
        cluster.triangle_offset=used;
        for(unsigned i=0;i<mesh.triangle_count;++i) {
          const unsigned packed=local_triangles[i*3]|(unsigned(local_triangles[i*3+1])<<8)|(unsigned(local_triangles[i*3+2])<<16);
          std::memcpy(pages.back().data()+used,&packed,4);used+=4;
        }
        dependencies.insert(cluster.page);geometry.clusters.push_back(cluster);
      }
      require(count>0,"animated primitive produced no clusters");
      const auto* first=&geometry.clusters[group.first_cluster].bounds;
      const auto sphere=meshopt_computeSphereBounds(first->center,count,sizeof(GeometryCluster),&first->radius,sizeof(GeometryCluster));
      std::copy_n(sphere.center,3,group.simplified.center);group.simplified.radius=sphere.radius+std::max(1e-5f,sphere.radius*1e-5f);
      group.first_page=unsigned(geometry.group_pages.size());group.page_count=unsigned(dependencies.size());
      geometry.group_pages.insert(geometry.group_pages.end(),dependencies.begin(),dependencies.end());
      geometry.roots.push_back(primitive);geometry.groups.push_back(group);
    }
    for(const auto& raw:pages) {
      GeometryPage page;std::vector<std::uint8_t> encoded(meshopt_encodeVertexBufferBound(page_bytes/4,4));
      const auto size=meshopt_encodeVertexBuffer(encoded.data(),encoded.size(),raw.data(),page_bytes/4,4);
      require(size!=0,"animated page encoding failed");encoded.resize(size);
      if(size>=page_bytes)encoded=raw;else page.codec=GeometryCodec::Meshoptimizer;
      page.encoded_size=unsigned(encoded.size());const auto checksum=rendering::map_texture_digest(encoded);
      std::copy(checksum.begin(),checksum.end(),page.checksum.begin());geometry.pages.push_back(page);geometry.payloads.push_back(std::move(encoded));
    }
    cooked.geometry=std::move(geometry);cooked.cluster_sources=std::move(sources);cooked.source_vertices=std::move(vertices);
    return validate_cooked_animation(cooked,error);
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
