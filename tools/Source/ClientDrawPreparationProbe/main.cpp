#include "WorldRendererInternal.h"
#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>

void check_halo_invalidation();
void check_camera_culling();

namespace {
using namespace octaryn::client::rendering;
using Coordinate=std::pair<std::int32_t,std::int32_t>;
using Columns=std::map<Coordinate,WorldColumnGpu>;
unsigned checks{};
void expect(bool result,const char* description) {
  ++checks;
  if(!result) throw std::runtime_error(description);
}
void add(Columns& columns,int x,int z,std::uint32_t faces) {
  auto& column=columns[{x,z}];
  column.face_count=faces;column.min_y=0;column.height=32;
  column.pass_counts[0]=faces;
}
bool ordered(const WorldDrawList& list,Columns& columns,std::initializer_list<Coordinate> expected) {
  if(list.visible.size()!=expected.size()) return false;
  std::size_t index{};
  for(const auto coordinate:expected)
    if(list.visible[index++].column!=&columns.at(coordinate)) return false;
  return true;
}
void visibility() {
  Columns columns;
  add(columns,0,0,5);add(columns,0,-2,7);add(columns,0,2,11);
  add(columns,500,0,13);add(columns,0,-1,0);
  WorldDrawList list;
  WorldCamera eye{16,16,16,0,0,1.57079632679f};
  world_prepare_draw_list(list,columns,eye,1280,720,true);
  expect(ordered(list,columns,{{0,-2},{0,0}}),"camera inside column and front column visible; behind/outside/empty excluded");
  expect(list.quads==12,"visible face count matches draw statistics");
  world_prepare_draw_list(list,columns,eye,1280,720,false);
  expect(ordered(list,columns,{{500,0},{0,-2},{0,2},{0,0}}),"culling disabled preserves distance order and excludes empty column");
  expect(list.quads==36,"culling disabled counts every nonempty column once");
  eye.yaw=3.14159265359f;
  world_prepare_draw_list(list,columns,eye,1280,720,true);
  expect(ordered(list,columns,{{0,2},{0,0}}),"changed camera rebuilds visibility for next frame");
  eye.yaw=0;eye.vertical_fov=.3f;
  world_prepare_draw_list(list,columns,eye,720,1280,true);
  expect(ordered(list,columns,{{0,-2},{0,0}}),"zoom and portrait dimensions retain central visible columns");
}
void stable_order() {
  Columns columns;
  add(columns,0,0,2);add(columns,-1,0,3);add(columns,0,-1,5);add(columns,-1,-1,7);
  // Equal squared distances must retain map order, regardless of insertion order.
  columns.at({0,0}).pass_counts={0,0,0,2,0};
  WorldCamera eye{0,16,0,0,0,1.57079632679f};
  WorldDrawList list;
  world_prepare_draw_list(list,columns,eye,1280,720,false);
  expect(ordered(list,columns,{{-1,-1},{-1,0},{0,-1},{0,0}}),"equal-distance columns keep original stable map order");
  expect(list.quads==17,"forward-only column remains in shared list and statistics");
}
void pass_order() {
  Columns columns;
  add(columns,0,-1,2);add(columns,0,-2,3);add(columns,0,-4,5);
  WorldDrawList list;
  world_prepare_draw_list(list,columns,{16,16,16,0,0,1.57079632679f},1280,720,false);
  for(std::size_t index=0;index<3;++index) {
    const int distances[]={1,2,4};
    expect(world_draw_item(list,index,false).column==&columns.at({0,-distances[index]}),
        "actual opaque/sprite selector draws near to far");
    expect(world_draw_item(list,index,true).column==&columns.at({0,-distances[2-index]}),
        "actual glass/lava/water selector preserves far to near");
  }
  expect(list.quads==10,"opposite traversal directions preserve every face");
}
void cached_stats() {
  WorldRenderer r;r.width=1280;r.height=720;
  expect(r.frame_queue.count()==2,"default renderer owns two mutable target slots");
  const auto store=[&](Coordinate at,std::uint32_t faces,std::uint32_t water,std::uint32_t lava,std::uint32_t opaque_patches=0) {
    WorldColumnGpu column;column.face_count=faces;column.pass_counts={faces-water-lava,0,0,water,lava};
    column.patch_counts={opaque_patches?opaque_patches:faces-water-lava,0,0,water,lava};
    world_renderer_store_column(r,at,std::move(column));
  };
  const auto verify=[&] {
    // Per slot: RGBA16F albedo, RGBA32F relative position, two RGBA8
    // G-buffers, RGBA16F HDR, D32 depth, RGBA8 presentation and R32 sun visibility.
    constexpr std::uint64_t target_bytes_per_pixel=2*(8+16+4+4+8+4+4+4);
    std::uint64_t faces{},bytes=std::uint64_t(r.width)*static_cast<std::uint64_t>(r.height)*target_bytes_per_pixel;
    for(const auto& [at,column]:r.columns) {
      (void)at;faces+=column.face_count;
      std::uint64_t patches{};for(const auto count:column.patch_counts) patches+=count;
      bytes+=std::max(1u,column.face_count)*16ull+160+std::max(std::uint64_t{1},patches)*4+
          std::max(1u,column.pass_counts[3]+column.pass_counts[4])*32ull;
    }
    const auto stats=open_world_renderer_stats(&r);
    expect(stats.columns==r.columns.size() && stats.quads==faces && stats.gpu_bytes==bytes,
        "cached counts equal complete map oracle after production mutations");
  };
  verify();
  r.lighting_settings.shadow_resolution=2048;r.local_shadows.resolution=2048;
  r.rt_shadows.width=1280;r.rt_shadows.height=720;
  verify(); // Settings and cached extents alone do not allocate optional lighting resources.
  store({0,0},10,0,0);store({4,4},20,2,1);store({-4,-4},0,0,0);verify();
  store({0,0},7,1,1);verify(); // Same-coordinate halo/stream replacement subtracts the old allocation.
  const auto previous_bytes=open_world_renderer_stats(&r).gpu_bytes;
  store({0,0},7,1,1,19);verify();
  expect(open_world_renderer_stats(&r).gpu_bytes==previous_bytes+14*4,
      "patch-only replacement adjusts backing bytes without changing face counts");
  store({4,4},0,0,0);verify(); // Empty meshes retain their minimum backing allocation.
  auto* retained=&r.columns.at({0,0});r.dirty.insert({0,0});
  for(int frame=0;frame<100;++frame) open_world_renderer_set_center(&r,0,0,4);
  expect(&r.columns.at({0,0})==retained && r.dirty.contains({0,0}),
      "unchanged window preserves retained resources and pending halo work");verify();
  open_world_renderer_set_center(&r,0,0,1);verify();
  expect(r.columns.size()==1,"shrink retires outer columns and their cached totals");
  open_world_renderer_set_center(&r,0,0,128);store({-32,0},3,0,0);verify();
  open_world_renderer_set_center(&r,0,0,33);verify();
  expect(r.radius==32 && r.columns.size()==2,"equivalent clamped radius keeps maximum-distance edge");
  r.width=640;r.height=480;verify();
  open_world_renderer_set_center(&r,100,100,0);verify();
  expect(r.columns.empty() && r.resident_quads==0 && r.column_gpu_bytes==0,
      "complete retirement clears resident totals without retaining phantom bytes");
}
void reuse_and_mutation() {
  Columns columns;
  for(int x=-4;x<=4;++x) for(int z=-4;z<=4;++z) add(columns,x,z,1);
  WorldCamera eye{16,16,16,0,0,1.57079632679f};
  WorldDrawList list;
  world_prepare_draw_list(list,columns,eye,1280,720,false);
  const auto capacity=list.visible.capacity();
  const auto* storage=list.visible.data();
  expect(list.visible.size()==81 && list.quads==81,"baseline fixture contains 81 columns");
  for(int frame=0;frame<100;++frame) {
    world_prepare_draw_list(list,columns,eye,1280,720,false);
    expect(list.visible.capacity()==capacity && list.visible.data()==storage,"repeated preparation retains vector storage");
  }
  columns.at({0,0}).face_count=0;
  columns.erase({4,4});add(columns,5,0,9);
  world_prepare_draw_list(list,columns,eye,1280,720,false);
  expect(list.visible.size()==80 && list.quads==88,"erased, inserted, and empty columns refresh the next list");
  expect(list.visible.front().column==&columns.at({-4,-4}),"farthest distance ordering survives map mutation");
  expect(std::any_of(list.visible.begin(),list.visible.end(),[&](const auto& item){return item.column==&columns.at({5,0});}),"new column enters retained list");
  expect(list.visible.capacity()==capacity && list.visible.data()==storage,"bounded mutation reuses existing list storage");
  columns.clear();add(columns,0,-2,4);
  world_prepare_draw_list(list,columns,eye,1280,720,true);
  expect(list.visible.size()==1,"replacement fixture is initially in view");
  WorldColumnGpu replacement;
  replacement.face_count=4;replacement.min_y=10000;replacement.height=32;
  columns.insert_or_assign({0,-2},std::move(replacement));
  world_prepare_draw_list(list,columns,eye,1280,720,true);
  expect(list.visible.empty() && list.quads==0,"column bounds replacement invalidates old visibility");
  columns.clear();
  world_prepare_draw_list(list,columns,eye,1280,720,false);
  expect(list.visible.empty() && list.quads==0 && list.visible.capacity()==capacity,"empty frame clears statistics without releasing capacity");
}
}
void check_temporal_camera();
int main() {
  try {check_temporal_camera();visibility();stable_order();pass_order();cached_stats();reuse_and_mutation();check_halo_invalidation();check_camera_culling();}
  catch(const std::exception& error) {
    std::fprintf(stderr,"world_draw_preparation=failed checks=%u reason=%s\n",checks,error.what());return 1;
  }
  std::printf("world_draw_preparation=passed checks=%u gpu_devices=0\n",checks);
  return 0;
}
