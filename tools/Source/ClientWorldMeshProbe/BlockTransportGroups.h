#pragma once
#include <array>
#include <string_view>

namespace mesh_probe {
struct BlockTransportGroup {const char* name;unsigned pipelines;bool rays,masks;};
inline constexpr std::array<BlockTransportGroup,14> BlockTransportGroups{{
  {"cache",7,false,false},{"admission",3,false,true},{"room",9,true,true},
  {"raster",1,false,false},{"rays",6,true,true},{"plants",6,true,true},
  {"numerical",2,false,false},{"sampling",1,false,true},{"convergence",8,true,true},
  {"dynamic",3,true,true},{"leaf",6,true,true},{"actor",8,true,true},{"boundary",1,true,true},{"local-area",5,true,true}
}};
inline const BlockTransportGroup* block_transport_group(std::string_view name) {
  for(const auto& group:BlockTransportGroups)if(name==group.name)return &group;
  return nullptr;
}
}
