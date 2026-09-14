#include "Probe.h"
#include "WorldHaloJobs.h"
#include "WorldMeshJob.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <chrono>
namespace mesh_probe {
namespace {
using Coordinate=std::pair<std::int32_t,std::int32_t>;
constexpr Coordinate center{-1,-1};
std::uint64_t completion_waits{};
double longest_completion_ms{};
void wait_jobs(WorldRenderer& r,const char* message) {
  if(!r.halo_jobs)return;
  for(std::size_t slot=0;slot<2;++slot) {
    const auto before=std::chrono::steady_clock::now();
    require(r.halo_jobs->wait(slot,1000000000ull),message);
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
    longest_completion_ms=std::max(longest_completion_ms,elapsed);++completion_waits;
  }
}
std::uint16_t fluid_id(const Fixture& f,const char* kind,int level) {
  for(std::size_t i=0;i<f.catalog.size();++i)
    if(f.catalog[i].fluidKind==kind && f.catalog[i].fluidLevel==level)return static_cast<std::uint16_t>(i);
  throw std::runtime_error("fluid lifecycle catalog level missing");
}
float corner(const Fixture& f) {
  const auto mesh=f.read_mesh(f.renderer.columns.at(center));
  const auto base=mesh.gpu.pass_counts[0]+mesh.gpu.pass_counts[1]+mesh.gpu.pass_counts[2];
  for(std::size_t i=0;i<mesh.fluids.size();++i) {
    const auto& face=mesh.faces[base+i];
    if(std::bit_cast<int>(face[0])==-1 && std::bit_cast<int>(face[1])==130 && std::bit_cast<int>(face[2])==-1)
      return mesh.fluids[i][2]; // Original (+X,+Z) corner.
  }
  throw std::runtime_error("central boundary fluid was not emitted");
}
void settle(Fixture& f,const std::string& phase,bool sparse_immediate=true) {
  auto& r=f.renderer;
  // Sparse fluid/lifecycle fixtures deliberately omit expected window columns;
  // these cases qualify immediate refresh, not arrival coalescing.
  if(sparse_immediate)r.dirty_urgent.insert(r.dirty.begin(),r.dirty.end());
  const auto bound=4*(r.dirty.size()+(r.halo_jobs?r.halo_jobs->pending():0)+1);std::size_t calls=0;
  std::vector<std::string> trace;
  while(world_mesh_has_pending(r)) {
    if(sparse_immediate)r.dirty_urgent.insert(r.dirty.begin(),r.dirty.end());
    std::ostringstream state;state<<"halo_trace phase="<<phase<<" pump="<<calls<<" bound="<<bound<<" dirty="<<r.dirty.size();
    if(r.halo_jobs)for(unsigned slot=0;slot<2;++slot) {
      const auto resources=r.halo_jobs->resources(slot);
      state<<" slot"<<slot<<"_buffers="<<resources.buffers_created<<" slot"<<slot<<"_capacity="<<resources.input_capacity
          <<" slot"<<slot<<"_fence="<<resources.observed_value<<"/"<<resources.signal_value
          <<" slot"<<slot<<"_emitting="<<resources.emitting<<" slot"<<slot<<"_finished="<<resources.finished
          <<" slot"<<slot<<"_polls="<<resources.poll_calls<<" slot"<<slot<<"_timeouts="<<resources.poll_timeouts
          <<" slot"<<slot<<"_result="<<resources.last_poll_result
          <<" slot"<<slot<<"_current="<<r.halo_jobs->current(slot,r);
    }
    for(const auto& [coordinate,source]:r.sources) {
      state<<" column="<<coordinate.first<<","<<coordinate.second<<":"<<source.revision<<":"<<source.height
          <<":"<<source.blocks.storage_identity()<<":"<<r.columns.at(coordinate).faces.get()
          <<":dirty="<<r.dirty.contains(coordinate)<<":active="<<(r.halo_jobs && r.halo_jobs->contains(coordinate));
    }
    trace.push_back(state.str());
    if(calls==bound)for(const auto& line:trace)std::fprintf(stderr,"%s\n",line.c_str());
    require(++calls<=bound,"halo refresh failed to drain bounded dirty queue");
    require(world_mesh_refresh_one(r),"production halo refresh failed");
    require(!r.halo_jobs || r.halo_jobs->pending()<=2,"halo jobs exceeded the two-slot bound");
    // Wait on the exact phase fences with a finite deadline. Production only polls.
    wait_jobs(r,"halo qualification fence completion");
  }
  require(open_world_renderer_stats(&r).pending_meshes==0,"settled halo still advertised pending work");
  for(const auto& [coordinate,source]:r.sources) {
    const auto found=r.columns.find(coordinate);require(found!=r.columns.end(),"retained source lost its GPU column");
    const auto name=phase+"_"+std::to_string(coordinate.first)+"_"+std::to_string(coordinate.second);
    f.verify(name.c_str(),source,f.read_mesh(found->second));
  }
  std::printf("world_mesh_halo phase=%s retained=%zu refresh_calls=%zu pending=0\n",phase.c_str(),r.columns.size(),calls);
}
void run(Fixture& f,const char* kind) {
  auto& r=f.renderer;r.columns.clear();r.sources.clear();r.dirty.clear();r.dirty_urgent.clear();
  r.resident_quads=0;r.column_gpu_bytes=0;
  open_world_renderer_set_center(&r,-1,-1,1);
  auto source=column(-1,-1,128,8),east=column(0,-1,128,8),south=column(-1,0,128,8),diagonal=column(0,0,128,8);
  put(source,31,2,31,fluid_id(f,kind,0));put(east,0,2,31,fluid_id(f,kind,7));
  put(south,31,2,0,fluid_id(f,kind,3));put(diagonal,0,2,0,fluid_id(f,kind,7));
  const auto update=[&](const StreamColumn& next,const char* phase,bool center_dirty) {
    require(open_world_renderer_update(&r,next),"actual retained column update failed");
    if(center_dirty)require(r.dirty.contains(center),"boundary change failed to enqueue central mesh refresh");
    // A stale coordinate at the front must not block the real pending neighbors.
    r.dirty.insert({-100,-100});settle(f,std::string(kind)+"_"+phase);
  };
  update(source,"alone",false);const float alone=corner(f);
  update(east,"east_arrival",true);update(south,"south_arrival",true);
  update(diagonal,"diagonal_arrival",true);const float low=corner(f);
  put(diagonal,0,2,0,fluid_id(f,kind,0));++diagonal.revision;
  update(diagonal,"diagonal_level",true);const float high=corner(f);
  require(high>low+.01f,"diagonal level change did not change real central GPU corner");
  put(diagonal,0,3,0,fluid_id(f,kind,0));++diagonal.revision;
  update(diagonal,"diagonal_above",true);
  require(std::abs(corner(f)-1.f)<.0001f,"diagonal fluid above did not lift shared corner to full height");
  open_world_renderer_set_center(&r,-1,-2,1);
  require(r.sources.size()==2 && r.dirty.contains(center),"window move did not retire south/diagonal and dirty survivor");
  settle(f,std::string(kind)+"_south_diagonal_unload");
  require(corner(f)<.99f,"retired diagonal remained in GPU fluid corner");
  open_world_renderer_set_center(&r,-1,-1,0);
  require(r.sources.size()==1 && r.dirty.contains(center),"last halo unload missed central survivor");
  settle(f,std::string(kind)+"_all_neighbors_unload");
  require(std::abs(corner(f)-alone)<.0001f,"fully unloaded halo did not return original standalone fluid surface");
  open_world_renderer_set_center(&r,100,100,0);
  settle(f,std::string(kind)+"_complete_retirement");
  require(r.sources.empty() && r.columns.empty() && r.dirty.empty(),"complete fluid fixture retirement retained stale work");
}
Slang::ComPtr<rhi::IBuffer> start_center(Fixture& f) {
  auto& r=f.renderer;
  require(r.dirty.contains(center),"async fixture did not queue the nearest central column");
  r.dirty_urgent.insert(r.dirty.begin(),r.dirty.end());
  auto previous=r.columns.at(center).faces;
  require(world_mesh_refresh_one(r),"async halo submission failed");
  require(r.halo_jobs && r.halo_jobs->contains(center) && r.halo_jobs->pending()>0,
      "first halo pump did not retain an in-flight center job");
  require(world_mesh_has_pending(r) && open_world_renderer_stats(&r).pending_meshes>=r.halo_jobs->pending(),
      "pending statistics omitted an in-flight halo job");
  require(r.columns.at(center).faces.get()==previous.get(),"count submission replaced visible geometry before completion");
  return previous;
}
void finish_stale(Fixture& f,rhi::IBuffer* retained) {
  auto& r=f.renderer;
  require(world_mesh_refresh_one(r),"stale job count-to-emit failed");
  wait_jobs(r,"stale job emit completion");
  require(world_mesh_refresh_one(r),"completed stale job rejection failed");
  require(r.columns.at(center).faces.get()==retained,"completed stale job overwrote retained geometry");
}
void async_cases(Fixture& f) {
  auto& r=f.renderer;
  open_world_renderer_set_center(&r,-1,-1,1);
  auto source=column(-1,-1,128,8),east=column(0,-1,128,8),south=column(-1,0,128,8),diagonal=column(0,0,128,8);
  put(source,31,2,31,fluid_id(f,"water",0));put(east,0,2,31,fluid_id(f,"water",7));
  put(south,31,2,0,fluid_id(f,"water",3));put(diagonal,0,2,0,fluid_id(f,"water",7));
  const auto update=[&](const StreamColumn& c) {require(open_world_renderer_update(&r,c),"async fixture authoritative update failed");};
  update(source);update(east);update(south);update(diagonal);settle(f,"async_baseline");

  put(diagonal,0,2,0,fluid_id(f,"water",0));++diagonal.revision;update(diagonal);
  const auto previous=start_center(f);
  wait_jobs(r,"async count completion");
  require(world_mesh_refresh_one(r),"async count-to-emit phase failed");
  require(r.halo_jobs->pending()==2,"async halo did not fill both bounded slots");
  require(r.columns.at(center).faces.get()==previous.get(),"emit submission prematurely replaced retained geometry");
  wait_jobs(r,"async emit completion before invalidation");
  // Same numeric revision, different immutable payload: revision-only validation
  // would publish a stale shared fluid corner from the completed emit.
  put(diagonal,0,2,0,fluid_id(f,"water",5));update(diagonal);
  require(world_mesh_refresh_one(r),"stale diagonal result rejection failed");
  require(r.columns.at(center).faces.get()==previous.get(),"stale diagonal emit replaced the current mesh");
  settle(f,"async_diagonal_same_revision");

  put(diagonal,0,3,0,fluid_id(f,"water",0));++diagonal.revision;update(diagonal);
  start_center(f);wait_jobs(r,"async primary count completion");
  put(source,31,2,31,fluid_id(f,"water",7));++source.revision;update(source);
  const auto replacement=r.columns.at(center).faces;
  finish_stale(f,replacement.get());
  require(r.columns.at(center).faces.get()==replacement.get(),"stale primary halo overwrote an authoritative replacement");
  settle(f,"async_primary_replacement");

  // Changed vertical layout is a dependency change even with unchanged revision.
  auto taller=column(0,0,127,9);taller.revision=diagonal.revision;
  put(taller,0,3,0,fluid_id(f,"water",5));put(taller,0,4,0,fluid_id(f,"water",0));
  r.dirty.insert(center);const auto before_extent=start_center(f);
  wait_jobs(r,"async extent count completion");update(taller);
  finish_stale(f,before_extent.get());
  require(r.columns.at(center).faces.get()==before_extent.get(),"changed halo extent published stale work");
  settle(f,"async_neighbor_extent");

  r.dirty.insert(center);start_center(f);
  wait_jobs(r,"async eviction count completion");
  open_world_renderer_set_center(&r,100,100,0);
  require(r.sources.empty() && r.columns.empty(),"eviction retained authoritative columns");
  open_world_renderer_set_center(&r,-1,-1,1);update(source);
  const auto reentered=r.columns.at(center).faces;
  finish_stale(f,reentered.get());
  require(r.columns.at(center).faces.get()==reentered.get(),"old residency job replaced a new column incarnation");
  settle(f,"async_eviction_reentry");

  // A formerly absent diagonal must invalidate the captured all-air halo.
  r.dirty.insert(center);const auto before_arrival=start_center(f);
  wait_jobs(r,"async arrival count completion");update(diagonal);
  finish_stale(f,before_arrival.get());
  require(r.columns.at(center).faces.get()==before_arrival.get(),"late neighbor arrival published an all-air halo result");
  settle(f,"async_neighbor_arrival");

  r.dirty.insert(center);start_center(f);wait_jobs(r,"async empty count completion");
  source.blocks.fill(0);++source.revision;update(source);
  const auto empty=r.columns.at(center).faces;
  require(r.columns.at(center).face_count==0,"empty authoritative column retained old faces");
  finish_stale(f,empty.get());
  require(r.columns.at(center).face_count==0 && r.columns.at(center).faces.get()==empty.get(),
      "stale halo resurrected geometry after an empty replacement");
  settle(f,"async_empty_replacement");

  r.dirty.insert(center);start_center(f);
  open_world_renderer_set_center(&r,100,100,0);settle(f,"async_all_evicted_in_flight");
  require(r.sources.empty() && r.columns.empty() && open_world_renderer_stats(&r).quads==0,
      "in-flight completion resurrected evicted geometry");
  std::puts("world_mesh_async_halo=passed slots=2 stale_primary=1 same_revision_payload=1 extent=1 arrival=1 eviction_reentry=1 empty=1");
}
void reuse_cases(Fixture& f) {
  auto& r=f.renderer;open_world_renderer_set_center(&r,-1,-1,1);
  require(r.columns.empty() && !world_mesh_has_pending(r),"reuse fixture requires fully retired jobs");
  r.halo_jobs=std::make_unique<WorldHaloJobs>();
  r.qualification_mesh=std::make_unique<WorldMeshJob>();
  const auto material=[&](const char* name) {
    const auto wanted=std::string("octaryn.basegame.block.")+name;
    for(std::size_t i=1;i<f.catalog.size();++i)if(f.catalog[i].id==wanted)return static_cast<std::uint16_t>(i);
    throw std::runtime_error("mesh reuse fixture catalog material missing");
  };
  const std::array<std::uint16_t,5> ids{material("stone"),material("bluebell"),material("glass"),
      fluid_id(f,"water",0),fluid_id(f,"lava",0)};
  constexpr std::array heights{8,1,65,512,8,1,65,512};
  constexpr std::uint64_t max_input_bytes=34ull*34*512*4;
  std::uint64_t max_halo_bytes{};
  for(std::size_t stage=0;stage<heights.size();++stage) {
    const auto height=heights[stage];const auto pattern=stage%4;
    const auto previous_delivery=r.qualification_mesh->resources();
    const std::array previous_slots{r.halo_jobs->resources(0),r.halo_jobs->resources(1)};
    for(int dx=0;dx<2;++dx) {
      auto source=column(-1+dx,-1,-256,height);source.revision=100+stage;
      for(unsigned p=0;p<ids.size();++p) {
        const bool present=pattern==0 || (pattern==1 && p==0) || (pattern==2 && p>=3);
        if(!present)continue;
        // Hit both vertical ends and a band boundary; locations are isolated so
        // all five passes can be independently enabled/cleared on reused jobs.
        put(source,2+int(p)*6,0,3,ids[p]);
        if(height>1)put(source,2+int(p)*6,height-1,25,ids[p]);
        if(height>32)put(source,2+int(p)*6,32,14,ids[p]);
      }
      require(open_world_renderer_update(&r,source),"reused initial mesh job failed");
    }
    // Force both persistent halo slots to see the same alternating sizes. The
    // real updates above still exercise synchronous delivery and invalidation.
    r.dirty.insert(center);r.dirty.insert({0,-1});
    const auto previous=start_center(f);wait_jobs(r,"reuse count completion");
    require(world_mesh_refresh_one(r) && r.halo_jobs->pending()==2,"reuse did not occupy both halo slots");
    require(r.columns.at(center).faces.get()==previous.get(),"reuse count/emit changed visible mesh early");
    settle(f,"reuse_"+std::to_string(stage)+"_height_"+std::to_string(height));
    for(const auto& [coordinate,gpu]:r.columns) {
      (void)coordinate;
      for(unsigned p=0;p<5;++p) {
        const bool present=pattern==0 || (pattern==1 && p==0) || (pattern==2 && p>=3);
        require((gpu.pass_counts[p]!=0)==present,"reused job retained an earlier material-pass count");
        require((gpu.patch_counts[p]!=0)==present,"reused job retained an earlier patch-pass count");
      }
    }
    const auto bytes=r.halo_jobs->gpu_bytes();
    const auto input_bytes=34ull*34*static_cast<std::uint64_t>(height)*4;
    const auto check_resources=[&](const auto& before,const auto& after,unsigned outputs) {
      const bool grew=input_bytes>before.input_capacity;
      const auto expected=stage==0?7u+outputs*4u:outputs*4u+unsigned(grew);
      require(after.buffers_created-before.buffers_created==expected,"mesh scratch recreated an unnecessary buffer");
      require(after.fences_created==1,"persistent mesh job recreated its fence");
      require(after.input_capacity>=input_bytes && after.input_capacity<=max_input_bytes &&
          after.input_capacity>=before.input_capacity,"mesh input capacity was unbounded or shrank");
      if(!grew)require(after.input_capacity==before.input_capacity,"shorter mesh changed retained input capacity");
      require(after.scratch_bytes==after.input_capacity+388,"mesh scratch accounting retained unexpected resources");
    };
    check_resources(previous_delivery,r.qualification_mesh->resources(),2);
    std::uint64_t slot_bytes{};
    for(unsigned slot=0;slot<2;++slot) {
      const auto resources=r.halo_jobs->resources(slot);
      check_resources(previous_slots[slot],resources,1);slot_bytes+=resources.scratch_bytes;
    }
    require(bytes==slot_bytes && bytes<=2*(max_input_bytes+388),"idle halo retained output or unbounded scratch buffers");
    max_halo_bytes=std::max(max_halo_bytes,bytes);
    const auto before=open_world_renderer_stats(&r);const auto snapshots=r.columns;
    for(unsigned idle=0;idle<4;++idle)require(world_mesh_refresh_one(r),"idle halo pump failed");
    require(!world_mesh_has_pending(r) && r.halo_jobs->gpu_bytes()==bytes,"idle pump changed scratch or pending work");
    const auto after=open_world_renderer_stats(&r);
    require(after.gpu_bytes==before.gpu_bytes && after.quads==before.quads,"idle pump changed retained mesh accounting");
    for(const auto& [coordinate,gpu]:snapshots) {
      const auto& current=r.columns.at(coordinate);
      require(current.faces.get()==gpu.faces.get() && current.fluids.get()==gpu.fluids.get() &&
          current.patches.get()==gpu.patches.get() && current.arguments.get()==gpu.arguments.get(),
          "idle pump replaced retained GPU buffers");
    }
  }
  open_world_renderer_set_center(&r,100,100,0);settle(f,"reuse_retired");
  std::printf("world_mesh_scratch_reuse=passed cycles=%zu slots=2 max_height=512 halo_bytes=%llu idle_pumps=32\n",
      heights.size(),static_cast<unsigned long long>(max_halo_bytes));
}
void coalescing_cases(Fixture& f) {
  auto& r=f.renderer;
  require(r.sources.empty() && !world_mesh_has_pending(r),"coalescing needs a fully retired fixture");
  r.halo_jobs=std::make_unique<WorldHaloJobs>();
  open_world_renderer_set_center(&r,center.first,center.second,1);
  auto source=column(center.first,center.second,128,8);
  put(source,31,2,31,fluid_id(f,"water",0));
  require(open_world_renderer_update(&r,source),"coalescing initial visible mesh failed");
  const auto initial=r.columns.at(center).faces;
  std::set<Coordinate> dirtied;
  for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
    if(dx==0 && dz==0)continue;
    auto neighbor=column(center.first+dx,center.second+dz,128,8);
    put(neighbor,dx>0?0:31,2,dz>0?0:31,fluid_id(f,"water",7));
    require(open_world_renderer_update(&r,neighbor),"coalescing neighbor arrival failed");
    dirtied.insert(r.dirty.begin(),r.dirty.end());
    for(unsigned phase=0;phase<3;++phase) {
      require(world_mesh_refresh_one(r),"coalescing pump failed");
      wait_jobs(r,"coalescing phase fence completion");
    }
    if(dx!=1 || dz!=1) {
      require(!r.halo_jobs->contains(center) && r.dirty.contains(center),
          "ordinary center refreshed before its last expected neighbor arrived");
      require(r.columns.at(center).faces.get()==initial.get(),"waiting arrivals replaced the visible initial mesh");
    }
  }
  settle(f,"coalesced_complete_window",false);
  const auto phases=r.halo_jobs->resources(0).signal_value+r.halo_jobs->resources(1).signal_value;
  require(phases==2*dirtied.size(),"arrival coalescing emitted more than one halo refresh per dirty column");
  require(r.columns.at(center).faces.get()!=initial.get(),"complete neighborhood never refreshed the initial mesh");

