#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace octaryn::client::rendering {
// The production static_assert pins Rml::Vertex to position8/color4/UV8 bytes.
struct RmlGeometryKey {
  std::vector<std::byte> vertices,indices;
  std::array<float,4> tint{1,1,1,1};
  std::uint64_t hash{};
};
inline bool canonical_rml_geometry(std::span<const std::byte> vertices,
                                   std::span<const std::byte> indices,RmlGeometryKey& key) {
  if(vertices.empty() || vertices.size()%20 || indices.empty() || indices.size()%4 ||
      vertices.size()+indices.size()>256*1024)return false;
  bool uniform=true;
  for(std::size_t offset=20;offset<vertices.size();offset+=20)
    for(unsigned channel=0;channel<4;++channel)
      uniform &= vertices[offset+8+channel]==vertices[8+channel];
  key.vertices.assign(vertices.begin(),vertices.end());
  key.indices.assign(indices.begin(),indices.end());
  key.tint={1,1,1,1};
  if(uniform)for(unsigned channel=0;channel<4;++channel)
    key.tint[channel]=float(std::to_integer<unsigned char>(vertices[8+channel]))/255.f;
  if(uniform)for(std::size_t offset=0;offset<vertices.size();offset+=20)
    for(unsigned channel=0;channel<4;++channel)key.vertices[offset+8+channel]=std::byte{255};
  key.hash=14695981039346656037ull;
  const auto append=[&](std::span<const std::byte> bytes) {
    for(const auto byte:bytes){key.hash^=std::to_integer<unsigned char>(byte);key.hash*=1099511628211ull;}
    key.hash^=bytes.size();key.hash*=1099511628211ull;
  };
  append(key.vertices);append(key.indices);
  return true;
}
}
