#include "WorldRendererInternal.h"
#include "WorldStream.h"
namespace octaryn::client::rendering {
std::vector<std::uint32_t> world_mesh_halo(const WorldRenderer& r,const world_presentation::StreamColumn& source) {
  std::vector<std::uint32_t> blocks(34u*34u*static_cast<unsigned>(source.height));
  for(int z=0;z<32;++z) for(int y=0;y<source.height;++y) {
    const auto input=32u*(static_cast<unsigned>(y)+static_cast<unsigned>(source.height)*static_cast<unsigned>(z));
    const auto output=1u+34u*(static_cast<unsigned>(y)+static_cast<unsigned>(source.height)*static_cast<unsigned>(z+1));
    source.blocks.read_range(input,std::span<std::uint32_t>(blocks).subspan(output,32));
  }
  // Retain authoritative voxel columns, never CPU mesh geometry. One-cell halos
  // give the GPU the original eight-neighbor fluid corner/flow sampling contract.
  for(int z=-1;z<=32;++z) for(int x=-1;x<=32;++x) {
    const int dx=x<0?-1:x>=32?1:0,dz=z<0?-1:z>=32?1:0;
    if(dx==0 && dz==0) continue;
    const auto found=r.sources.find({source.x+dx,source.z+dz});
    const auto* neighbor=found==r.sources.end()?nullptr:&found->second;
    if(!neighbor) continue;
    const int local_x=(x+32)%32,local_z=(z+32)%32;
    for(int y=0;y<source.height;++y) {
      const int local_y=source.min_y+y-neighbor->min_y;
      if(local_y<0 || local_y>=neighbor->height) continue;
      blocks[static_cast<std::size_t>(x+1)+34u*(static_cast<unsigned>(y)+static_cast<unsigned>(source.height)*static_cast<unsigned>(z+1))]=
          neighbor->blocks[static_cast<std::size_t>(local_x)+32u*(static_cast<unsigned>(local_y)+static_cast<unsigned>(neighbor->height)*static_cast<unsigned>(local_z))];
    }
  }
  return blocks;
}
}