  open_world_renderer_set_center(&r,center.first,center.second,0);
  require(r.dirty_urgent.contains(center),"neighbor unload did not bypass arrival coalescing");
  settle(f,"coalesced_unload",false);
  open_world_renderer_set_center(&r,center.first,center.second,1);
  auto east=column(center.first+1,center.second,128,8);
  put(east,0,2,31,fluid_id(f,"water",7));
  require(open_world_renderer_update(&r,east),"partial-window arrival failed");
  require(world_mesh_refresh_one(r) && r.dirty.contains(center) && !r.halo_jobs->contains(center),
      "ordinary partial-window arrival failed to wait");
  put(east,0,2,31,fluid_id(f,"water",0));++east.revision;
  require(open_world_renderer_update(&r,east) && r.dirty_urgent.contains(center),"actual boundary edit did not promote waiting work");
  require(world_mesh_refresh_one(r) && r.halo_jobs->contains(center),"urgent boundary edit waited for absent neighbors");
  const auto retained=r.columns.at(center).faces;
  wait_jobs(r,"urgent count completion before neighbor mutation");
  // Interior edits change snapshot identity without invalidating center's border.
  // The old urgent classification was consumed when its job started.
  put(east,15,2,15,fluid_id(f,"water",0));++east.revision;
  require(open_world_renderer_update(&r,east) && !r.dirty.contains(center) && !r.dirty_urgent.contains(center),
      "interior neighbor mutation accidentally supplied retry urgency");
  require(world_mesh_refresh_one(r),"invalidated urgent count-to-emit failed");
  wait_jobs(r,"invalidated urgent emit completion");
  require(world_mesh_refresh_one(r) && r.halo_jobs->contains(center),
      "stale urgent correction returned to waiting for unrelated missing neighbors");
  require(r.columns.at(center).faces.get()==retained.get(),"stale urgent output replaced the retained mesh");
  settle(f,"coalesced_urgent_edit",false);

