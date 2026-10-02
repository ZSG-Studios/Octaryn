#include "Simplify.h"
#include <meshoptimizer.h>
#include <algorithm>
#include <array>
#include <cstdio>

namespace octaryn::client::rendering {
MapLodData cook_map_lods(const MapModel& model) {
  MapLodData output;output.primitives.resize(model.primitives.size());
  std::uint64_t reduced[2]{},original{};
  for(size_t p=0;p<model.primitives.size();++p) {
    const auto& primitive=model.primitives[p];original+=primitive.index_count;
    if(primitive.material.alpha_mode!=MapAlphaMode::Opaque || primitive.index_count<96) {
      reduced[0]+=primitive.index_count;reduced[1]+=primitive.index_count;continue;
    }
    const auto begin=model.indices.begin()+primitive.first_index,end=begin+primitive.index_count;
    const auto bounds=std::minmax_element(begin,end);const unsigned base=*bounds.first,count=*bounds.second-base+1;
    std::vector<unsigned> input;input.reserve(primitive.index_count);
    for(auto it=begin;it!=end;++it)input.push_back(*it-base);
    // Attribute-aware collapse preserves seams; independent primitive borders remain locked.
    std::vector<std::array<float,19>> attributes(count);
    for(unsigned i=0;i<count;++i) {
      const auto& vertex=model.vertices[base+i];auto& a=attributes[i];
      std::copy_n(vertex.normal,3,a.begin());std::copy_n(vertex.uv,2,a.begin()+3);
      std::copy_n(vertex.uv1,2,a.begin()+5);std::copy_n(vertex.color,4,a.begin()+7);
      std::copy_n(vertex.blend0,4,a.begin()+11);std::copy_n(vertex.blend1,4,a.begin()+15);
    }
    const float weights[19]={1,1,1,10,10,10,10,1,1,1,1,10,10,10,10,10,10,10,10};
    const auto* positions=model.vertices[base].position;
    const float scale=meshopt_simplifyScale(positions,count,sizeof(MapVertex));
    for(unsigned level=0;level<2;++level) {
      std::vector<unsigned> indices(input.size());float error{};
      const size_t target=std::max<size_t>(3,(input.size()>>(level+1))/3*3);
      const size_t result=meshopt_simplifyWithAttributes(indices.data(),input.data(),input.size(),positions,count,sizeof(MapVertex),
          attributes.front().data(),sizeof(attributes.front()),weights,19,nullptr,target,level==0?.002f:.008f,
          meshopt_SimplifyLockBorder,&error);
      if(result>=input.size() || result<3) {reduced[level]+=primitive.index_count;continue;}
      indices.resize(result);meshopt_optimizeVertexCache(indices.data(),indices.data(),indices.size(),count);
      auto& destination=output.primitives[p][level];destination.first=static_cast<unsigned>(output.indices.size());
      destination.count=static_cast<unsigned>(indices.size());destination.error=error*scale;
      for(auto index:indices)output.indices.push_back(index+base);
      reduced[level]+=indices.size();
    }
  }
  std::printf("map_lod_cook primitives=%zu triangles_reference=%llu triangles_lod1=%llu triangles_lod2=%llu added_index_bytes=%zu\n",
      model.primitives.size(),static_cast<unsigned long long>(original/3),static_cast<unsigned long long>(reduced[0]/3),
      static_cast<unsigned long long>(reduced[1]/3),output.indices.size()*4);
  return output;
}
}
