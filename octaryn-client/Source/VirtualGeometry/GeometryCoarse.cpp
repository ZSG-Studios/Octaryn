#include "GeometryCoarse.h"
#include "GeometryMesh.h"
#include "../MapWorld/MapTextureCache.h"
#include <meshoptimizer.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <numeric>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void require(bool value,const char* error) {if(!value)throw std::runtime_error(error);}
void check_cancel(const std::atomic_bool* cancel) {
  require(!cancel || !cancel->load(std::memory_order_relaxed),"coarse geometry preparation canceled");
}
void encode_page(GeometryAsset& asset,const std::vector<std::uint8_t>& raw) {
  GeometryPage page;
  std::vector<std::uint8_t> encoded(meshopt_encodeVertexBufferBound(page_bytes/4,4));
  const auto size=meshopt_encodeVertexBuffer(encoded.data(),encoded.size(),raw.data(),page_bytes/4,4);
  require(size!=0,"coarse page compression failed");encoded.resize(size);
  if(size>=page_bytes)encoded=raw;else page.codec=GeometryCodec::Meshoptimizer;
  page.encoded_size=unsigned(encoded.size());
  const auto hash=map_texture_digest(encoded);std::copy(hash.begin(),hash.end(),page.checksum.begin());
  asset.pages.push_back(page);asset.payloads.push_back(std::move(encoded));
}
}
std::uint64_t geometry_metadata_bytes(const GeometryAsset& asset) {
  return 176ull+asset.clusters.size()*sizeof(GeometryCluster)+asset.groups.size()*sizeof(GeometryGroup)+
      asset.group_pages.size()*4ull+asset.roots.size()*4ull+asset.pages.size()*88ull;
}
bool cook_coarse_geometry(const MapModel& model,const std::string& hash,GeometryAsset& output,
    std::string& error,GeometryCoarseOptions options) {
  try {
    check_cancel(options.cancel);
    require(model.primitives.size()==1 && !model.indices.empty() && model.indices.size()%3==0 &&
        model.indices.size()/3<=options.maximum_triangles && options.maximum_triangles<=262144,
        "coarse input must be one complete bounded material primitive");
    require(options.target_triangles && options.target_triangles<=options.maximum_triangles &&
        std::isfinite(options.inherited_error) && options.inherited_error>=0 &&
        std::isfinite(options.maximum_error) && options.maximum_error>=options.inherited_error,
        "coarse geometry options invalid");
    const bool filter_faces=model.primitives.front().material.alpha_mode!=MapAlphaMode::Blend;
    auto mesh=geometry_mesh(model,model.primitives.front(),options.position_only,filter_faces);
    std::vector<unsigned> simplified(mesh.indices.size());float introduced{};
    const float weights[23]={1,1,1,10,10,10,10,1,1,1,1,1,1,1,1,10,10,10,10,10,10,10,10};
    auto count=meshopt_simplifyWithAttributes(simplified.data(),mesh.indices.data(),mesh.indices.size(),
        mesh.vertices.front().position,mesh.vertices.size(),sizeof(MapVertex),
        options.position_only?nullptr:mesh.attributes.front().data(),sizeof(mesh.attributes.front()),
        options.position_only?nullptr:weights,options.position_only?0:23,mesh.locks.data(),
        std::min(mesh.indices.size(),std::size_t(options.target_triangles)*3),
        options.maximum_error-options.inherited_error,meshopt_SimplifyLockBorder|meshopt_SimplifyErrorAbsolute,&introduced);
    require(count && count%3==0 && count<=mesh.indices.size(),"coarse simplifier removed a complete domain");
    // Full attribute equality and winding are required; BLEND retains layer multiplicity.
    if(filter_faces)count=meshopt_filterIndexBuffer(simplified.data(),simplified.data(),count,
        mesh.vertices.data(),mesh.vertices.size(),sizeof(MapVertex),sizeof(MapVertex));
    require(count!=0,"coarse simplifier left no nondegenerate surface");
    const float inherited=std::nextafter(options.inherited_error+introduced,FLT_MAX);
    require(std::isfinite(inherited) && inherited<=options.maximum_error,"coarse error limit exceeded");
    simplified.resize(count);check_cancel(options.cancel);
    const auto capacity=meshopt_buildMeshletsBound(count,cluster_vertices,cluster_triangles);
    std::vector<meshopt_Meshlet> meshlets(capacity);
    std::vector<unsigned> vertices(capacity*cluster_vertices);
    std::vector<unsigned char> triangles(capacity*cluster_triangles*3);
    const auto meshlet_count=meshopt_buildMeshlets(meshlets.data(),vertices.data(),triangles.data(),
        simplified.data(),count,mesh.vertices.front().position,mesh.vertices.size(),sizeof(MapVertex),
        cluster_vertices,cluster_triangles,0);
    require(meshlet_count>0,"coarse cluster output empty");
    GeometryAsset asset;asset.space=GeometrySpace::Object;asset.source_hash=hash;
    asset.material_count=1;asset.source_triangles=count/3;asset.roots={0};
    std::vector<std::uint8_t> raw(page_bytes);unsigned used{};
    const unsigned stride=options.position_only?12:sizeof(MapVertex);
    const auto& material=model.primitives.front().material;
    for(std::size_t i=0;i<meshlet_count;++i) {
      check_cancel(options.cancel);const auto& m=meshlets[i];
      const auto* local_vertices=vertices.data()+m.vertex_offset;
      const auto* local_triangles=triangles.data()+m.triangle_offset;
      const auto bounds=meshopt_computeMeshletBounds(local_vertices,local_triangles,m.triangle_count,
          mesh.vertices.front().position,mesh.vertices.size(),sizeof(MapVertex));
      const auto bytes=m.vertex_count*stride+m.triangle_count*4;
      used=(used+15)&~15u;
      if(used+bytes>page_bytes) {encode_page(asset,raw);std::fill(raw.begin(),raw.end(),0);used=0;}
      GeometryCluster cluster;cluster.page=unsigned(asset.pages.size());cluster.vertex_offset=used;
      cluster.vertex_count=m.vertex_count;cluster.triangle_count=m.triangle_count;
      cluster.flags=unsigned(material.alpha_mode)|(material.double_sided?256u:0u)|(options.position_only?geometry_position_only:0u);
      cluster.bounds={{bounds.center[0],bounds.center[1],bounds.center[2]},bounds.radius+std::max(1e-5f,bounds.radius*1e-5f),inherited};
      for(unsigned v=0;v<m.vertex_count;++v)std::memcpy(raw.data()+used+v*stride,&mesh.vertices.at(local_vertices[v]),stride);
      used+=m.vertex_count*stride;cluster.triangle_offset=used;
      for(unsigned t=0;t<m.triangle_count;++t) {
        const std::uint32_t packed=local_triangles[t*3]|(unsigned(local_triangles[t*3+1])<<8)|(unsigned(local_triangles[t*3+2])<<16);
        std::memcpy(raw.data()+used,&packed,4);used+=4;
      }
      asset.clusters.push_back(cluster);
    }
    encode_page(asset,raw);
    asset.group_pages.resize(asset.pages.size());std::iota(asset.group_pages.begin(),asset.group_pages.end(),0u);
    GeometryGroup group;group.cluster_count=unsigned(asset.clusters.size());group.page_count=unsigned(asset.pages.size());
    group.simplified.error=FLT_MAX;asset.groups.push_back(group);
    if(!validate_geometry(asset,error))return false;
    output=std::move(asset);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
