#include "WorldRendererInternal.h"
#include "DDGIOccupancy.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
namespace octaryn::client::rendering {
namespace {
constexpr std::uint8_t Needed=0,Solid=1,Sky=2;
constexpr int SkyMargin=6;
int floor_div(int value,int size) {return value/size-(value%size<0);}
std::uint16_t source_block(const WorldRenderer& r,int x,int y,int z,bool* known=nullptr) {
  const int cx=floor_div(x,32),cz=floor_div(z,32);
  const auto found=r.sources.find({cx,cz});
  if(found==r.sources.end()) {if(known)*known=false;return 0;}
  const auto& column=found->second;const int local_y=y-column.min_y;
  if(local_y<0 || local_y>=column.height) {if(known)*known=false;return 0;}
  if(known)*known=true;
  return column.blocks[static_cast<std::size_t>(x-cx*32)+32u*(static_cast<unsigned>(local_y)+
      static_cast<unsigned>(column.height)*static_cast<unsigned>(z-cz*32))];
}
std::array<int,3> probe_voxel(const DDGISystem& s,const std::array<int,3>& cell) {
  const float anchor=s.cell_centered?.5f:0.f;
  const float extra=s.cell_centered?0.f:s.config.spacing*.5f;
  std::array<int,3> voxel{};
  for(unsigned axis=0;axis<3;++axis)
    voxel[axis]=int(std::floor((float(cell[axis])+anchor)*s.config.spacing+extra));
  return voxel;
}
bool gi_solid(const WorldRenderer& r,std::uint16_t block) {
  if(block==0)return false;
  // Transparent blocks host probes: water, glass, leaves, sprites and clouds
  // transmit or scatter light, so probes must exist inside them. Lava and
  // occluding blocks displace probes exactly like the RT visibility rules.
  const auto flags=world_atlas_flags(r.atlas);
  if(block>=flags.size())return true;
  return (flags[block]&1)!=0 || (flags[block]&512)!=0;
}
}
bool ddgi_ignore_covers(const DDGISystem& s,std::int32_t column_x,std::int32_t column_z) {
  return s.ignore_active && floor_div(s.ignore_voxel[0],32)==column_x &&
    floor_div(s.ignore_voxel[2],32)==column_z;
}
void ddgi_clear_ignore(DDGISystem& s) {
  if(!s.ignore_active)return;
  s.ignore_active=false;s.ignore_held=false;s.ignore_released=0;
  s.ignore_voxel={std::numeric_limits<int>::max(),std::numeric_limits<int>::max(),
    std::numeric_limits<int>::max()};
}
void ddgi_follow_opening(WorldRenderer& r,DDGISystem& s) {
  const auto& target=r.selection;
  const bool held=target.opening!=0 && target.face<6;
  if(held) {
    const std::array<int,3> voxel{target.x,target.y,target.z};
    if(!s.ignore_active || s.ignore_voxel!=voxel) {
      s.ignore_voxel=voxel;s.ignore_active=true;s.ignore_revision=r.scene_changes.revision();
    }
    s.ignore_held=true;s.ignore_released=0;return;
  }
  if(!s.ignore_active)return;
  s.ignore_held=false;++s.ignore_released;
  if(source_block(r,s.ignore_voxel[0],s.ignore_voxel[1],s.ignore_voxel[2])==0)return;
  if(s.ignore_released>1 && r.scene_changes.revision()==s.ignore_revision)ddgi_clear_ignore(s);
}
void ddgi_classify_occupancy(WorldRenderer& r,DDGISystem& s) {
  if(s.occupancy.size()!=s.control_data.size())s.occupancy.assign(s.control_data.size(),Needed);
  bool controls=false;
  for(unsigned i=0;i<s.control_data.size();++i) {
    const auto voxel=probe_voxel(s,s.control_data[i].cell);
    const bool hole=s.ignore_active && voxel==s.ignore_voxel;
    bool known=false;
    const std::uint16_t block=source_block(r,voxel[0],voxel[1],voxel[2],&known);
    const bool still_solid=hole && block!=0;
    std::uint8_t kind=Needed;
    if(still_solid) kind=Needed;
    else if(gi_solid(r,block)) kind=Solid;
    else if(!known) kind=Needed;
    else {
      bool nearby=false;
      for(int down=1;down<=SkyMargin && !nearby;++down)
        nearby=source_block(r,voxel[0],voxel[1]-down,voxel[2])!=0;
      const int n[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
      for(auto* step:n)if(source_block(r,voxel[0]+step[0],voxel[1]+step[1],voxel[2]+step[2])!=0)nearby=true;
      kind=nearby?Needed:Sky;
    }
    const std::uint8_t previous=s.occupancy[i];
    const std::uint32_t seed_only=still_solid?1u:0u;
    const bool was_seed_only=s.control_data[i].padding[1]!=0;
    if(previous!=kind || s.control_data[i].padding[0]!=(kind==Solid?1u:0u) ||
       s.control_data[i].padding[1]!=seed_only) {
      s.occupancy[i]=kind;
      // Only solid interiors leave the 8-probe cage. Sky probes still hold
      // environment irradiance; RTXGI never removes them from interpolation.
      s.control_data[i].padding[0]=kind==Solid?1u:0u;
      // padding.y keeps the speculative hole in the cage and seeded from
      // neighbors, but out of the trace budget until the voxel is empty.
      s.control_data[i].padding[1]=seed_only;
      if((kind==Needed && previous==Solid && !still_solid) || (was_seed_only && !seed_only))
        s.dirty[i]=true;
      controls=true;
    }
  }
  if(controls)s.controls_dirty=true;
}
}
