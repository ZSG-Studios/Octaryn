#pragma once
#include <cstddef>
#include <cmath>
#include <stdexcept>
namespace octaryn::client::rendering {
// A declared collapsed tangent plane is authored data, never a request for
// derivative or Mikk reconstruction. This bounded contract is triangle-local.
template<class Normals,class Uvs,class Tangents,class Indices>
void validate_map_zero_basis(std::size_t count,const Normals& normals,const Uvs& uv,
    const Tangents& tangents,const Tangents& bitangents,const Indices& indices) {
  const auto require=[](bool valid,const char* reason){if(!valid)throw std::runtime_error(reason);};
  require(count && normals.size()==count && uv.size()==count && tangents.size()==count && bitangents.size()==count,
      "zero-basis attributes must have matching source cardinality");
  for(std::size_t i=0;i<count;++i) {
    double nn=0;
    for(unsigned axis=0;axis<3;++axis) {
      require(std::isfinite(normals[i][axis]) && tangents[i][axis]==0 && bitangents[i][axis]==0,
          "zero-basis declaration requires finite normal and exact zero authored T/B");
      nn+=double(normals[i][axis])*normals[i][axis];
    }
    require(nn>=1e-16,"zero-basis source normal must be nonzero");
    for(unsigned axis=0;axis<2;++axis)require(std::isfinite(uv[i][axis]),"nonfinite zero-basis UV");
  }
  require(indices.size()%3==0,"zero-basis requires indexed triangles");
  for(std::size_t corner=0;corner<indices.size();corner+=3) {
    const auto a=indices[corner],b=indices[corner+1],c=indices[corner+2];
    require(a<count && b<count && c<count,"zero-basis source index outside attributes");
    const double determinant=(double(uv[b][0])-uv[a][0])*(double(uv[c][1])-uv[a][1])-
        (double(uv[c][0])-uv[a][0])*(double(uv[b][1])-uv[a][1]);
    require(determinant==0,"zero-basis source triangle must have degenerate authored UVs");
  }
}
}
