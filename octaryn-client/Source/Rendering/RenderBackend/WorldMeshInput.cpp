#include "WorldRendererInternal.h"
#include "WorldMeshInput.h"
namespace octaryn::client::rendering {
void world_mesh_halo_into(const WorldRenderer& r,const world_presentation::StreamColumn& source,
    std::vector<std::uint32_t>& blocks,world_presentation::ColumnOrigins* preloaded) {
  if(preloaded)*preloaded={};
  blocks.resize(34u*34u*static_cast<unsigned>(source.height));
  // The center 32x32 columns are overwritten below.  Only the one-cell
  // neighbor border can retain data from a prior job, so clear that border
  // instead of touching the full 34x34 working slice for every Y row.
  // Unavailable input is air and must not inherit old values.
  const auto height=static_cast<unsigned>(source.height);
  for(unsigned y=0;y<height;++y) for(unsigned z=0;z<34;++z) {
    auto* row=blocks.data()+34u*(y+height*z);
    if(z==0 || z==33) std::fill_n(row,34u,0u);
    else { row[0]=0u; row[33]=0u; }
  }
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
    if(!neighbor) {
      const auto neighbor_x=std::int64_t(source.x)+dx,neighbor_z=std::int64_t(source.z)+dz;
      if(std::abs(neighbor_x-r.center_x)>r.radius || std::abs(neighbor_z-r.center_z)>r.radius)continue;
      const auto& halo=source.mesh_halo;
      const auto index=world_presentation::column_neighbor_index(dx,dz);
      if(!halo || halo->min_y!=source.min_y || halo->height!=source.height ||
          halo->blocks.size()!=132u*height || !halo->neighbors[index])continue;
      for(int y=0;y<source.height;++y)
        blocks[static_cast<std::size_t>(x+1)+34u*(static_cast<unsigned>(y)+height*static_cast<unsigned>(z+1))]=
            halo->blocks[world_presentation::column_halo_index(x,y,z,source.height)];
      if(preloaded)(*preloaded)[index]=halo->neighbors[index];
      continue;
    }
    const int local_x=(x+32)%32,local_z=(z+32)%32;
    for(int y=0;y<source.height;++y) {
      const int local_y=source.min_y+y-neighbor->min_y;
      if(local_y<0 || local_y>=neighbor->height) continue;
      blocks[static_cast<std::size_t>(x+1)+34u*(static_cast<unsigned>(y)+static_cast<unsigned>(source.height)*static_cast<unsigned>(z+1))]=
          neighbor->blocks[static_cast<std::size_t>(local_x)+32u*(static_cast<unsigned>(local_y)+static_cast<unsigned>(neighbor->height)*static_cast<unsigned>(local_z))];
    }
  }
}
void world_mesh_halo_into(const WorldRenderer& r,const world_presentation::StreamColumn& source,
    std::vector<std::uint32_t>& blocks) {
  world_mesh_halo_into(r,source,blocks,nullptr);
}
std::vector<std::uint32_t> world_mesh_halo(const WorldRenderer& r,const world_presentation::StreamColumn& source) {
  std::vector<std::uint32_t> blocks;
  world_mesh_halo_into(r,source,blocks);
  return blocks;
}
}
