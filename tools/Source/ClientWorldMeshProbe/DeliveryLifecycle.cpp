#include "Probe.h"
#include "WorldMeshJob.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace mesh_probe {
namespace {
using namespace octaryn::client::world_presentation;
template<typename T> void write(std::ofstream& out,T value) {
  out.write(reinterpret_cast<const char*>(&value),sizeof(value));
}
void snapshot(const std::filesystem::path& path,std::uint16_t top,bool adjacent=false) {
  std::ofstream out(path,std::ios::binary|std::ios::trunc);
  out.write("OCSTRM01",8);
  write(out,3u);write(out,std::uint64_t{42});write(out,std::uint64_t{});
  write(out,-1);write(out,-1);write(out,1u);write(out,std::uint64_t{1337});
  write(out,0u);write(out,3u);write(out,std::uint64_t{});write(out,0u);write(out,0.0);write(out,0.0f);
  for(int i=0;i<8;++i)write(out,0.0f);
  write(out,0u);write(out,1u);write(out,adjacent?2u:1u);write(out,adjacent?2u:1u);
  write(out,-1);write(out,-1);write(out,-32);write(out,-32);write(out,0u);write(out,1u);
  if(adjacent) {write(out,0);write(out,-1);write(out,0);write(out,-32);write(out,1u);write(out,1u);}
  write(out,-1);write(out,255);write(out,-1);write(out,top);
  if(adjacent) {write(out,0);write(out,255);write(out,-1);write(out,top);}
  require(bool(out),"delivery fixture snapshot write");
}
StreamColumn ready(WorldStream& stream,std::uint16_t expected) {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  StreamColumn source;
  while(std::chrono::steady_clock::now()<deadline) {
    if(stream.peek(source) && source.blocks[32u*512u*32u-1]==expected)return source;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  throw std::runtime_error("delivery fixture worker timeout: "+stream.status());
}
void wait(WorldRenderer& r) {
  require(r.delivery_jobs && r.delivery_jobs->wait(0,1000000000ull) && r.delivery_jobs->wait(1,1000000000ull),
      "explicit delivery fence qualification wait");
}
void query(WorldStream& stream,bool visible,std::uint16_t expected=0) {
  std::uint16_t block{};
  require(stream.try_block(-1,255,-1,block)==visible,"GPU/query publication visibility mismatch");
  if(visible)require(block==expected,"GPU/query publication delivered wrong edit");
}
std::array<StreamColumn,2> pair_ready(WorldStream& stream,std::uint16_t expected) {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  std::array<StreamColumn,2> sources;
  while(std::chrono::steady_clock::now()<deadline) {
    if(stream.peek(sources[0]) && stream.peek(sources[1],&sources[0]) && sources[0].x==-1 && sources[1].x==0 &&
        sources[0].blocks[32u*512u*32u-1]==expected && sources[1].blocks[32u*512u*32u-32]==expected)return sources;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  throw std::runtime_error("adjacent delivery mailbox timeout");
}
void pair_query(WorldStream& stream,bool visible,std::uint16_t expected=0) {
  query(stream,visible,expected);std::uint16_t value{};
  require(stream.try_block(0,255,-1,value)==visible && (!visible || value==expected),"adjacent query publication mismatch");
}
void finish_pair(WorldRenderer& r) {
  wait(r);require(world_renderer_progress_delivery(r),"adjacent phase progress");
  wait(r);require(world_renderer_progress_delivery(r) && r.delivery_jobs->completed()==2,"adjacent emit completion");
}
void settle_pair(Fixture& f,WorldStream& stream) {
  auto& r=f.renderer;
  // The fixture supplies only two identities in a radius-one window. Explicitly
  // qualify both boundaries instead of waiting for seven absent fixture neighbors.
  for(unsigned step=0;step<32;++step) {
    require(open_world_renderer_stream(&r,stream),"adjacent delivery settling");
    for(const auto& coordinate:r.dirty)r.dirty_urgent.insert(coordinate);
    require(world_mesh_refresh_one(r),"adjacent halo settling");
    if(!world_mesh_has_pending(r))return;
    wait(r);
    if(r.halo_jobs)for(std::size_t slot=0;slot<2;++slot)
      require(r.halo_jobs->wait(slot,1000000000ull),"adjacent halo qualification wait");
  }
  throw std::runtime_error("adjacent delivery failed to settle");
}
}
void delivery_lifecycle_cases(Fixture& f) {
  auto& r=f.renderer;
  require(r.columns.empty() && r.sources.empty() && !world_mesh_has_pending(r),"delivery fixture must start empty");
  const auto path=std::filesystem::current_path()/"delivery-lifecycle.bin";
  struct Remove {std::filesystem::path path;~Remove(){std::error_code ec;std::filesystem::remove(path,ec);}} cleanup{path};
  const auto stone=[&] {
    for(std::size_t i=1;i<f.catalog.size();++i)if(f.catalog[i].id=="octaryn.basegame.block.stone")return static_cast<std::uint16_t>(i);
    throw std::runtime_error("delivery fixture stone missing");
  }();
  snapshot(path,stone);WorldStream stream(path);stream.request(-1,-1,1);
  open_world_renderer_set_center(&r,-1,-1,1);
  auto source=ready(stream,stone);query(stream,false);
  require(open_world_renderer_stream(&r,stream) && r.delivery_jobs->pending()==1,"delivery count submission");
  require(r.sources.empty() && r.columns.empty() && open_world_renderer_stats(&r).pending_meshes==1,
      "count submission published source/geometry or hid pending work");
  query(stream,false);wait(r);
  require(world_renderer_progress_delivery(r) && r.delivery_jobs->resources(0).emitting,"late delivery emit submission");
  require(r.sources.empty() && r.columns.empty(),"emit submission published source/geometry early");
  query(stream,false);wait(r);
  require(world_renderer_progress_delivery(r) && r.delivery_jobs->completed()==1 && r.sources.empty() && r.columns.empty(),
      "late emit completion must retain output without query/source/geometry publication");
  query(stream,false);
  require(open_world_renderer_stream(&r,stream) && r.delivery_jobs->pending()==0,"delivery completion publication");
  query(stream,true,stone);
  f.verify("async_initial_delivery",source,f.read_mesh(r.columns.at({-1,-1})));
  const auto old_faces=r.columns.at({-1,-1}).faces;
  snapshot(path,0);source=ready(stream,0);
  require(open_world_renderer_stream(&r,stream),"delivery edit count submission");
  query(stream,true,stone);wait(r);
  require(world_renderer_progress_delivery(r),"late delivery edit emit submission");
  query(stream,true,stone);
  require(r.columns.at({-1,-1}).faces.get()==old_faces.get() &&
      r.sources.at({-1,-1}).revision!=source.revision,"edit replaced live GPU/source before completion");
  wait(r);require(world_renderer_progress_delivery(r) && r.delivery_jobs->completed()==1,"late edit completion retention");
  query(stream,true,stone);
  require(r.columns.at({-1,-1}).faces.get()==old_faces.get(),"late edit completion replaced visible mesh");
  require(open_world_renderer_stream(&r,stream),"delivery edit publication");
  query(stream,true,0);f.verify("async_air_edit_delivery",source,f.read_mesh(r.columns.at({-1,-1})));
  snapshot(path,stone);ready(stream,stone);
  require(open_world_renderer_stream(&r,stream),"eviction delivery count submission");
  stream.request(100,100,1);open_world_renderer_set_center(&r,100,100,1);
  stream.request(-1,-1,1);open_world_renderer_set_center(&r,-1,-1,1);
  require(r.delivery_jobs->cancelled()==1 && r.columns.empty() && r.sources.empty(),"center reversal must cancel retired delivery");
  query(stream,false);wait(r);
  require(open_world_renderer_stream(&r,stream),"cancelled delivery emit polling");
  wait(r);require(open_world_renderer_stream(&r,stream),"cancelled delivery final polling");
  query(stream,false);require(r.columns.empty(),"cancelled completion resurrected old geometry");
  // Completion may start the surviving queued/redelivered source in this same pump.
  ready(stream,stone);
  if(r.delivery_jobs->pending()==0)require(open_world_renderer_stream(&r,stream),"reentry count submission");
  wait(r);require(open_world_renderer_stream(&r,stream),"reentry emit submission");
  query(stream,false);wait(r);require(open_world_renderer_stream(&r,stream),"reentry publication");
  query(stream,true,stone);
  for(unsigned retry=0;r.delivery_jobs->pending() && retry<8;++retry) {
    wait(r);require(open_world_renderer_stream(&r,stream),"reentry duplicate queued delivery");
  }
  const auto resources=r.delivery_jobs->resources(0);
  require(resources.fences_created==1 && resources.input_capacity==34ull*34*512*4 &&
      resources.scratch_bytes==resources.input_capacity+388,"delivery scratch must remain bounded and reused");
  require(r.delivery_jobs->pending()==0 && open_world_renderer_stats(&r).pending_meshes==0,"settled delivery remained pending");
  open_world_renderer_set_center(&r,100,100,1);
  require(r.debug.errors.load()==0,"delivery GPU validation errors");
  std::puts("world_mesh_async_delivery=passed query_commit=exact initial=1 edited_air=1 eviction_reentry=1 slots=2 ready_bound=2");
}
void dual_delivery_cases(Fixture& f) {
  auto& r=f.renderer;
  require(r.sources.empty() && !world_mesh_has_pending(r),"adjacent delivery needs a retired fixture");
  r.delivery_jobs=std::make_unique<WorldDeliveryJobs>();
  const auto path=std::filesystem::current_path()/"delivery-dual.bin";
  struct Remove {std::filesystem::path path;~Remove(){std::error_code ec;std::filesystem::remove(path,ec);}} cleanup{path};
  std::uint16_t stone{};
  for(std::size_t i=1;i<f.catalog.size();++i)if(f.catalog[i].id=="octaryn.basegame.block.stone")stone=static_cast<std::uint16_t>(i);
  require(stone!=0,"adjacent fixture material missing");
  snapshot(path,stone,true);WorldStream stream(path);stream.request(-1,-1,1);
  open_world_renderer_set_center(&r,-1,-1,1);auto sources=pair_ready(stream,stone);
  require(open_world_renderer_stream(&r,stream) && r.delivery_jobs->pending()==1 &&
      r.delivery_jobs->resources(0).signal_value==1 && r.delivery_jobs->resources(1).fences_created==0,
      "first pump must submit exactly one count despite two ready columns");
  require(open_world_renderer_stream(&r,stream) && r.delivery_jobs->pending()==2 && r.sources.empty(),
      "second bounded delivery must stage without early source publication");
  pair_query(stream,false);finish_pair(r);pair_query(stream,false);
  require(r.sources.empty() && open_world_renderer_stats(&r).pending_meshes==2,"completed pair must stay pending and invisible");
  require(open_world_renderer_stream(&r,stream) && r.delivery_jobs->pending()==0 && r.columns.size()==2,
      "completed adjacent pair must publish in staged order");
  pair_query(stream,true,stone);
  require(r.dirty.contains({-1,-1}) && r.dirty.contains({0,-1}) && r.dirty_urgent.empty(),
      "new adjacent residency must coalesce both halo repairs without promoting arrival to urgent work");
  settle_pair(f,stream);
  for(const auto& source:sources)f.verify("async_adjacent_delivery",source,f.read_mesh(r.columns.at({source.x,source.z})));
  const auto old_left=r.columns.at({-1,-1}).faces,old_right=r.columns.at({0,-1}).faces;
  snapshot(path,0,true);sources=pair_ready(stream,0);
  require(open_world_renderer_stream(&r,stream) && open_world_renderer_stream(&r,stream) && r.delivery_jobs->pending()==2,
      "adjacent edited-air staging");
  finish_pair(r);pair_query(stream,true,stone);
  require(r.columns.at({-1,-1}).faces.get()==old_left.get() && r.columns.at({0,-1}).faces.get()==old_right.get(),
      "adjacent edits replaced visible geometry before query publication");
  require(open_world_renderer_stream(&r,stream),"adjacent edited-air publication");pair_query(stream,true,0);
  require(r.dirty_urgent.contains({-1,-1}) && r.dirty_urgent.contains({0,-1}),
      "adjacent edited boundaries must urgently repair both existing and staged snapshots");
  settle_pair(f,stream);
  for(const auto& source:sources)f.verify("async_adjacent_edited_air",source,f.read_mesh(r.columns.at({source.x,source.z})));
  snapshot(path,stone,true);pair_ready(stream,stone);
  require(open_world_renderer_stream(&r,stream) && open_world_renderer_stream(&r,stream) && r.delivery_jobs->pending()==2,
      "adjacent eviction staging");
  stream.request(100,100,1);open_world_renderer_set_center(&r,100,100,1);
  require(r.delivery_jobs->cancelled()==2,"eviction must cancel both staged GPU jobs");
  finish_pair(r);require(open_world_renderer_stream(&r,stream),"adjacent cancelled completion");
  pair_query(stream,false);require(r.columns.empty() && r.sources.empty() && r.delivery_jobs->pending()==0,
      "cancelled adjacent pair resurrected terrain");
  stream.request(-1,-1,1);open_world_renderer_set_center(&r,-1,-1,1);pair_ready(stream,stone);
  settle_pair(f,stream);pair_query(stream,true,stone);
  std::uint64_t scratch{};
  for(std::size_t slot=0;slot<2;++slot) {
    const auto resources=r.delivery_jobs->resources(slot);scratch+=resources.scratch_bytes;
    require(resources.fences_created==1 && resources.input_capacity==34ull*34*512*4 &&
        resources.scratch_bytes==resources.input_capacity+388,"two-slot scratch must be bounded and reused");
  }
  require(r.delivery_jobs->gpu_bytes()==scratch,"idle delivery slots retained mesh outputs");
  open_world_renderer_set_center(&r,100,100,1);
  require(r.debug.errors.load()==0,"adjacent delivery GPU validation errors");
  std::puts("world_mesh_dual_delivery=passed adjacent_seam_oracle=1 ordered_publication=1 edited_air=1 eviction_reentry=1 slots=2 max_new_count_per_pump=1");
}
}
