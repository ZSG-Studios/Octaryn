#include "WorldRendererInternal.h"
#include "WorldRayTracingState.h"
#include <optional>
#include <stdexcept>
namespace octaryn::client::rendering {
namespace {
using Coordinate=std::pair<std::int32_t,std::int32_t>;
template<class Visit> void snapshots(WorldRayTracing::State& ray,Visit&& visit) {
  const std::array entries{ray.current.get(),ray.frames[0].snapshot.get(),ray.frames[0].update_source.get(),
      ray.frames[1].snapshot.get(),ray.frames[1].update_source.get(),ray.snapshot_pool[0].get(),
      ray.snapshot_pool[1].get(),ray.snapshot_pool[2].get()};
  for(std::size_t index=0;index<entries.size();++index)
    if(entries[index] && std::find(entries.begin(),entries.begin()+index,entries[index])==entries.begin()+index)
      visit(*entries[index]);
}
WorldRetirementProgress progress(WorldRenderer& r) {
  std::uint64_t count=r.items.assets.size();
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
} // namespace
std::uint64_t open_world_renderer_retirement_remaining(WorldRenderer* r) {return r?remaining(*r):0;}
WorldRetirementProgress open_world_renderer_retirement_progress(WorldRenderer* r) {return r?progress(*r):WorldRetirementProgress{};}
bool open_world_renderer_begin_retirement(WorldRenderer* r) {
  if(!r)return false;
  if(r->retirement_started)return true;
  // This fence includes BLAS submissions outside the ordinary frame slots.
  if(!r->frame_queue.synchronize(r->queue,frame_fence_timeout_ms()) ||
      !open_world_renderer_flush(r))return false;
  r->retirement_started=true;
  r->items.poses.clear();r->items.instances.clear();r->items.batches.clear();r->items.history->poses.clear();
  r->items.buffers={};r->items.gbuffer.setNull();r->items.motion.setNull();
  if(r->ray_tracing) {
    auto& ray=*r->ray_tracing->state;if(ray.allocator)ray.allocator->stop();
    snapshots(ray,[](auto& scene){scene.item_assets.clear();});
    // The spare owns only two capacity buffers; the queue-wide fence is complete.
    ray.spare.reset();ray.bytes_dirty=true;
    ray.snapshot_pool={};
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
  if(r->ray_tracing && r->ray_tracing->state->allocator) {
    auto& allocator=r->ray_tracing->state->allocator;allocator->collect();
    if(allocator->finished())allocator.reset();
  }
  batch_size=std::min(batch_size,32u);
  for(std::uint32_t index=0;index<batch_size;++index) {
    if(!r->items.assets.empty())r->items.assets.pop_back();
    // Admit one bounded ownership unit at a time. Actual backend destruction
    // occurs during the next maintenance submission and feeds its next budget.
    if(r->ray_tracing)snapshots(*r->ray_tracing->state,[&](auto& scene) {
      const auto count=std::min(scene.columns.size(),std::size_t(1));
      scene.columns.erase(scene.columns.begin(),scene.columns.begin()+count);
    });
    if(r->ray_tracing) {
      auto& ray=*r->ray_tracing->state;
      std::optional<Coordinate> coordinate;
      if(!ray.columns.empty())coordinate=ray.columns.begin()->first;
      if(!ray.changed.empty() && (!coordinate || ray.changed.begin()->first<*coordinate))
        coordinate=ray.changed.begin()->first;
      if(coordinate) {
        ray.columns.erase(*coordinate);ray.changed.erase(*coordinate);ray.built_pass_counts.erase(*coordinate);
      }
    }
    if(double(SDL_GetTicksNS()-started)/1e6>=budget_ms)break;
  }
  const auto count=remaining(*r);
  if(!count && r->ray_tracing) {
    auto& ray=*r->ray_tracing->state;
    ray.current.reset();ray.spare.reset();ray.candidates.clear();ray.built_pass_counts.clear();
    ray.snapshot_pool={};
    for(auto& frame:ray.frames) {frame.snapshot.reset();frame.update_source.reset();}
    ray.stats={};
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
  // A failed frame may own an acquired image. Drain fences without reacquiring it.
  const bool present=!r->frame_failed && r->window && r->surface && width==r->width && height==r->height && width>0 && height>0 &&
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
