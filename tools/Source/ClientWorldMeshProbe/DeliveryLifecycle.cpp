#include "Probe.h"
#include "WorldMeshJob.h"
#include "ResourceProbePacing.h"
#include "WorldMeshInput.h"
#include "StreamSnapshot.h"
#include "MeshPhase.h"
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
  require(bool(r.delivery_jobs),"delivery jobs missing during qualification");
  for(std::size_t slot=0;slot<WorldDeliveryJobs::Capacity;++slot)
    require(r.delivery_jobs->wait(slot,1000000000ull),"explicit delivery fence qualification wait");
  resource_probe::complete("delivery_fence_progress");
}
bool pump(WorldRenderer& r,WorldStream& stream) {
  resource_probe::admit();
  if(!r.delivery_jobs)r.delivery_jobs=std::make_unique<WorldDeliveryJobs>();
  // Phase assertions use an explicit qualification budget; production spare
  // time is exercised separately with its two-millisecond slices below.
  return r.delivery_jobs->pump(r,stream,1000.0) && finish_mesh_allocations(r,*r.delivery_jobs);
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
  wait(r);require(progress_mesh_phase(r,*r.delivery_jobs),"adjacent phase progress");
  wait(r);require(progress_mesh_phase(r,*r.delivery_jobs) && r.delivery_jobs->completed()==2,"adjacent emit completion");
}
void settle_pair(Fixture& f,WorldStream& stream) {
  auto& r=f.renderer;
  // The fixture supplies only two identities in a radius-one window. Explicitly
  // qualify both boundaries instead of waiting for seven absent fixture neighbors.
  for(unsigned step=0;step<32;++step) {
    require(pump(r,stream),"adjacent delivery settling");
    for(const auto& coordinate:r.dirty)r.dirty_urgent.insert(coordinate);
    require(world_mesh_refresh_one(r),"adjacent halo settling");
    if(!world_mesh_has_pending(r))return;
    wait(r);
    if(r.halo_jobs)for(std::size_t slot=0;slot<WorldHaloJobs::Capacity;++slot)
      require(r.halo_jobs->wait(slot,1000000000ull),"adjacent halo qualification wait");
  }
  throw std::runtime_error("adjacent delivery failed to settle");
}
void grid_snapshot(const std::filesystem::path& path,std::uint16_t top) {
  std::ofstream out(path,std::ios::binary|std::ios::trunc);
  out.write("OCSTRM01",8);
  write(out,3u);write(out,std::uint64_t{42});write(out,std::uint64_t{});
  write(out,-1);write(out,-1);write(out,1u);write(out,std::uint64_t{1337});
  write(out,0u);write(out,3u);write(out,std::uint64_t{});write(out,0u);write(out,0.0);write(out,0.0f);
  for(int i=0;i<8;++i)write(out,0.0f);
  write(out,0u);write(out,1u);write(out,9u);write(out,9u);
  unsigned index{};
  for(int z=-2;z<=0;++z)for(int x=-2;x<=0;++x) {
    write(out,x);write(out,z);write(out,x*32);write(out,z*32);write(out,index++);write(out,1u);
  }
  for(int z=-2;z<=0;++z)for(int x=-2;x<=0;++x) {
    write(out,x*32+31);write(out,255);write(out,z*32+31);write(out,top);
  }
  require(bool(out),"delivery grid snapshot write failed");
}
std::array<StreamColumn,9> grid_ready(WorldStream& stream,std::uint16_t expected) {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  std::array<StreamColumn,9> sources;
  while(std::chrono::steady_clock::now()<deadline) {
    std::array<const StreamColumn*,9> excluded{};
    std::size_t count{};
    while(count<sources.size() && stream.peek(sources[count],std::span<const StreamColumn* const>(excluded.data(),count)) &&
        sources[count].blocks[32u*512u*32u-1]==expected) {
      excluded[count]=&sources[count];++count;
    }
    if(count==sources.size())return sources;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  throw std::runtime_error("delivery grid mailbox timeout");
}
void prefetch_cases(Fixture& f) {
  auto& r=f.renderer;
  require(r.sources.empty() && !world_mesh_has_pending(r),"prefetch fixture needs retired terrain");
  r.delivery_jobs=std::make_unique<WorldDeliveryJobs>();
  const auto path=std::filesystem::current_path()/"delivery-prefetch.bin";
  struct Remove {std::filesystem::path path;~Remove(){std::error_code ec;std::filesystem::remove(path,ec);}} cleanup{path};
  std::uint16_t stone{};
  for(std::size_t i=1;i<f.catalog.size();++i)if(f.catalog[i].id=="octaryn.basegame.block.stone")stone=static_cast<std::uint16_t>(i);
  require(stone!=0,"prefetch material missing");
  grid_snapshot(path,stone);WorldStream stream(path);stream.request(-1,-1,1);
  open_world_renderer_set_center(&r,-1,-1,1);auto sources=grid_ready(stream,stone);
  require(open_world_renderer_stream_progress(&r,stream,0) && r.delivery_jobs->pending()==0,
      "zero spare-time budget started GPU work");
  const auto revision=r.scene_changes.revision();
  const auto hold_private=[&] {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(r.delivery_jobs->completed()<WorldDeliveryJobs::Capacity && std::chrono::steady_clock::now()<deadline) {
      resource_probe::admit();
      require(open_world_renderer_stream_progress(&r,stream,2.0),"budgeted private delivery progress failed");
      require(r.delivery_jobs->pending()<=WorldDeliveryJobs::Capacity,"private delivery exceeded fixed GPU capacity");
      wait(r);std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(r.delivery_jobs->pending()==WorldDeliveryJobs::Capacity &&
        r.delivery_jobs->completed()==WorldDeliveryJobs::Capacity,"eight private meshes failed to complete");
  };
  hold_private();
  require(r.sources.empty() && r.columns.empty() && r.scene_changes.revision()==revision,
      "spare-time prefetch published visible sources or scene changes");
  for(const auto& source:sources) {
    std::uint16_t block{};
    require(!stream.try_block(source.x*32+31,255,source.z*32+31,block),"private mesh exposed CPU collision query early");
  }
  const auto held_bytes=r.delivery_jobs->gpu_bytes();
  for(unsigned repeat=0;repeat<3;++repeat)require(open_world_renderer_stream_progress(&r,stream,2.0),"held prefetch failed");
  require(r.delivery_jobs->pending()==8 && r.delivery_jobs->gpu_bytes()==held_bytes &&
      r.delivery_jobs->resources(8).signal_value==0,"held eighth slot admitted ninth work or lost retained bytes");
  require(pump(r,stream) && r.columns.size()==8 && r.sources.size()==8 && r.delivery_jobs->pending()==1,
      "pre-camera publication failed to release exactly the staged eight meshes");
  require(r.dirty.empty() && r.dirty_urgent.empty(),"preloaded grid queued arrival repairs");
  std::array<Slang::ComPtr<rhi::IBuffer>,8> first_faces;
  for(std::size_t i=0;i<first_faces.size();++i) {
    first_faces[i]=r.columns.at({sources[i].x,sources[i].z}).faces;
    require(!r.sources.at({sources[i].x,sources[i].z}).mesh_halo,"resident source retained private border storage");
  }
  // The oracle samples independently generated full neighbours, including the
  // ninth column whose GPU mesh and query have not been published yet.
  auto published_sources=r.sources;
  for(const auto& source:sources)r.sources.insert_or_assign({source.x,source.z},source);
  for(std::size_t i=0;i<first_faces.size();++i)
    f.verify("preloaded_before_last_arrival",sources[i],f.read_mesh(r.columns.at({sources[i].x,sources[i].z})));
  r.sources=std::move(published_sources);
  for(std::size_t index=0;index<sources.size();++index) {
    std::uint16_t block{};
    const auto& source=sources[index];
    require(stream.try_block(source.x*32+31,255,source.z*32+31,block)==(index<8),
        "delivery publication lost mailbox order");
  }
  settle_pair(f,stream);
  for(std::size_t i=0;i<first_faces.size();++i)
    require(r.columns.at({sources[i].x,sources[i].z}).faces.get()==first_faces[i].get(),
        "matching neighbour arrival rebuilt an already exact mesh");
  require(r.dirty.empty() && (!r.halo_jobs || r.halo_jobs->pending()==0),"preloaded grid retained halo jobs");
  std::weak_ptr<const ColumnHalo> released_halo=sources[0].mesh_halo;
  sources[0].mesh_halo.reset();
  require(released_halo.expired(),"published query or renderer retained private halo storage");
  for(const auto& source:sources)f.verify("prefetched_grid",source,f.read_mesh(r.columns.at({source.x,source.z})));
  grid_snapshot(path,0);sources=grid_ready(stream,0);hold_private();
  for(const auto& source:sources) {
    std::uint16_t block{};
    require(stream.try_block(source.x*32+31,255,source.z*32+31,block) && block==stone,
        "prefetched edit changed query before frame-head publication");
  }
  stream.request(100,100,1);open_world_renderer_set_center(&r,100,100,1);
  require(r.delivery_jobs->cancelled()==8,"window retirement missed a prefetched GPU slot");
  require(pump(r,stream) && r.delivery_jobs->pending()==0 && r.columns.empty() && r.sources.empty(),
      "cancelled private results resurrected retired geometry");
  std::uint64_t scratch{};
  for(std::size_t slot=0;slot<WorldDeliveryJobs::Capacity;++slot) {
    const auto resources=r.delivery_jobs->resources(slot);scratch+=resources.scratch_bytes;
    require(resources.fences_created==1 && resources.input_capacity==34ull*34*512*4 &&
        resources.scratch_bytes==resources.input_capacity+388,"prefetch slots exceeded reusable scratch bound");
  }
  require(r.delivery_jobs->gpu_bytes()==scratch,"cancelled prefetch retained emitted buffers");
  require(r.debug.errors.load()==0,"prefetch GPU validation errors");
  std::puts("world_mesh_delivery_prefetch=passed slots=8 private_ready=8 ninth_blocked=1 ordered_publication=1 zero_budget=1 cancellation=1 scratch_reused=1 preloaded_exact=1 matching_arrival_no_remesh=1 transient_border_released=1");
}
void settle_preload_repair(Fixture& f) {
  auto& r=f.renderer;
  if(!r.halo_jobs)r.halo_jobs=std::make_unique<WorldHaloJobs>();
  for(unsigned step=0;step<32;++step) {
    resource_probe::Frame frame("delivery_preload_repair");
    require(r.halo_jobs->pump(r,1000.0,1000.0) && finish_mesh_allocations(r,*r.halo_jobs),
        "preload boundary repair progress failed");
    if(r.dirty.empty() && r.halo_jobs->pending()==0)return;
    for(std::size_t slot=0;slot<WorldHaloJobs::Capacity;++slot)
      require(r.halo_jobs->wait(slot,1000000000ull),"preload boundary repair fence wait");
  }
  throw std::runtime_error("preload boundary repair failed to settle");
}
void preload_invalidation_cases(Fixture& f) {
  auto& r=f.renderer;
  const auto path=std::filesystem::current_path()/"delivery-preload-invalidation.bin";
  struct Remove {std::filesystem::path path;~Remove(){std::error_code ec;std::filesystem::remove(path,ec);}} cleanup{path};
  std::uint16_t stone{};
  for(std::size_t i=1;i<f.catalog.size();++i)
    if(f.catalog[i].id=="octaryn.basegame.block.stone")stone=static_cast<std::uint16_t>(i);
  require(stone!=0,"preload invalidation material missing");
  constexpr auto right_slot=column_neighbor_index(1,0);
  for(unsigned mode=0;mode<3;++mode) {
    require(r.sources.empty() && !world_mesh_has_pending(r),"preload invalidation needs retired fixture");
    snapshot(path,stone,true);WorldStream stream(path);stream.request(-1,-1,1);
    open_world_renderer_set_center(&r,-1,-1,1);const auto sources=pair_ready(stream,stone);
    // Publish only the left column. No resident-neighbor eviction may mask the
    // case where its consumed preload leaves before that neighbor ever appears.
    require(open_world_renderer_update(&r,sources[0]) &&
        stream.publish(sources[0])==StreamPublication::Published,"publish isolated preload consumer");
    const auto old_faces=r.columns.at({-1,-1}).faces;
    const auto old_preload=r.columns.at({-1,-1}).preloaded_neighbors[right_slot];
    require(old_preload && world_mesh_preload_matches(old_preload,sources[1]) && r.dirty.empty() &&
        !r.sources.contains({0,-1}) && !r.columns.contains({0,-1}),"isolated mesh did not consume unpublished preload");
    query(stream,true,stone);std::uint16_t block{};
    require(!stream.try_block(0,255,-1,block),"preloaded neighbor became query-visible");
    r.sources.emplace(std::make_pair(0,-1),sources[1]);
    const auto before=f.verify("preload_before_invalidation",sources[0],f.read_mesh(r.columns.at({-1,-1})));
    r.sources.erase({0,-1});
    if(mode==0) {
      stream.request(-2,-1,1);open_world_renderer_set_center(&r,-2,-1,1);
      require(r.dirty.contains({-1,-1}) && r.dirty_urgent.contains({-1,-1}) && r.sources.size()==1,
          "never-published preload leaving the window failed to invalidate its consumer urgently");
    } else {
      auto arrival=sources[1];
      if(mode==1) {
        auto edits=arrival.origin->edits;edits.push_back({0,255,-1,0});
        arrival=generate_stream_column({arrival.x,arrival.z,arrival.revision,std::move(edits)},arrival.epoch);
        require(*arrival.origin!=*sources[1].origin &&
            arrival.blocks.storage_identity()==arrival.generated_blocks.storage_identity(),
            "origin mismatch fixture must remain an unmodified generated payload");
      } else {
        put(arrival,0,511,31,0);
        require(arrival.origin==sources[1].origin &&
            arrival.blocks.storage_identity()!=arrival.generated_blocks.storage_identity(),
            "COW mismatch fixture must retain its unchanged generation origin");
      }
      require(arrival.revision==sources[1].revision && !world_mesh_preload_matches(old_preload,arrival),
          "preload identity trusted a revision collision or changed generated storage");
      require(open_world_renderer_update(&r,arrival) && r.dirty.contains({-1,-1}),
          "mismatching neighbor arrival incorrectly suppressed required boundary repair");
      // This two-column fixture intentionally omits the other seven window identities.
      r.dirty_urgent.insert({-1,-1});
    }
    require(r.columns.at({-1,-1}).faces.get()==old_faces.get(),"invalidation replaced geometry before repair completion");
    settle_preload_repair(f);
    const auto after=f.verify(mode==0?"preload_neighbor_retired":mode==1?"preload_origin_mismatch":"preload_cow_mismatch",
        sources[0],f.read_mesh(r.columns.at({-1,-1})));
    require(after!=before && r.columns.at({-1,-1}).faces.get()!=old_faces.get() &&
        !r.columns.at({-1,-1}).preloaded_neighbors[right_slot],"boundary repair kept stale preload geometry or identity");
    if(mode==0)require(!stream.try_block(0,255,-1,block),"retired private neighbor became query-visible during repair");
    open_world_renderer_set_center(&r,100,100,0);
  }
  snapshot(path,stone,true);WorldStream stream(path);stream.request(-1,-1,1);
  open_world_renderer_set_center(&r,-1,-1,1);const auto sources=pair_ready(stream,stone);
  r.delivery_jobs=std::make_unique<WorldDeliveryJobs>();
  require(pump(r,stream) && r.delivery_jobs->pending()==2 && r.sources.empty(),"stage pending preload consumers");
  pair_query(stream,false);
  stream.request(-2,-1,1);open_world_renderer_set_center(&r,-2,-1,1);
  require(r.delivery_jobs->cancelled()==1 && r.sources.empty(),"window shrink cancelled the retained center or exposed a source");
  finish_pair(r);pair_query(stream,false);
  require(pump(r,stream) && r.delivery_jobs->pending()==0 && r.sources.size()==1 && r.columns.size()==1 &&
      r.sources.contains({-1,-1}) && r.dirty.contains({-1,-1}) && r.dirty_urgent.contains({-1,-1}),
      "pending delivery published a stale outside-window preload without urgent repair");
  const auto stale_faces=r.columns.at({-1,-1}).faces;
  require(bool(r.columns.at({-1,-1}).preloaded_neighbors[right_slot]),"pending fixture never consumed the departing preload");
  query(stream,true,stone);std::uint16_t value{};
  require(!stream.try_block(0,255,-1,value),"cancelled neighbor escaped query publication");
  settle_preload_repair(f);
  f.verify("pending_preload_neighbor_retired",sources[0],f.read_mesh(r.columns.at({-1,-1})));
  require(r.columns.at({-1,-1}).faces.get()!=stale_faces.get() &&
      !r.columns.at({-1,-1}).preloaded_neighbors[right_slot] && !stream.try_block(0,255,-1,value),
      "pending preload repair failed to remove private neighbor dependence");
  open_world_renderer_set_center(&r,100,100,0);
  require(r.debug.errors.load()==0,"preload invalidation GPU validation errors");
  std::puts("world_mesh_preload_invalidation=passed published_window_exit=1 pending_window_exit=1 exact_origin_mismatch=1 cow_mismatch=1 full_face_oracle=1 private_query=1");
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
  require(pump(r,stream) && r.delivery_jobs->pending()==1,"delivery count submission");
  require(r.sources.empty() && r.columns.empty() && open_world_renderer_stats(&r).pending_meshes==1,
      "count submission published source/geometry or hid pending work");
  query(stream,false);wait(r);
  require(progress_mesh_phase(r,*r.delivery_jobs) && r.delivery_jobs->resources(0).emitting,"late delivery emit submission");
  require(r.sources.empty() && r.columns.empty(),"emit submission published source/geometry early");
  query(stream,false);wait(r);
  require(progress_mesh_phase(r,*r.delivery_jobs) && r.delivery_jobs->completed()==1 && r.sources.empty() && r.columns.empty(),
      "late emit completion must retain output without query/source/geometry publication");
  query(stream,false);
  require(pump(r,stream) && r.delivery_jobs->pending()==0,"delivery completion publication");
  query(stream,true,stone);
  f.verify("async_initial_delivery",source,f.read_mesh(r.columns.at({-1,-1})));
  const auto old_faces=r.columns.at({-1,-1}).faces;
  snapshot(path,0);source=ready(stream,0);
  require(pump(r,stream),"delivery edit count submission");
  query(stream,true,stone);wait(r);
  require(progress_mesh_phase(r,*r.delivery_jobs),"late delivery edit emit submission");
  query(stream,true,stone);
  require(r.columns.at({-1,-1}).faces.get()==old_faces.get() &&
      r.sources.at({-1,-1}).revision!=source.revision,"edit replaced live GPU/source before completion");
  wait(r);require(progress_mesh_phase(r,*r.delivery_jobs) && r.delivery_jobs->completed()==1,"late edit completion retention");
  query(stream,true,stone);
  require(r.columns.at({-1,-1}).faces.get()==old_faces.get(),"late edit completion replaced visible mesh");
  require(pump(r,stream),"delivery edit publication");
  query(stream,true,0);f.verify("async_air_edit_delivery",source,f.read_mesh(r.columns.at({-1,-1})));
  snapshot(path,stone);ready(stream,stone);
  require(pump(r,stream),"eviction delivery count submission");
  stream.request(100,100,1);open_world_renderer_set_center(&r,100,100,1);
  stream.request(-1,-1,1);open_world_renderer_set_center(&r,-1,-1,1);
  require(r.delivery_jobs->cancelled()==1 && r.columns.empty() && r.sources.empty(),"center reversal must cancel retired delivery");
  query(stream,false);wait(r);
  require(pump(r,stream),"cancelled delivery emit polling");
  wait(r);require(pump(r,stream),"cancelled delivery final polling");
  query(stream,false);require(r.columns.empty(),"cancelled completion resurrected old geometry");
  // Completion may start the surviving queued/redelivered source in this same pump.
  ready(stream,stone);
  if(r.delivery_jobs->pending()==0)require(pump(r,stream),"reentry count submission");
  wait(r);require(pump(r,stream),"reentry emit submission");
  query(stream,false);wait(r);require(pump(r,stream),"reentry publication");
  query(stream,true,stone);
  for(unsigned retry=0;r.delivery_jobs->pending() && retry<8;++retry) {
    wait(r);require(pump(r,stream),"reentry duplicate queued delivery");
  }
  const auto resources=r.delivery_jobs->resources(0);
  require(resources.fences_created==1 && resources.input_capacity==34ull*34*512*4 &&
      resources.scratch_bytes==resources.input_capacity+388,"delivery scratch must remain bounded and reused");
  require(r.delivery_jobs->pending()==0 && open_world_renderer_stats(&r).pending_meshes==0,"settled delivery remained pending");
  open_world_renderer_set_center(&r,100,100,1);
  require(r.debug.errors.load()==0,"delivery GPU validation errors");
  std::printf("world_mesh_async_delivery=passed query_commit=exact initial=1 edited_air=1 eviction_reentry=1 slots=%zu ready_bound=%zu\n",
      WorldDeliveryJobs::Capacity,StreamReadyCapacity);
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
  require(pump(r,stream) && r.delivery_jobs->pending()==2 &&
      r.delivery_jobs->resources(0).signal_value==1 && r.delivery_jobs->resources(1).signal_value==1,
      "first pump must fill both bounded count slots without early source publication");
  require(pump(r,stream) && r.delivery_jobs->pending()==2 && r.sources.empty(),
      "second bounded delivery pump must not exceed the two-slot limit");
  pair_query(stream,false);finish_pair(r);pair_query(stream,false);
  require(r.sources.empty() && open_world_renderer_stats(&r).pending_meshes==2,"completed pair must stay pending and invisible");
  require(pump(r,stream) && r.delivery_jobs->pending()==0 && r.columns.size()==2,
      "completed adjacent pair must publish in staged order");
  pair_query(stream,true,stone);
  require(r.dirty.empty() && r.dirty_urgent.empty(),
      "matching preloaded adjacent columns must not schedule halo repairs");
  settle_pair(f,stream);
  for(const auto& source:sources)f.verify("async_adjacent_delivery",source,f.read_mesh(r.columns.at({source.x,source.z})));
  const auto old_left=r.columns.at({-1,-1}).faces,old_right=r.columns.at({0,-1}).faces;
  snapshot(path,0,true);sources=pair_ready(stream,0);
  require(pump(r,stream) && pump(r,stream) && r.delivery_jobs->pending()==2,
      "adjacent edited-air staging");
  finish_pair(r);pair_query(stream,true,stone);
  require(r.columns.at({-1,-1}).faces.get()==old_left.get() && r.columns.at({0,-1}).faces.get()==old_right.get(),
      "adjacent edits replaced visible geometry before query publication");
  require(pump(r,stream),"adjacent edited-air publication");pair_query(stream,true,0);
  require(r.dirty_urgent.contains({-1,-1}) && r.dirty_urgent.contains({0,-1}),
      "adjacent edited boundaries must urgently repair both existing and staged snapshots");
  settle_pair(f,stream);
  for(const auto& source:sources)f.verify("async_adjacent_edited_air",source,f.read_mesh(r.columns.at({source.x,source.z})));
  snapshot(path,stone,true);pair_ready(stream,stone);
  require(pump(r,stream) && pump(r,stream) && r.delivery_jobs->pending()==2,
      "adjacent eviction staging");
  stream.request(100,100,1);open_world_renderer_set_center(&r,100,100,1);
  require(r.delivery_jobs->cancelled()==2,"eviction must cancel both staged GPU jobs");
  finish_pair(r);require(pump(r,stream),"adjacent cancelled completion");
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
  std::printf("world_mesh_dual_delivery=passed adjacent_seam_oracle=1 ordered_publication=1 edited_air=1 eviction_reentry=1 slots=%zu paired_columns=2\n",
      WorldDeliveryJobs::Capacity);
}
}
void delivery_cases(Fixture& f,DeliveryGroup group) {
  resource_probe::start();
  if(group==DeliveryGroup::All || group==DeliveryGroup::Lifecycle)delivery_lifecycle_cases(f);
  if(group==DeliveryGroup::All || group==DeliveryGroup::Dual)dual_delivery_cases(f);
  if(group==DeliveryGroup::All || group==DeliveryGroup::Prefetch)prefetch_cases(f);
  if(group==DeliveryGroup::All || group==DeliveryGroup::PreloadInvalidation)preload_invalidation_cases(f);
}
}
