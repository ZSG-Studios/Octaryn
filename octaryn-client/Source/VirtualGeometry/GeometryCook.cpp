#include "GeometryCook.h"
#include "GeometryMesh.h"
#include "../MapWorld/MapTextureCache.h"
#include <meshoptimizer.h>
#include <clusterlod.h>
#include <algorithm>
#include <array>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void require(bool valid,const char* message) {if(!valid)throw std::runtime_error(message);}
GeometryBounds bounds(clodBounds b) {
  // Preserve upstream accumulated DAG error, expanding spatial bounds for rounding.
  return {{b.center[0],b.center[1],b.center[2]},b.radius+std::max(1e-5f,b.radius*1e-5f),b.error};
}
struct PageBuilder {
  GeometryAsset& asset;
  std::array<std::vector<std::vector<std::uint8_t>>,2> buckets;
  unsigned offsets[2]{page_bytes,page_bytes};
  void append(GeometryCluster& output,const clodCluster& cluster,const GeometryMesh& mesh,bool root) {
    auto& raw=buckets[root?0:1];auto& used=offsets[root?0:1];
    std::array<unsigned,cluster_vertices> vertices{};
    std::array<unsigned char,cluster_triangles*3> triangles{};
    require(cluster.vertex_count<=cluster_vertices && cluster.index_count<=cluster_triangles*3,
        "clusterlod exceeded configured output limits");
    const auto count=clodLocalIndices(vertices.data(),triangles.data(),cluster.indices,cluster.index_count);
    require(count==cluster.vertex_count,"cluster local vertex count mismatch");
    const auto stride=(output.flags&geometry_position_only)?12u:unsigned(sizeof(MapVertex));
    const unsigned bytes=unsigned(count*stride+cluster.index_count/3*4);
    used=(used+15)&~15u;
    if(used+bytes>page_bytes) {raw.emplace_back(page_bytes,0);used=0;}
    output.page=unsigned(raw.size()-1)|(root?0x80000000u:0u);output.vertex_offset=used;output.vertex_count=unsigned(count);
    for(size_t i=0;i<count;++i)std::memcpy(raw.back().data()+used+i*stride,&mesh.vertices.at(vertices[i]),stride);
    used+=unsigned(count*stride);output.triangle_offset=used;output.triangle_count=unsigned(cluster.index_count/3);
    for(unsigned i=0;i<output.triangle_count;++i) {
      const std::uint32_t packed=triangles[i*3]|(unsigned(triangles[i*3+1])<<8)|(unsigned(triangles[i*3+2])<<16);
      std::memcpy(raw.back().data()+used,&packed,4);used+=4;
    }
  }
  void finish() {
    // Coarse roots never share allocation pages with demand-loaded refinement.
    for(auto& cluster:asset.clusters)cluster.page=(cluster.page&0x80000000u)?
        cluster.page&0x7fffffffu:cluster.page+unsigned(buckets[0].size());
    asset.group_pages.clear();
    for(auto& group:asset.groups) {
      std::set<unsigned> dependencies;
      for(unsigned i=0;i<group.cluster_count;++i)dependencies.insert(asset.clusters[group.first_cluster+i].page);
      group.first_page=unsigned(asset.group_pages.size());group.page_count=unsigned(dependencies.size());
      asset.group_pages.insert(asset.group_pages.end(),dependencies.begin(),dependencies.end());
    }
    for(const auto& raw:buckets)for(const auto& page:raw) {
      GeometryPage descriptor;
      std::vector<std::uint8_t> encoded(meshopt_encodeVertexBufferBound(page_bytes/4,4));
      const auto size=meshopt_encodeVertexBuffer(encoded.data(),encoded.size(),page.data(),page_bytes/4,4);
      require(size!=0,"geometry page compression failed");encoded.resize(size);
      if(size>=page_bytes)encoded=page;else descriptor.codec=GeometryCodec::Meshoptimizer;
      descriptor.encoded_size=unsigned(encoded.size());
      const auto hash=map_texture_digest(encoded);std::copy(hash.begin(),hash.end(),descriptor.checksum.begin());
      asset.pages.push_back(descriptor);asset.payloads.push_back(std::move(encoded));
    }
  }
};
}
bool cook_geometry(const MapModel& model,const std::string& source_hash,GeometryAsset& output,std::string& error,GeometryCookOptions options) {
  try {
    require(!model.vertices.empty() && !model.indices.empty() && !model.primitives.empty(),"geometry input empty");
    require(model.primitives.size()<invalid_id && model.indices.size()<invalid_id,"geometry input exceeds format limits");
    GeometryAsset asset;asset.source_hash=source_hash;asset.material_count=unsigned(model.primitives.size());
    asset.source_triangles=model.indices.size()/3;PageBuilder pages{asset};size_t expected=0;
    for(unsigned material=0;material<model.primitives.size();++material) {
      const auto& primitive=model.primitives[material];
      require(primitive.first_index==expected,"geometry primitives must partition indices");expected+=primitive.index_count;
      auto input=geometry_mesh(model,primitive,options.position_only);
      const float weights[15]={1,1,1,10,10,10,10,1,1,1,1,1,1,1,1};
      clodMesh mesh{};mesh.indices=input.indices.data();mesh.index_count=input.indices.size();
      mesh.vertex_count=input.vertices.size();mesh.vertex_positions=input.vertices.front().position;
      mesh.vertex_positions_stride=sizeof(MapVertex);mesh.vertex_attributes=input.attributes.front().data();
      mesh.vertex_attributes_stride=sizeof(input.attributes.front());mesh.attribute_weights=weights;mesh.attribute_count=15;
      if(options.position_only) {mesh.vertex_attributes=nullptr;mesh.attribute_weights=nullptr;mesh.attribute_count=0;}
      mesh.vertex_lock=input.locks.data();
      auto config=clodDefaultConfig(cluster_triangles);config.max_vertices=cluster_vertices;
      config.simplify_permissive=false;config.simplify_fallback_permissive=false;config.simplify_fallback_sloppy=false;
      config.simplify_error_merge_previous=1;config.simplify_error_merge_additive=1;
      clodBuild(config,mesh,[&](clodGroup source,const clodCluster* clusters,size_t count) {
        require(asset.groups.size()<INT_MAX && asset.clusters.size()+count<invalid_id,"geometry hierarchy too large");
        const auto id=unsigned(asset.groups.size());GeometryGroup group;
        group.first_cluster=unsigned(asset.clusters.size());group.cluster_count=unsigned(count);
        group.depth=unsigned(source.depth);group.simplified=bounds(source.simplified);
        std::set<unsigned> dependencies;
        for(size_t i=0;i<count;++i) {
          GeometryCluster cluster;cluster.group=id;cluster.material=material;cluster.bounds=bounds(clusters[i].bounds);
          cluster.refined_group=clusters[i].refined<0?invalid_id:unsigned(clusters[i].refined);
          cluster.flags=unsigned(primitive.material.alpha_mode)|(primitive.material.double_sided?256u:0u)|
              (options.position_only?geometry_position_only:0u);
          pages.append(cluster,clusters[i],input,source.simplified.error==FLT_MAX);
          dependencies.insert(cluster.page);asset.clusters.push_back(cluster);
        }
        group.first_page=unsigned(asset.group_pages.size());group.page_count=unsigned(dependencies.size());
        asset.group_pages.insert(asset.group_pages.end(),dependencies.begin(),dependencies.end());asset.groups.push_back(group);
        if(source.simplified.error==FLT_MAX)asset.roots.push_back(id);
        return int(id);
      });
    }
    require(expected==model.indices.size(),"geometry indices outside primitives");pages.finish();
    if(!validate_geometry(asset,error))return false;
    output=std::move(asset);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
