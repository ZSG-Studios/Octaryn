#include "WorldRendererInternal.h"
#include "StreamSnapshot.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>

namespace {
using namespace octaryn::client::rendering;
using octaryn::client::world_presentation::StreamColumn;
using Coordinate=std::pair<std::int32_t,std::int32_t>;
using Halos=std::map<Coordinate,std::vector<std::uint32_t>>;
unsigned checks{};
void expect(bool result,const char* message) {++checks;if(!result) throw std::runtime_error(message);}
Coordinate center{-3,-5};
StreamColumn column(Coordinate coordinate,int height=3) {
  StreamColumn result;result.x=coordinate.first;result.z=coordinate.second;
  result.min_y=-2;result.height=height;result.revision=17;
  result.blocks.resize(static_cast<std::size_t>(height)*32*32);return result;
}
void initialize(WorldRenderer& r,int height=3) {
  for(int dz=-1;dz<=1;++dz) for(int dx=-1;dx<=1;++dx) {
    const Coordinate at{center.first+dx,center.second+dz};
    r.sources.emplace(at,column(at,height));world_renderer_store_column(r,at,{});
  }
}
void change(StreamColumn& source,int x,int y,int z) {
  source.blocks[static_cast<std::size_t>(x+32*(y+source.height*z))]=14;
}
Halos halos(const WorldRenderer& r,bool neighbors_only=true) {
  Halos result;
  for(const auto& [at,source]:r.sources) {
    if(neighbors_only && at==center) continue;
    result.emplace(at,world_mesh_halo(r,source));
  }
  return result;
}
std::set<Coordinate> recipients(const WorldRenderer& r) {
  std::set<Coordinate> result;
  for(const auto& [at,source]:r.sources) {
    (void)source;
    if(at!=center && std::abs(at.first-center.first)<=1 && std::abs(at.second-center.second)<=1)
      result.insert(at);
  }
  return result;
}
std::size_t verify(WorldRenderer& r,const StreamColumn& next,bool exact=true) {
  const auto before=halos(r);
  const auto prior=r.dirty;
  const auto old=r.sources.find(center);
  const auto old_bytes=old==r.sources.end()?octaryn::client::world_presentation::ColumnBlocks{}:old->second.blocks;
  world_mesh_invalidate_neighbors(r,next);
  if(old!=r.sources.end()) expect(old->second.blocks==old_bytes,"invalidation must not mutate retained source");
  for(const auto& at:prior) expect(r.dirty.contains(at),"invalidation must preserve pending dirty work");
  r.sources.insert_or_assign(center,next);
  const auto after=halos(r);
  std::set<Coordinate> changed;
  for(const auto& [at,bytes]:before) if(bytes!=after.at(at)) changed.insert(at);
  for(const auto& at:changed) expect(r.dirty.contains(at),"changed production halo was skipped");
  auto expected=prior;
  if(exact) expected.insert(changed.begin(),changed.end());
  else {const auto all=recipients(r);expected.insert(all.begin(),all.end());}
  expect(r.dirty==expected,"dirty neighbors disagree with production halo dependency oracle");
  return changed.size();
}
void all_horizontal_positions() {
  WorldRenderer r;initialize(r);
  const auto original=r.sources.at(center);
  for(int z=0;z<32;++z) for(int x=0;x<32;++x) {
    auto next=original;change(next,x,1,z);next.revision=31;
    r.dirty.clear();
    const auto changed=verify(r,next);
    const unsigned edges=static_cast<unsigned>(x==0 || x==31)+static_cast<unsigned>(z==0 || z==31);
    expect(changed==(edges==2?3u:edges),"interior/edge/corner recipient count");
    r.sources.at(center)=original;
  }
  // Revision values are content identities, not sufficient boundary-change evidence.
  auto identical=original;identical.revision=999;identical.epoch=777;
  r.dirty.clear();expect(verify(r,identical)==0,"changed metadata alone must not rebuild neighbors");
  r.dirty={{99,-99},{center.first-1,center.second}};
  auto interior=identical;change(interior,15,1,15);
  expect(verify(r,interior)==0,"interior edit must retain existing dirty set without adding neighbors");
  r.dirty.clear();auto perimeter=interior;
  for(int z=0;z<32;++z) for(int x=0;x<32;++x)
    if(x==0 || x==31 || z==0 || z==31) change(perimeter,x,2,z);
  expect(verify(r,perimeter)==8,"changed complete boundary must reach all eight neighbors");
}
void vertical_and_lifecycle() {
  WorldRenderer r;initialize(r);
  for(int y:{0,2}) {
    const auto original=r.sources.at(center);auto next=original;
    change(next,0,y,0);r.dirty.clear();expect(verify(r,next)==3,"corner changes at first/last Y must reach diagonal");
    r.sources.at(center)=original;
  }
  auto shifted=r.sources.at(center);shifted.min_y+=1;r.dirty.clear();verify(r,shifted,false);
  auto taller=column(center,4);r.dirty.clear();verify(r,taller,false);
  // Recipients may have different vertical intervals; all matching world-Y halo
  // samples must still be dirtied (a local row index is not a world-height key).
  for(auto& [at,source]:r.sources) if(at!=center) source.min_y+=1;
  auto next=taller;change(next,31,3,31);r.dirty.clear();verify(r,next);
  r.sources.erase(center);r.columns.erase(center);r.dirty.clear();verify(r,column(center),false);
  r.sources.erase({center.first-1,center.second-1});r.columns.erase({center.first-1,center.second-1});
  next=r.sources.at(center);change(next,0,1,0);r.dirty.clear();
  expect(verify(r,next)==2,"missing diagonal recipient must not acquire phantom dirty work");
  // Malformed new or retained payloads must take the conservative path without
  // letting this oracle call world_mesh_halo on malformed input.
  for(bool invalid_old:{false,true}) {
    const auto original=r.sources.at(center);next=original;
    if(invalid_old) r.sources.at(center).blocks.pop_back();else next.blocks.pop_back();
    r.dirty.clear();world_mesh_invalidate_neighbors(r,next);
    expect(r.dirty==recipients(r),"invalid column shape must conservatively invalidate all resident neighbors");
    r.sources.at(center)=original;
  }
  WorldRenderer full;initialize(full,512);next=full.sources.at(center);change(next,31,511,16);
  expect(verify(full,next)==1,"top row of full-height column participates in boundary comparison");
}
void unload() {
  WorldRenderer r;initialize(r);
  // Give every retiring boundary observable input, including diagonal samples.
  for(auto& [at,source]:r.sources) {
    (void)at;source.blocks.fill(5);
  }
  const auto before=halos(r,false);
  open_world_renderer_set_center(&r,center.first+1,center.second+1,1);
  const auto after=halos(r,false);
  expect(r.sources.size()==4 && r.columns.size()==4,"unload fixture must retire five columns");
  unsigned changed=0;
  for(const auto& [at,bytes]:after) if(bytes!=before.at(at)) {
    ++changed;expect(r.dirty.contains(at),"unload skipped a surviving changed halo");
  }
  expect(changed==3,"unload must affect cardinal and diagonal surviving recipients");
  for(const auto& at:r.dirty) expect(r.sources.contains(at),"unloaded coordinates must not remain dirty");
}
void scheduling() {
  WorldRenderer r;initialize(r);r.center_x=center.first;r.center_z=center.second;r.radius=32;
  const Coordinate far{-30,-30},near{center.first+1,center.second};
  r.sources.emplace(far,column(far));world_renderer_store_column(r,far,{});
  const auto fill_neighborhood=[&](Coordinate at) {
    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
      const Coordinate neighbor{at.first+dx,at.second+dz};
      if(!r.sources.contains(neighbor)) {
        r.sources.emplace(neighbor,column(neighbor));world_renderer_store_column(r,neighbor,{});
      }
    }
  };
  fill_neighborhood(far);
  r.dirty={far,center,near};
  auto edited=r.sources.at(center);change(edited,31,1,16);
  world_mesh_invalidate_neighbors(r,edited);
  expect(r.dirty_urgent==std::set<Coordinate>{near},"boundary edit promotes already queued neighbor only");
  Coordinate selected;
  expect(world_mesh_take_pending(r,selected) && selected==near,
      "edited boundary outranks closer ordinary work and lexicographically earlier distant work");
  expect(r.dirty==std::set<Coordinate>{far,center} && r.dirty_urgent.empty(),
      "selection retains every unselected pending mesh and consumes urgent state");
  expect(world_mesh_take_pending(r,selected) && selected==center,"ordinary arrivals prioritize proximity");
  expect(world_mesh_take_pending(r,selected) && selected==far,"distant work is preserved and eventually selected");
  expect(!world_mesh_take_pending(r,selected),"empty pending queue has no phantom work");
  const Coordinate arrival{center.first+2,center.second};
  r.dirty.insert(far);world_mesh_invalidate_neighbors(r,column(arrival));
  expect(r.dirty_urgent.empty(),"new residency does not acquire edit priority");
  expect(!r.sources.contains(arrival),"arrival fixture must begin with unknown neighbor data");
  fill_neighborhood(near);
  expect(world_mesh_take_pending(r,selected) && selected==near,"new-arrival nearby halo outranks distant backlog");
  r.dirty.clear();r.dirty_urgent.clear();
  world_mesh_invalidate_neighbors(r,r.sources.at(center));
  expect(r.dirty.empty() && r.dirty_urgent.empty(),"unchanged existing boundaries do not queue or promote work");
  r.dirty={far,near};r.dirty_urgent=r.dirty;
  expect(world_mesh_take_pending(r,selected) && selected==near,"urgent work also prioritizes proximity");
  world_renderer_store_column(r,far,{});
  expect(r.dirty.empty() && r.dirty_urgent.empty(),"direct replacement consumes queued and urgent work together");
  const Coordinate tie_x{center.first-1,center.second},tie_z{center.first,center.second-1};
  fill_neighborhood(tie_x);fill_neighborhood(tie_z);
  r.dirty={tie_z,tie_x};
  expect(world_mesh_take_pending(r,selected) && selected==tie_x,"equal distances use deterministic coordinate order");
  r.dirty={far,near};r.dirty_urgent=r.dirty;
  open_world_renderer_set_center(&r,100,100,0);
  expect(r.sources.empty() && r.dirty.empty() && r.dirty_urgent.empty(),"unload clears urgent state with residency");
  r.dirty.insert(far);r.dirty_urgent={far,near};
  expect(!world_mesh_take_pending(r,selected) && r.dirty.empty() && r.dirty_urgent.empty(),
      "orphaned dirty and urgent entries are cleaned without phantom selection");
}
void readiness() {
  WorldRenderer r;initialize(r);r.center_x=center.first;r.center_z=center.second;r.radius=1;
  const Coordinate missing{center.first+1,center.second+1};
  const auto saved=r.sources.at(missing);r.sources.erase(missing);
  r.dirty.insert(center);Coordinate selected;
  expect(!world_mesh_take_pending(r,selected) && r.dirty.contains(center),
      "ordinary halo preserves pending work until the last diagonal source arrives");
  r.dirty_urgent.insert(center);
  expect(world_mesh_take_pending(r,selected) && selected==center,
      "urgent edit or unload bypasses an incomplete expected neighborhood");
  r.dirty.insert(center);r.sources.emplace(missing,saved);
  expect(world_mesh_take_pending(r,selected) && selected==center,
      "last expected source releases a coalesced ordinary halo");
  const Coordinate edge{center.first+1,center.second};
  r.radius=2;r.dirty.insert(edge);
  expect(!world_mesh_take_pending(r,selected) && r.dirty.contains(edge),
      "grown window waits for newly expected neighbors");
  r.radius=1;
  expect(world_mesh_take_pending(r,selected) && selected==edge,
      "shrinking the window treats outside neighbors as absent without stale wait state");
  r.dirty.insert(edge);++r.center_x;
  expect(!world_mesh_take_pending(r,selected) && r.dirty.contains(edge),
      "moved window retains ordinary work while new neighboring sources are missing");
  --r.center_x;
  expect(world_mesh_take_pending(r,selected) && selected==edge,
      "center reversal re-evaluates readiness without stale waiting state");
}
void selection_cost() {
  WorldRenderer r;
  for(int z=-32;z<=32;++z) for(int x=-32;x<=32;++x) {
    r.sources.try_emplace({x,z});r.dirty.emplace(x,z);
  }
  Coordinate selected;
  constexpr unsigned iterations=64;
  const auto start=std::chrono::steady_clock::now();
  for(unsigned i=0;i<iterations;++i) {
    expect(world_mesh_take_pending(r,selected) && selected==Coordinate{0,0},"maximum window chooses nearest pending column");
    r.dirty.insert(selected);
  }
  const double elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
  expect(r.dirty.size()==4225,"maximum window retains bounded complete pending set");
  std::printf("world_mesh_selection columns=4225 samples=%u mean_us=%.3f gpu_devices=0\n",iterations,elapsed/iterations);
}
std::vector<std::uint32_t> scalar_halo(const WorldRenderer& r,const StreamColumn& source) {
  // Frozen pre-optimization scalar sampling order, independent of bulk reads.
  std::vector<std::uint32_t> result(34u*34u*static_cast<unsigned>(source.height));
  for(int z=-1;z<=32;++z) for(int x=-1;x<=32;++x) {
    const int dx=x<0?-1:x>=32?1:0,dz=z<0?-1:z>=32?1:0;
    const auto found=r.sources.find({source.x+dx,source.z+dz});
    const auto* neighbor=dx==0 && dz==0?&source:found==r.sources.end()?nullptr:&found->second;
    if(!neighbor) continue;
    for(int y=0;y<source.height;++y) {
      const int local_y=source.min_y+y-neighbor->min_y;
      if(local_y<0 || local_y>=neighbor->height) continue;
      result[static_cast<std::size_t>(x+1)+34u*(static_cast<unsigned>(y)+static_cast<unsigned>(source.height)*static_cast<unsigned>(z+1))]=
          neighbor->blocks[static_cast<std::size_t>((x+32)%32)+32u*(static_cast<unsigned>(local_y)+static_cast<unsigned>(neighbor->height)*static_cast<unsigned>((z+32)%32))];
    }
  }
  return result;
}
void halo_input() {
  using namespace octaryn::client::world_presentation;
  WorldRenderer r;
  auto source=generate_stream_column(SnapshotColumn{center.first,center.second,1,{}},7);
  const auto* identity=source.blocks.storage_identity();
  expect(world_mesh_halo(r,source)==scalar_halo(r,source),"compact generated center without neighbors must match scalar halo byte-for-byte");
  for(int dz=-1;dz<=1;++dz) for(int dx=-1;dx<=1;++dx) {
    if(dx==0 && dz==0) continue;
    auto neighbor=generate_stream_column(SnapshotColumn{center.first+dx,center.second+dz,1,{}},7);
    neighbor.min_y+=dx+2*dz;neighbor.height-=std::abs(dx+2*dz);
    neighbor.blocks.resize(static_cast<std::size_t>(neighbor.height)*32*32);
    if(dx!=dz) neighbor.blocks.compact();
    r.sources.emplace(Coordinate{neighbor.x,neighbor.z},std::move(neighbor));
  }
  expect(world_mesh_halo(r,source)==scalar_halo(r,source),"signed generated terrain with all eight shifted-height dense/compact neighbors must match scalar halo");
  expect(source.blocks.storage_identity()==identity,"halo range reads must not detach source snapshot");
  auto dense=source;dense.blocks[12345]=65535;
  expect(world_mesh_halo(r,dense)==scalar_halo(r,dense),"dense edited center must preserve maximum ID in scalar halo");
  dense.blocks.compact();
  expect(world_mesh_halo(r,dense)==scalar_halo(r,dense),"repacked edited center must preserve all halo bytes");
  constexpr unsigned iterations=24;
  std::uint64_t checksum=0;
  const auto measure=[&](bool bulk) {
    const auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<iterations;++i) {
      const auto bytes=bulk?world_mesh_halo(r,source):scalar_halo(r,source);
      checksum+=bytes[(i*7919u)%bytes.size()]+bytes.size();
    }
    return std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/iterations;
  };
  const auto scalar_us=measure(false),bulk_us=measure(true);
  std::printf("world_halo_input generated=1 height=512 neighbors=8 iterations=%u scalar_mean_us=%.3f bulk_mean_us=%.3f checksum=%llu gpu_devices=0\n",
      iterations,scalar_us,bulk_us,static_cast<unsigned long long>(checksum));
}
}
void check_halo_invalidation() {
  all_horizontal_positions();vertical_and_lifecycle();unload();scheduling();readiness();selection_cost();halo_input();
  std::printf("world_halo_invalidation=passed checks=%u horizontal_positions=1024 gpu_devices=0\n",checks);
}