  open_world_renderer_set_center(&r,center.first,center.second,0);
  settle(f,"coalesced_before_reversal",false);
  open_world_renderer_set_center(&r,center.first,center.second,1);r.dirty.insert(center);
  require(world_mesh_refresh_one(r) && r.dirty.contains(center),"grown window did not wait for expected sources");
  open_world_renderer_set_center(&r,center.first+1,center.second+1,1);
  require(world_mesh_refresh_one(r) && r.dirty.contains(center),"center reversal lost waiting dirty work");
  open_world_renderer_set_center(&r,center.first,center.second,0);
  settle(f,"coalesced_reversal_shrink",false);
  open_world_renderer_set_center(&r,100,100,0);settle(f,"coalesced_retirement",false);
  std::printf("world_mesh_coalescing=passed arrival_jobs=%zu early_center_jobs=0 urgent_edit=1 urgent_retry=1 unload=1 reversal_shrink=1\n",dirtied.size());
}
}
void halo_lifecycle_cases(Fixture& f) {
  completion_waits=0;longest_completion_ms=0;
  run(f,"water");run(f,"lava");
  async_cases(f);reuse_cases(f);coalescing_cases(f);
  std::printf("world_mesh_completion_wait=passed calls=%llu max_ms=%.6f timeout_ms=1000 production_timeout_ns=0\n",
      static_cast<unsigned long long>(completion_waits),longest_completion_ms);
  std::puts("world_mesh_halo_lifecycle=passed families=2 signed_corner=-1,-1 gpu_update_refresh=actual");
}
}
