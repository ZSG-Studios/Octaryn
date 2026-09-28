#include "GeometryPageCodec.h"
#include "../MapWorld/MapTextureCache.h"
#include <meshoptimizer.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <set>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void check(bool value,const std::string& error) {if(!value)throw std::runtime_error(error);}
bool digest_valid(std::string_view value) {
  return value.size()==64 && std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9') || (c>='a'&&c<='f');});
}
bool bounds_valid(const GeometryBounds& bounds) {
  return std::isfinite(bounds.center[0]) && std::isfinite(bounds.center[1]) && std::isfinite(bounds.center[2]) &&
      std::isfinite(bounds.radius) && bounds.radius>=0 && std::isfinite(bounds.error) && bounds.error>=0;
}
bool range(size_t start,size_t count,size_t size) {return start<=size && count<=size-start;}
}
bool decode_geometry_payload(const GeometryPage& page,std::span<const std::uint8_t> encoded,
    std::vector<std::uint8_t>& output,std::string& error) {
  output.clear();
  if(page.decoded_size!=page_bytes || !page.encoded_size || page.encoded_size>page_bytes || encoded.size()!=page.encoded_size ||
      map_texture_digest(encoded)!=std::string(page.checksum.data(),page.checksum.size())) {
    error="geometry page checksum or size mismatch";return false;
  }
  std::vector<std::uint8_t> decoded(page_bytes);
  if(page.codec==GeometryCodec::Raw && encoded.size()==page_bytes)std::copy(encoded.begin(),encoded.end(),decoded.begin());
  else if(page.codec!=GeometryCodec::Meshoptimizer ||
      meshopt_decodeVertexBuffer(decoded.data(),page_bytes/4,4,encoded.data(),encoded.size())!=0) {
    error="geometry page codec invalid";return false;
  }
  output=std::move(decoded);error.clear();return true;
}
bool decode_geometry_page(const GeometryAsset& asset,std::uint32_t page,std::vector<std::uint8_t>& output,std::string& error) {
  if(page>=asset.pages.size() || page>=asset.payloads.size()) {output.clear();error="geometry page not resident";return false;}
  return decode_geometry_payload(asset.pages[page],asset.payloads[page],output,error);
}
bool validate_geometry(const GeometryAsset& asset,std::string& error) {
  try {
    check(digest_valid(asset.source_hash),"geometry source digest invalid");
    check(asset.space==GeometrySpace::World || asset.space==GeometrySpace::Object,"geometry coordinate space invalid");
    check(asset.material_count && asset.source_triangles && !asset.clusters.empty() && !asset.groups.empty() &&
        !asset.roots.empty() && !asset.pages.empty(),"geometry manifest empty");
    check(asset.payloads.empty() || asset.payloads.size()==asset.pages.size(),"geometry payload table incomplete");
    for(const auto& page:asset.pages) {
      check(page.decoded_size==page_bytes && page.encoded_size && page.encoded_size<=page_bytes,"geometry page size invalid");
      check(page.codec==GeometryCodec::Raw || page.codec==GeometryCodec::Meshoptimizer,"geometry page codec unknown");
      check(page.codec!=GeometryCodec::Raw || page.encoded_size==page_bytes,"geometry raw page size invalid");
      check(digest_valid({page.checksum.data(),page.checksum.size()}),"geometry page digest invalid");
    }
    std::vector<unsigned> incoming(asset.groups.size());std::set<unsigned> roots;
    for(auto root:asset.roots)check(root<asset.groups.size() && roots.insert(root).second,"geometry root invalid or duplicated");
    size_t expected_cluster=0,expected_page=0;std::uint64_t source_triangles=0;
    for(size_t id=0;id<asset.groups.size();++id) {
      const auto& group=asset.groups[id];
      check(group.cluster_count && group.first_cluster==expected_cluster && range(group.first_cluster,group.cluster_count,asset.clusters.size()),
          "geometry group cluster range invalid");
      check(group.page_count && group.first_page==expected_page && range(group.first_page,group.page_count,asset.group_pages.size()),
          "geometry group page range invalid");
      check(bounds_valid(group.simplified),"geometry group bounds invalid");
      check((group.simplified.error==FLT_MAX)==roots.contains(unsigned(id)),"geometry terminal root mismatch");
      std::set<unsigned> pages;
      const auto material=asset.clusters[group.first_cluster].material;
      for(size_t c=group.first_cluster;c<size_t(group.first_cluster)+group.cluster_count;++c) {
        const auto& cluster=asset.clusters[c];
        check(cluster.group==id && cluster.material==material && cluster.material<asset.material_count,"geometry cluster owner invalid");
        check((cluster.flags&~259u)==0 && (cluster.flags&3u)<=2,"geometry cluster material flags invalid");
        check(cluster.page<asset.pages.size() && cluster.vertex_count && cluster.vertex_count<=cluster_vertices &&
            cluster.triangle_count && cluster.triangle_count<=cluster_triangles,"geometry cluster counts invalid");
        check(cluster.vertex_offset%16==0 && range(cluster.vertex_offset,cluster.vertex_count*sizeof(MapVertex),page_bytes) &&
            cluster.triangle_offset==cluster.vertex_offset+cluster.vertex_count*sizeof(MapVertex) &&
            range(cluster.triangle_offset,cluster.triangle_count*4,page_bytes),"geometry cluster page range invalid");
        check(bounds_valid(cluster.bounds),"geometry cluster bounds invalid");
        pages.insert(cluster.page);
        if(cluster.refined_group==invalid_id)source_triangles+=cluster.triangle_count;
        else {
          check(cluster.refined_group<id,"geometry refinement cycle or invalid reference");
          const auto& refined=asset.groups[cluster.refined_group];
          check(refined.depth<group.depth && refined.simplified.error<=group.simplified.error,"geometry hierarchy is not monotonic");
          check(asset.clusters[refined.first_cluster].material==material,"geometry refinement crosses materials");
          ++incoming[cluster.refined_group];
        }
      }
      check(pages.size()==group.page_count && std::equal(pages.begin(),pages.end(),asset.group_pages.begin()+group.first_page),
          "geometry group residency dependencies incomplete");
      expected_cluster+=group.cluster_count;expected_page+=group.page_count;
    }
    check(expected_cluster==asset.clusters.size() && expected_page==asset.group_pages.size(),"geometry orphan metadata");
    check(source_triangles==asset.source_triangles,"geometry leaf triangle coverage mismatch");
    for(size_t i=0;i<incoming.size();++i)check((incoming[i]==0)==roots.contains(unsigned(i)),"geometry root reachability invalid");
    std::vector<std::vector<unsigned>> page_clusters(asset.pages.size());
    for(unsigned c=0;c<asset.clusters.size();++c)page_clusters[asset.clusters[c].page].push_back(c);
    if(!asset.payloads.empty())for(unsigned p=0;p<asset.pages.size();++p) {
      std::vector<std::uint8_t> decoded;check(decode_geometry_page(asset,p,decoded,error),error);
      std::vector<std::pair<unsigned,unsigned>> ranges;
      for(const auto c:page_clusters[p]) {
        const auto& cluster=asset.clusters[c];
        ranges.emplace_back(cluster.vertex_offset,cluster.triangle_offset+cluster.triangle_count*4);
        for(unsigned i=0;i<cluster.vertex_count;++i) {
          MapVertex vertex;std::memcpy(&vertex,decoded.data()+cluster.vertex_offset+i*sizeof(MapVertex),sizeof(vertex));
          double distance=0;
          for(unsigned axis=0;axis<3;++axis) {
            check(std::isfinite(vertex.position[axis]),"geometry decoded vertex nonfinite");
            const double d=double(vertex.position[axis])-cluster.bounds.center[axis];distance+=d*d;
          }
          check(std::sqrt(distance)<=double(cluster.bounds.radius)+1e-4,"geometry cluster bounds do not contain vertices");
        }
        for(unsigned i=0;i<cluster.triangle_count;++i) {
          std::uint32_t triangle;std::memcpy(&triangle,decoded.data()+cluster.triangle_offset+i*4,4);
          check((triangle>>24)==0 && (triangle&255)<cluster.vertex_count && ((triangle>>8)&255)<cluster.vertex_count &&
              ((triangle>>16)&255)<cluster.vertex_count,"geometry decoded local index invalid");
        }
      }
      std::sort(ranges.begin(),ranges.end());
      for(size_t i=1;i<ranges.size();++i)check(ranges[i-1].second<=ranges[i].first,"geometry cluster payloads overlap");
    }
    error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
