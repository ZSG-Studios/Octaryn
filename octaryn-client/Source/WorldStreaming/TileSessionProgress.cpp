#include "TileSessionInternal.h"
#include "MapRayBudget.h"
#include "DeviceMemory.h"
#include "TileBudget.h"
#include <algorithm>
#include <cstdio>
namespace octaryn::client::rendering {
bool TileSession::State::progress(rhi::ICommandEncoder* commands,bool ray_required,const MapRaySubmitScope* profile) {
  if(recorded){error="tile_upload_submission_not_acknowledged";return false;}
  if(!progress_rays(ray_required,profile))return false;
  refresh();
  Entry* upload=nullptr;
  for(auto& entry:entries)if(entry.phase==Phase::Uploading) {upload=&entry;break;}
  if(!upload)for(unsigned index:priority) {
    auto& entry=entries[index];if(entry.phase!=Phase::Prepared || !entry.wanted)continue;
    std::uint64_t ray_bytes{};
    if(ray_required && !map_ray_prepare_budget(device,size_query_address,entry.prepared->model,ray_bytes)) {
      error="tile_prepared_ray_size_query_failed";return false;
    }
    entry.reserved=map_prepared_geometry_bytes(*entry.prepared)+ray_bytes+65536;
    const auto images=map_texture_pool_additional_bytes(*textures,*entry.prepared);
    if(entry.reserved+images>budget.gpu_bytes) {error="tile_exceeds_gpu_budget";return false;}
    const auto required=statistics.resident_bytes+statistics.reserved_bytes+statistics.retired_bytes+
        statistics.texture_bytes+entry.reserved+images;
    if(required>budget.gpu_bytes) {
      // Under pressure, discard only prefetch outside the wanted set. Actor tiles
      // always remain wanted; their collision cannot disappear to admit scenery.
      bool evicted=false;
      for(auto candidate=priority.rbegin();candidate!=priority.rend();++candidate) {
        if(entries[*candidate].phase==Phase::Ready && !entries[*candidate].wanted) {evict(*candidate);evicted=true;break;}
      }
      if(!evicted && retired.empty() && !statistics.uploading) {
        error="tile_wanted_working_set_exceeds_gpu_budget required_bytes="+std::to_string(required)+
            " budget_bytes="+std::to_string(budget.gpu_bytes);return false;
      }
      break;
    }
    const auto memory=device_memory_stats(device->getInfo());
    if(memory.sample_id!=memory_sample) {memory_sample=memory.sample_id;admitted_since_sample=0;}
    const auto additional=entry.reserved+images+statistics.reserved_bytes;
    if(memory.budget_available && !tile_gpu_admits(additional,memory.local_budget,memory.local_usage,
        admitted_since_sample,memory.reserved_capacity_bytes)) {
      if(!memory_pause_logged)std::printf("tile_admission_paused reason=os_gpu_70_percent usage=%llu budget=%llu additional=%llu admitted=%llu capacity_reserved=%llu\n",
          memory.local_usage,memory.local_budget,additional,admitted_since_sample,memory.reserved_capacity_bytes);
      memory_pause_logged=true;break;
    }
    if(!memory.budget_available && !memory_pause_logged) {
      std::printf("tile_gpu_budget coverage=explicit_only reason=os_budget_unavailable limit=%llu\n",budget.gpu_bytes);
      memory_pause_logged=true;
    }
    admitted_since_sample+=additional;
    const auto setup_start=std::chrono::steady_clock::now();
    entry.builder=begin_map_renderer_build(device,rhi::Format::RGBA16Float,rhi::Format::D32Float,
        std::move(*entry.prepared),textures,ray_required);
    std::printf("tile_resource_setup id=%u cpu_ms=%.3f os_budget_available=%u reserved_bytes=%llu\n",index,
        std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-setup_start).count(),
        memory.budget_available?1u:0u,additional);
    entry.prepared.reset();
    if(!entry.builder){error="tile_gpu_begin_failed";return false;}
    entry.cancelled_upload=false;entry.phase=Phase::Uploading;upload=&entry;break;
  }
  if(!upload)return true;
  const auto result=pump_map_renderer_build(upload->builder,commands,budget.upload_bytes,budget.upload_ms);
  const auto upload_progress=map_renderer_build_progress(upload->builder);
  commands_recorded=commands_recorded || upload_progress.commands_recorded;
  if(upload_progress.uploaded_this_pump>budget.upload_bytes) {error="tile_upload_budget_exceeded";return false;}
  if(upload_progress.cpu_ms>0 || upload_progress.uploaded_this_pump || result==MapBuildStatus::NeedsSubmission)
    std::printf("tile_upload bytes=%llu budget=%llu cpu_ms=%.3f pending_bytes=%llu maximum_call_ms=%.3f material_cpu_ms=%.3f material_records=%u\n",
        static_cast<unsigned long long>(upload_progress.uploaded_this_pump),static_cast<unsigned long long>(budget.upload_bytes),
        upload_progress.cpu_ms,static_cast<unsigned long long>(upload_progress.pending_bytes),
        upload_progress.max_upload_call_ms,upload_progress.material_ms,upload_progress.material_records);
  if(result==MapBuildStatus::Failed){error="tile_gpu_upload_failed";return false;}
  if(result==MapBuildStatus::NeedsSubmission) {
    if(commands_recorded)recorded=upload->builder;
    else {
      // The final data may have filled the previous pump exactly. Its fence
      // already covers all writes; reaching the CPU-only final stage adds none.
      if(!last_fence){error="tile_upload_completion_missing_fence";return false;}
      map_renderer_build_submitted(upload->builder,last_fence,last_signal);
    }
  }
  if(result==MapBuildStatus::Ready) {
    auto* map=take_map_renderer_build(upload->builder);
    if(!map){error="tile_gpu_take_failed";return false;}
    destroy_map_renderer_build(upload->builder);upload->builder=nullptr;
    upload->map=std::shared_ptr<MapRenderer>(map,destroy_map_renderer);upload->phase=Phase::Ray;
    if(upload->cancelled_upload) {
      retired.push_back({std::move(upload->map),last_fence,last_signal,upload->reserved});
      upload->collision.reset();upload->phase=Phase::Absent;upload->reserved=0;++statistics.cancelled;
    }
  }
  return true;
}
}
