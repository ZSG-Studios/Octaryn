#include "TileCook.h"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace octaryn::tools::tiles {
void order_triangles(const MapModel& model,std::vector<Triangle>& triangles,
    const std::array<int,3>& cell,const Settings& settings) {
  struct Ordered {std::uint64_t key;Triangle triangle;};
  std::vector<Ordered> ordered;ordered.reserve(triangles.size());
  constexpr std::uint32_t bins=1u<<21;
  for(const auto triangle:triangles) {
    std::uint64_t key=0;
    for(unsigned axis=0;axis<3;++axis) {
      double center=0;
      for(unsigned corner=0;corner<3;++corner)
        center+=model.vertices.at(model.indices.at(triangle.first+corner)).position[axis]/3.;
      const double local=center/settings.cell-cell[axis];
      const auto coordinate=static_cast<std::uint32_t>(std::clamp(std::floor(local*bins),0.,double(bins-1)));
      for(unsigned bit=0;bit<21;++bit)key|=std::uint64_t((coordinate>>bit)&1)<<(bit*3+axis);
    }
    ordered.push_back({key,triangle});
  }
  std::sort(ordered.begin(),ordered.end(),[](const Ordered& a,const Ordered& b) {
    return std::tie(a.key,a.triangle.primitive,a.triangle.first)<std::tie(b.key,b.triangle.primitive,b.triangle.first);
  });
  for(size_t i=0;i<triangles.size();++i)triangles[i]=ordered[i].triangle;
}
}
