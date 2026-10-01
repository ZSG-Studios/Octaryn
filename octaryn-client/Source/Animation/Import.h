#pragma once
#include "AnimationAsset.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <cmath>
#include <stdexcept>
namespace octaryn::client::animation::importing {
inline void check(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
template<class T> std::vector<T> values(const fastgltf::Asset& asset,std::size_t index,fastgltf::AccessorType type,std::size_t limit) {
  check(index<asset.accessors.size(),"animation accessor index out of range");
  const auto& accessor=asset.accessors[index];check(accessor.type==type&&accessor.count<=limit,"animation accessor shape or count limit");
  std::vector<T> result;result.reserve(accessor.count);
  fastgltf::iterateAccessor<T>(asset,accessor,[&](T value) {result.push_back(value);});return result;
}
template<std::size_t N,class T> std::array<float,N> vector(const T& source) {
  std::array<float,N> result{};for(std::size_t c=0;c<N;++c) {result[c]=source[c];check(std::isfinite(result[c]),"nonfinite animation attribute");}return result;
}
void geometry(const fastgltf::Asset&,Asset&,const LoadLimits&);
void clips(const fastgltf::Asset&,Asset&,const LoadLimits&);
}
