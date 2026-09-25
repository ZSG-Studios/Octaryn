#include "WorldRendererInternal.h"
#include "WorldRayTracingState.h"
#include "WorldMeshJob.h"
#include <optional>
#include <stdexcept>
namespace octaryn::client::rendering {
namespace {
using Coordinate=std::pair<std::int32_t,std::int32_t>;
template<class Visit> void snapshots(WorldRayTracing::State& ray,Visit&& visit) {
  const std::array entries{ray.current.get(),ray.frames[0].snapshot.get(),ray.frames[0].update_source.get(),
      ray.frames[1].snapshot.get(),ray.frames[1].update_source.get()};
  for(std::size_t index=0;index<entries.size();++index)
    if(entries[index] && std::find(entries.begin(),entries.begin()+index,entries[index])==entries.begin()+index)
      visit(*entries[index]);
}
WorldRetirementProgress progress(WorldRenderer& r) {
  std::uint64_t count=r.columns.size()+r.sources.size()+r.prediction_bases.size()+r.block_lights.columns.size();
  if(r.mesh_allocator)count+=r.mesh_allocator->pending()+(r.mesh_allocator->finished()?0u:1u);
  if(r.batch)for(const auto& frame:r.batch->frames)count+=frame.retained.size();
  for(const auto& frame:r.shadow_fallback.batch.frames)count+=frame.retained.size();
  if(r.ray_tracing) {
    auto& ray=*r.ray_tracing->state;
    if(ray.allocator)count+=ray.allocator->pending()+(ray.allocator->finished()?0u:1u);
    count+=ray.columns.size()+ray.changed.size();
    snapshots(ray,[&](auto& scene) {count+=scene.columns.size();});
  }
  rhi::ResourceRetirementInfo retirement{};
  if(!r.queue || !world_rhi_ok(r.queue->getResourceRetirementInfo(&retirement)))
    throw std::runtime_error("Renderer resource retirement status failed");
  // One final drain unit preserves monotonic owner progress while eligible
  // resources move into the backend worker, including its active deletion.
  return {count,retirement.pendingCount,retirement.pendingBufferBytes};
}
std::uint64_t remaining(WorldRenderer& r) {return progress(r).remaining();}
template<class Map> void next_coordinate(const Map& entries,std::optional<Coordinate>& coordinate) {
  if(!entries.empty() && (!coordinate || entries.begin()->first<*coordinate))coordinate=entries.begin()->first;
}
void retire_coordinate(WorldRenderer& r,Coordinate coordinate) {
  if(const auto found=r.columns.find(coordinate);found!=r.columns.end()) {
    std::uint64_t bytes{};
    const auto& column=found->second;
    for(auto* buffer:{column.faces.get(),column.fluids.get(),column.patches.get()})
      if(buffer)bytes+=buffer->getDesc().size;
    r.column_gpu_bytes-=std::min(r.column_gpu_bytes,bytes);
    r.resident_quads-=std::min(r.resident_quads,std::uint64_t(column.face_count));
    r.columns.erase(found);
  }
  r.sources.erase(coordinate);r.prediction_bases.erase(coordinate);r.block_lights.columns.erase(coordinate);
  if(r.ray_tracing) {
    auto& ray=*r.ray_tracing->state;
    ray.columns.erase(coordinate);ray.changed.erase(coordinate);ray.built_pass_counts.erase(coordinate);
  }
}
}
std::uint64_t open_world_renderer_retirement_remaining(WorldRenderer* r) {return r?remaining(*r):0;}
WorldRetirementProgress open_world_renderer_retirement_progress(WorldRenderer* r) {return r?progress(*r):WorldRetirementProgress{};}
bool open_world_renderer_begin_retirement(WorldRenderer* r) {
  if(!r)return false;
  if(r->retirement_started)return true;
  // This fence includes mesh/BLAS submissions outside the ordinary frame slots.
  if(!r->frame_queue.synchronize(r->queue,frame_fence_timeout_ms()) ||
      !open_world_renderer_flush(r))return false;
  if(r->mesh_allocator)r->mesh_allocator->stop();
  r->retirement_started=true;
  r->draw_list.visible.clear();r->draw_list.quads=0;r->drawn_columns=0;r->drawn_quads=0;
  r->shadow_fallback.batch.columns.clear();
  r->dirty.clear();r->dirty_urgent.clear();r->predicted_edits.clear();
  // These owners are bounded to a handful of jobs; their submissions have ended.
  r->delivery_jobs.reset();r->halo_jobs.reset();r->qualification_mesh.reset();
  if(r->ray_tracing) {
    auto& ray=*r->ray_tracing->state;if(ray.allocator)ray.allocator->stop();
    // The spare owns only two capacity buffers; the queue-wide fence is complete.
    ray.spare.reset();ray.bytes_dirty=true;
    for(auto& job:ray.jobs) {
      job.submission.setNull();job.pending.reset();job.refit_source.reset();job.allocation.reset();job.signal=0;
    }
  }
  return true;
}
std::uint64_t open_world_renderer_retire_step(WorldRenderer* r,std::uint32_t batch_size,double budget_ms) {
  if(!r)return 0;
  if(!r->retirement_started || !batch_size || !(budget_ms>0.0))return remaining(*r);
  const auto started=SDL_GetTicksNS();
  if(r->mesh_allocator) {
    r->mesh_allocator->collect();
    if(r->mesh_allocator->finished())r->mesh_allocator.reset();
  }
  if(r->ray_tracing && r->ray_tracing->state->allocator) {
    auto& allocator=r->ray_tracing->state->allocator;allocator->collect();
    if(allocator->finished())allocator.reset();
  }
  batch_size=std::min(batch_size,32u);
  for(std::uint32_t index=0;index<batch_size;++index) {
    // Admit one bounded ownership unit at a time. Actual backend destruction
    // occurs during the next maintenance submission and feeds its next budget.
    if(r->batch)for(auto& frame:r->batch->frames) {
      // Two buffers per column may occur in each of the two raster passes.
      const auto count=std::min(frame.retained.size(),std::size_t(4));
      frame.retained.resize(frame.retained.size()-count);
    }
    for(auto& frame:r->shadow_fallback.batch.frames) {
      const auto count=std::min(frame.retained.size(),std::size_t(2));
      frame.retained.erase(frame.retained.begin(),frame.retained.begin()+count);
    }
    if(r->ray_tracing)snapshots(*r->ray_tracing->state,[&](auto& scene) {
      const auto count=std::min(scene.columns.size(),std::size_t(1));
      // Snapshot construction follows the ordered ray column map. Retire from
      // the same end so current snapshots do not defer most BLAS releases.
      scene.columns.erase(scene.columns.begin(),scene.columns.begin()+count);
    });
    std::optional<Coordinate> coordinate;
    next_coordinate(r->columns,coordinate);next_coordinate(r->sources,coordinate);
    next_coordinate(r->prediction_bases,coordinate);next_coordinate(r->block_lights.columns,coordinate);
    if(r->ray_tracing) {
      next_coordinate(r->ray_tracing->state->columns,coordinate);
      next_coordinate(r->ray_tracing->state->changed,coordinate);
    }
    if(coordinate)retire_coordinate(*r,*coordinate);
    if(double(SDL_GetTicksNS()-started)/1e6>=budget_ms)break;
  }
  const auto count=remaining(*r);
  if(!count) {
    r->resident_quads=0;r->column_gpu_bytes=0;r->block_lights.source_count=0;
    if(r->ray_tracing) {
      auto& ray=*r->ray_tracing->state;
      ray.current.reset();ray.spare.reset();ray.candidates.clear();ray.built_pass_counts.clear();
      for(auto& frame:ray.frames) {frame.snapshot.reset();frame.update_source.reset();}
      ray.stats={};
    }
  }
  return count;
}
bool open_world_renderer_retirement_frame(WorldRenderer* r) {
  if(!r || !r->retirement_started)return false;
  r->active_frame=r->frame_queue.slot(r->frames);
  if(!r->frame_queue.wait(r->active_frame,frame_fence_timeout_ms()))return false;
  int width{},height{};
  if(r->window)SDL_GetWindowSizeInPixels(r->window,&width,&height);
  Slang::ComPtr<rhi::ITexture> image;
  // A resized/minimized window still gets a real maintenance submission. Do
  // not allocate new presentation targets while retiring the old renderer.
  const bool present=r->window && r->surface && width==r->width && height==r->height && width>0 && height>0 &&
      !(SDL_GetWindowFlags(r->window)&SDL_WINDOW_MINIMIZED);
  if(present && !world_rhi_ok(r->surface->acquireNextImage(image.writeRef())))return false;
  auto commands=r->queue->createCommandEncoder();if(!commands)return false;
  if(r->target().color) {
    float black[4]{};
    commands->clearTextureFloat(r->target().color,{0,1,0,1},black);
    if(image) {
      const rhi::SubresourceRange range{0,1,0,1};
      commands->copyTexture(image,range,{},r->target().color,range,{},
          {static_cast<std::uint32_t>(r->width),static_cast<std::uint32_t>(r->height),1});
      commands->setTextureState(image,rhi::ResourceState::Present);
    }
  }
  auto submission=commands->finish();
  if(!submission || !r->frame_queue.submit(r->queue,submission,r->active_frame))return false;
  if(image && !world_rhi_ok(r->surface->present()))return false;
  if(!r->frame_queue.wait(r->active_frame,frame_fence_timeout_ms()))return false;
  ++r->frames;return r->debug.errors.load()==0;
}
}
