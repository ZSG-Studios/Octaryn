#include "WorldGeometryRay.h"
#include "WorldGeometry.h"
#include "SceneRayScheduler.h"
#include "GeometryStream.h"
#include "GeometryBudget.h"
#include "RayAdmission.h"
#include "../MapWorld/MapRendererInternal.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include "../Rendering/RenderBackend/SlangShaderPath.h"
#include "../Rendering/RenderBackend/FrameWatchdog.h"
#include "../Rendering/RenderBackend/DeviceMemory.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <chrono>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
struct RayCpuScope {
  const char* asset;const char* phase;std::uint64_t frame;
  std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
  ~RayCpuScope() {
    const auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    if(milliseconds>=5)std::printf("world_geometry_ray_cpu asset=%s frame=%llu phase=%s milliseconds=%.3f\n",
        asset,static_cast<unsigned long long>(frame),phase,milliseconds);
  }
};
}
static_assert(sizeof(MapRayGeometry)==160);
std::uint64_t geometry_ray_reservation(const MapGeometryCache& cache) {
  return geometry_ray_reservation(cache.clusters);
}
std::uint64_t geometry_ray_instance_reservation(const MapGeometryCache& cache,std::uint64_t nodes) {
  return geometry_ray_instance_reservation(cache.clusters,nodes);
}
struct WorldGeometryRay::State {
  RayGeometry ray;
  SelectionTopology topology;
  RayGeometryConfig config;
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::IFence> fence;
  Slang::ComPtr<rhi::ICommandBuffer> pending;
  std::vector<std::uint32_t> selected,pending_selection,rejected;
  std::uint64_t signal{},last_check{},instance_revision{};
  std::uint64_t blocked_memory_bytes{UINT64_MAX};
  RayAdmission admission;
  SelectionView pending_view;
  float admitted_error{1};
  bool initialized{};
  std::string error;
  bool fail(const std::string& reason) {error=reason;return false;}
  bool submit(WorldRenderer& renderer,Slang::ComPtr<rhi::ICommandEncoder> commands) {
    auto submission=commands->finish();
    if(!submission) {commands.setNull();ray.cancel_unsubmitted();return fail("ray geometry command finish failed");}
    auto* buffer=submission.get();auto* signal_fence=fence.get();const auto next=++signal;
    rhi::SubmitDesc submit{};submit.commandBuffers=&buffer;submit.commandBufferCount=1;
    submit.signalFences=&signal_fence;submit.signalFenceValues=&next;submit.signalFenceCount=1;
    if(SLANG_FAILED(renderer.queue->submit(submit)))frame_gpu_shutdown_failed("virtual_geometry_ray_submit");
    pending=std::move(submission);
    if(!ray.submitted(fence,next))frame_gpu_shutdown_failed("virtual_geometry_ray_fence_registration");
    return true;
  }
};
WorldGeometryRay::WorldGeometryRay():state_(std::make_unique<State>()) {}
WorldGeometryRay::~WorldGeometryRay() {
  auto& s=*state_;
  if(s.pending) {
    rhi::IFence* fence=s.fence;
    if(SLANG_FAILED(s.device->waitForFences(1,&fence,&s.signal,true,frame_fence_timeout_ms()*1000000ull)))
      frame_gpu_shutdown_failed("virtual_geometry_ray_retirement");
  }
}
std::shared_ptr<const RaySnapshot> WorldGeometryRay::snapshot() const {return state_->ray.snapshot();}
const std::string& WorldGeometryRay::error() const {return state_->error;}
bool WorldGeometryRay::memory_blocked() const {
  return bool(state_->config.scheduler) && state_->blocked_memory_bytes!=UINT64_MAX;
}
std::uint64_t WorldGeometryRay::gpu_bytes() const {return state_->ray.gpu_bytes();}
bool WorldGeometryRay::idle() const {
  const auto& s=*state_;if(!s.pending)return true;
  std::uint64_t value{};
  if(SLANG_FAILED(s.fence->getCurrentValue(&value)) || value==UINT64_MAX)
    frame_gpu_shutdown_failed("virtual_geometry_ray_retirement_status");
  return value>=s.signal;
}
bool WorldGeometryRay::prepare(WorldRenderer& renderer,MapRenderer& map,const WorldCamera& camera,bool notify_scene) {
  auto& s=*state_;
  if(!map.geometry)return s.fail("ray geometry requires virtual geometry ownership");
  if(!map.geometry->ready())return true;
  auto& stream=map.geometry->stream();
  SelectionView view{{camera.x,camera.y,camera.z},
      .5f*float(renderer.render_height())/std::tan(std::clamp(camera.vertical_fov,.2f,2.7f)*.5f),s.admitted_error};
  const auto cut_error=[&](const SelectionView& query) {
    return ray_cut_error(stream.asset(),s.selected,query,map.geometry_instances);
  };
  const auto retain_complete=[&] {
    s.admission.reject(s.pending_view,cut_error(s.pending_view));
    s.rejected=std::move(s.pending_selection);s.error.clear();s.last_check=renderer.frames;
    std::printf("world_geometry_ray_admission asset=%s action=retain_complete reason=budget pixels=%.6g requested_pixels=%.6g selection_pixels=%.6g retry=camera_change\n",
        stream.asset().source_hash.c_str(),cut_error(view),s.config.error_pixels,s.admitted_error);
  };
  if(!s.initialized) {
    s.config.maximum_clusters=65535;
    s.config.error_pixels=map.geometry->requested_error_pixels();
    s.admitted_error=s.config.error_pixels;
    s.config.maximum_resident_bytes=geometry_ray_reservation(map.geometry_cache);
    s.config.maximum_build_bytes=s.config.maximum_resident_bytes/2;
    s.config.scheduler=map.scene_ray_scheduler;
    if(s.config.scheduler) {
      s.config.build_local_tlas=false;
      s.config.maximum_resident_bytes=s.config.maximum_build_bytes=s.config.scheduler->ledger()->stats().limit;
      s.config.admit_publication=[&renderer,&map](std::shared_ptr<const RaySnapshot> replacement,std::string& error) {
        const bool active=std::any_of(renderer.resident_maps.begin(),renderer.resident_maps.end(),
            [&](const auto& owner){return owner.get()==&map;});
        if(!active)return true;
        const auto admission=world_ray_admit_scene(renderer,renderer.resident_maps,&map,std::move(replacement));
        if(admission==SceneRayAdmission::Failed)error=renderer.status;
        return admission==SceneRayAdmission::Ready;
      };
    }
    const auto memory=device_memory_stats(renderer.device->getInfo(),true);
    if(!s.config.scheduler && memory.budget_available && (memory.local_usage>=memory.local_budget ||
        s.config.maximum_build_bytes>memory.local_budget-memory.local_usage))
      return s.fail("virtual geometry ray build exceeds available GPU memory budget");
    const auto shader=resolve_slang_shader_path("octaryn-client/Shaders/VirtualGeometry/RayExpand.slang");
    if(!build_selection_topology(stream.asset(),s.topology,s.error) ||
        !s.ray.initialize(renderer.device,stream.asset(),shader.c_str(),s.config))
      return s.fail(s.error.empty()?s.ray.error():s.error);
    if(SLANG_FAILED(renderer.device->createFence({},s.fence.writeRef())))return s.fail("ray geometry fence creation failed");
    s.device=renderer.device;s.initialized=true;
  }
  if(s.instance_revision!=map.geometry_instances_revision) {
    s.instance_revision=map.geometry_instances_revision;s.admission.clear();s.rejected.clear();s.last_check=0;
    s.admitted_error=s.config.error_pixels;
    s.blocked_memory_bytes=UINT64_MAX;
  }
  if(s.pending) {
    std::uint64_t completed{};
    if(SLANG_FAILED(s.fence->getCurrentValue(&completed)) || completed==UINT64_MAX)return s.fail("ray geometry fence failed");
    if(completed<s.signal)return true;
    bool published{};
    {RayCpuScope timing{stream.asset().source_hash.c_str(),"poll",renderer.frames};published=s.ray.poll();}
    if(!published && !s.ray.error().empty())return s.fail(s.ray.error());
    s.pending.setNull();
    if(!published && s.ray.budget().deferred) {
      s.last_check=renderer.frames;s.pending_selection.clear();
      if(s.config.scheduler) {
        s.blocked_memory_bytes=s.config.scheduler->ledger()->stats().used;
        s.admission.reject(view,cut_error(view));
      }
      return true;
    }
    if(published) {
      s.selected=std::move(s.pending_selection);s.rejected.clear();s.admission.clear();
      if(notify_scene)renderer.scene_changes.notify_column(0,0,0,0,SceneChangeKind::Modified);
      const auto scene=s.ray.snapshot();
      std::printf("world_geometry_ray_ready asset=%s generation=%llu clusters=%zu batches=%zu bytes=%llu budget=%llu offscreen=complete materials=authored error_pixels=%.6g requested_pixels=%.6g vertex_stride=%u\n",
        scene->source_hash.c_str(),static_cast<unsigned long long>(scene->generation),scene->clusters.size(),scene->batches.size(),
        static_cast<unsigned long long>(scene->bytes),static_cast<unsigned long long>(s.config.maximum_resident_bytes),cut_error(view),s.config.error_pixels,scene->vertex_stride);
      std::printf("world_geometry_ray_selection asset=%s instance_union=%u instances=%zu requested_pixels=%.6g selection_pixels=%.6g error_pixels=%.6g\n",
          scene->source_hash.c_str(),unsigned(!map.geometry_instances.empty()),map.geometry_instances.size(),s.config.error_pixels,s.admitted_error,cut_error(view));
    }
  }
  if(s.ray.continuation_ready()) {
    auto commands=renderer.queue->createCommandEncoder();if(!commands)return s.fail("ray continuation encoder failed");
    bool recorded{};
    {RayCpuScope timing{stream.asset().source_hash.c_str(),"continue",renderer.frames};recorded=s.ray.record_continue(commands);}
    if(!recorded) {
      const bool limited=s.ray.budget().limited,retain=limited && bool(s.ray.snapshot());
      {RayCpuScope timing{stream.asset().source_hash.c_str(),"discard",renderer.frames};commands.setNull();s.ray.cancel_unsubmitted();}
      if(retain) {retain_complete();return true;}
      if(limited) {
        s.rejected=std::move(s.pending_selection);s.admitted_error=std::min(std::numeric_limits<float>::max()/4,
            std::max(1.f,s.admitted_error))*4;s.error.clear();return true;
      }
      return s.fail(s.ray.error());
    }
    std::printf("world_geometry_ray_progress asset=%s submission=%llu bytes=%llu budget=%llu\n",stream.asset().source_hash.c_str(),
        static_cast<unsigned long long>(s.signal+1),static_cast<unsigned long long>(s.ray.budget().build_bytes),
        static_cast<unsigned long long>(s.config.maximum_build_bytes));
    return s.submit(renderer,std::move(commands));
  }
  if(const auto scene=s.ray.snapshot()) {
    s.ray.poll();if(!s.ray.error().empty())return s.fail(s.ray.error());
    // Completed frame snapshots can still retain the previous generation.
    // Their short lifetime must not become a permanent quality admission failure.
    if(s.ray.gpu_bytes()>scene->bytes)return true;
    if(renderer.frames-s.last_check<8)return true;
  }
  s.last_check=renderer.frames;
  if(s.config.scheduler && s.blocked_memory_bytes!=UINT64_MAX) {
    if(s.blocked_memory_bytes==s.config.scheduler->ledger()->stats().used &&
        !s.admission.allows(view,cut_error(view)))return true;
    s.blocked_memory_bytes=UINT64_MAX;s.admission.clear();
  }
  view.error_pixels=s.admitted_error;
  if(s.ray.snapshot() && s.admission.limited()) {
    const auto error=cut_error(view);
    if(!s.admission.allows(view,error))return true;
    // The last accepted cut supplies a measured quality bound for this next
    // camera, rather than reattempting the failed finest cut on every page.
    view.error_pixels=std::max(view.error_pixels,error);
  }
  const auto pages=stream.page_table();SelectionResult selection;
  Slang::ComPtr<rhi::ICommandEncoder> commands;
  for(unsigned attempt=0;;++attempt) {
    std::vector<InstanceSelectionView> views;views.reserve(map.geometry_instances.size());
    for(const auto& transform:map.geometry_instances)views.push_back(instance_selection_view(view,transform));
    bool selected{};
    {RayCpuScope timing{stream.asset().source_hash.c_str(),"select",renderer.frames};
      selected=views.empty()?select_geometry(s.topology,pages,view,s.config.maximum_clusters,s.config.feedback_capacity,selection,s.error):
          select_geometry_instances(s.topology,pages,views,s.config.maximum_clusters,s.config.feedback_capacity,selection,s.error);}
    if(!selected)return false;
    map.geometry->request_ray_pages(selection.requests);
    // A budget change never removes roots or replaces the last complete snapshot.
    if(selection.clusters==s.selected && s.ray.snapshot()) {s.admitted_error=view.error_pixels;return true;}
    if(selection.clusters==s.rejected) {
      if(s.ray.snapshot())return true;
      if(view.error_pixels==std::numeric_limits<float>::max())return s.fail("minimum compacted ray cut exceeds memory budget");
      view.error_pixels=attempt>=15?std::numeric_limits<float>::max():std::min(std::numeric_limits<float>::max()/4,
          std::max(1.f,view.error_pixels))*4;continue;
    }
    commands=renderer.queue->createCommandEncoder();
    if(!commands)return s.fail("ray geometry command encoder failed");
    bool recorded{};
    {RayCpuScope timing{stream.asset().source_hash.c_str(),"record",renderer.frames};
      recorded=views.empty()?s.ray.record(commands,stream.pool(),pages,view):s.ray.record(commands,stream.pool(),pages,views);}
    if(recorded)break;
    {RayCpuScope timing{stream.asset().source_hash.c_str(),"discard",renderer.frames};commands.setNull();s.ray.cancel_unsubmitted();}
    const auto& budget=s.ray.budget();
    if(budget.deferred) {s.error.clear();return true;}
    if(!budget.limited)return s.fail(s.ray.error());
    if(s.config.scheduler) {
      // SceneSession retains the complete parent domain. A child may not admit
      // itself at a looser threshold to bypass the hierarchy transaction.
      s.blocked_memory_bytes=s.config.scheduler->ledger()->stats().used;
      s.admission.reject(view,cut_error(view));s.error.clear();
      std::printf("world_geometry_ray_admission asset=%s action=defer_complete reason=scene_budget pixels=%.6g requested_pixels=%.6g used=%llu budget=%llu\n",
          stream.asset().source_hash.c_str(),cut_error(view),s.config.error_pixels,
          static_cast<unsigned long long>(s.blocked_memory_bytes),
          static_cast<unsigned long long>(s.config.scheduler->ledger()->stats().limit));
      return true;
    }
    const bool retain=bool(s.ray.snapshot());
    std::printf("world_geometry_ray_admission asset=%s clusters=%u triangles=%u expansion=%llu blas=%llu scratch=%llu separate_scratch=%llu build=%llu budget=%llu resident=%llu action=%s\n",
        stream.asset().source_hash.c_str(),budget.clusters,budget.triangles,
        static_cast<unsigned long long>(budget.expansion_bytes),static_cast<unsigned long long>(budget.blas_bytes),
        static_cast<unsigned long long>(budget.scratch_bytes),static_cast<unsigned long long>(budget.separate_scratch_bytes),
        static_cast<unsigned long long>(budget.build_bytes),static_cast<unsigned long long>(s.config.maximum_build_bytes),
        static_cast<unsigned long long>(budget.resident_bytes),retain?"retain_complete":"coarsen_complete");
    if(retain) {s.pending_view=view;s.pending_selection=selection.clusters;retain_complete();return true;}
    if(view.error_pixels==std::numeric_limits<float>::max()) {
      if(s.ray.snapshot()) {s.error.clear();return true;}
      return s.fail("minimum complete ray cut exceeds memory budget");
    }
    view.error_pixels=attempt>=15?std::numeric_limits<float>::max():std::max(1.f,view.error_pixels*4);
  }
  s.admitted_error=view.error_pixels;
  s.pending_view=view;
  const auto& budget=s.ray.budget();
  std::printf("world_geometry_ray_build asset=%s clusters=%u triangles=%u build=%llu budget=%llu scratch=%llu separate_scratch=%llu uncompacted=%llu\n",
      stream.asset().source_hash.c_str(),budget.clusters,budget.triangles,
      static_cast<unsigned long long>(budget.build_bytes),static_cast<unsigned long long>(s.config.maximum_build_bytes),
      static_cast<unsigned long long>(budget.scratch_bytes),static_cast<unsigned long long>(budget.separate_scratch_bytes),
      static_cast<unsigned long long>(budget.uncompacted_build_bytes));
  // Extraction precedes all later uploads on this same queue. Published ray
  // buffers are independent copies, so page eviction cannot alter a snapshot.
  s.pending_selection=std::move(selection.clusters);return s.submit(renderer,std::move(commands));
}
bool prepare_geometry_ray(WorldRenderer& renderer,MapRenderer& map,const WorldCamera& camera,bool notify_scene) {
  if(!renderer.ray_requested || !world_ray_available(renderer))return true;
  if(!map.geometry_ray)map.geometry_ray=std::make_shared<WorldGeometryRay>();
  if(map.geometry_ray->prepare(renderer,map,camera,notify_scene))return true;
  renderer.status=map.geometry_ray->error();return false;
}
}
