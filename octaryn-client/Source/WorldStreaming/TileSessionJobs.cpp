#include "TileSessionInternal.h"
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <exception>
namespace octaryn::client::rendering {
int TileSession::State::Job::execute(void* context) noexcept {
  auto& job=*static_cast<Job*>(context);
  try {
    if(job.cancelled.load())return 0;
    const auto started=std::chrono::steady_clock::now();
    std::error_code io_error;const auto bytes=std::filesystem::file_size(job.source,io_error);
    if(io_error || bytes>64ull*1024*1024) {job.error="tile_source_missing_or_exceeds_64MiB";return 0;}
    if(job.cancelled.load())return 0;
    job.prepared=std::make_unique<PreparedMapAsset>();
    if(!prepare_map_asset(job.source,*job.prepared,job.error,&job.cancelled,job.texture_cache,job.texture_reuse.get()))return 0;
    const auto prepared_at=std::chrono::steady_clock::now();
    if(job.cancelled.load())return 0;
    const auto& model=job.prepared->model;
    // A tile is the bounded build unit; large worlds must be cooked into cells.
    if(model.indices.size()/3>131072 || model.primitives.size()>2048) {job.error="tile_geometry_budget_exceeded";return 0;}
    std::vector<float> positions;positions.reserve(model.vertices.size()*3);
    for(const auto& vertex:model.vertices)positions.insert(positions.end(),vertex.position,vertex.position+3);
    character_motion::MeshCollision mesh{positions.data(),positions.size(),model.indices.data(),model.indices.size()};
    job.collision=character_motion::MeshCollisionScene::prepare_tile(mesh);
    if(!job.collision){job.error="tile_collision_prepare_failed";return 0;}
    job.success=!job.cancelled.load();
    std::printf("tile_prepare id=%u asset_cpu_ms=%.3f collision_cpu_ms=%.3f triangles=%zu cancelled=%u resident_texture_reuses=%u avoided_dds_bytes=%llu\n",job.tile,
        std::chrono::duration<double,std::milli>(prepared_at-started).count(),
        std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-prepared_at).count(),
        model.indices.size()/3,job.success?0u:1u,job.prepared->images.resident_reuses,
        static_cast<unsigned long long>(job.prepared->images.avoided_payload_bytes));
  } catch(const std::exception& failure) {job.error=failure.what();}
    catch(...) {job.error="tile_prepare_exception";}
  return 0;
}
TileSession::State::~State() {
  for(auto& job:jobs)job.cancelled=true;
  for(auto& job:jobs)if(job.task)octaryn_native_schedule_runtime_task_destroy(job.task);
  for(auto& entry:entries)if(entry.builder)destroy_map_renderer_build(entry.builder);
  if(scheduler)octaryn_native_schedule_runtime_destroy(scheduler);
}
bool TileSession::State::poll_jobs() {
  for(auto& job:jobs) {
    if(!job.task || !octaryn_native_schedule_runtime_task_ready(job.task))continue;
    octaryn_native_schedule_runtime_report report{};
    const int result=octaryn_native_schedule_runtime_task_result(job.task,&report);
    octaryn_native_schedule_runtime_task_destroy(job.task);job.task=nullptr;
    if(job.prepared) {
      texture_reuses+=job.prepared->images.resident_reuses;
      avoided_dds_bytes+=job.prepared->images.avoided_payload_bytes;
    }
    auto& entry=entries[job.tile];
    if(job.cancelled || !entry.wanted) {
      ++statistics.cancelled;entry.phase=Phase::Absent;
    } else if(result!=0 || !job.success) {
      error="tile_prepare_failed: "+job.source.generic_string()+": "+job.error;return false;
    } else {
      // GPU admission queries the backend's actual AS sizes before allocation.
      entry.reserved=map_prepared_geometry_bytes(*job.prepared)+65536;
      entry.prepared=std::move(job.prepared);entry.collision=std::move(job.collision);entry.phase=Phase::Prepared;
    }
    job.prepared.reset();job.collision.reset();job.texture_reuse.reset();job.error.clear();job.success=false;
  }
  return true;
}
bool TileSession::State::start_jobs() {
  constexpr std::uint64_t reservation=256ull*1024*1024;
  const auto maximum=std::min<std::uint64_t>(jobs.size(),budget.preparation_bytes/reservation);
  unsigned pending=0;
  for(const auto& entry:entries)if(entry.phase==Phase::Loading || entry.phase==Phase::Prepared || entry.phase==Phase::Uploading)++pending;
  for(unsigned index:priority) {
    if(pending>=maximum)break;
    auto& entry=entries[index];if(!entry.wanted || entry.phase!=Phase::Absent)continue;
    auto idle=std::find_if(jobs.begin(),jobs.end(),[](const auto& job){return !job.task;});
    if(idle==jobs.end())break;
    auto& job=*idle;job.tile=index;job.source=tiles.payload_directory()/tiles.tile(index)->file;
    job.texture_cache=tiles.texture_cache_directory();
    job.texture_reuse=texture_reuse_enabled?snapshot_map_texture_reuse(*textures,job.texture_cache):nullptr;
    job.cancelled=false;job.success=false;job.error.clear();
    octaryn_native_schedule_runtime_job task{};task.job_id="world_tile_prepare";task.context=&job;task.execute=Job::execute;
    job.task=octaryn_native_schedule_runtime_submit_worker(scheduler,&task,1);
    if(!job.task){error="tile_scheduler_submit_failed";return false;}
    entry.phase=Phase::Loading;++pending;
  }
  return true;
}
}
