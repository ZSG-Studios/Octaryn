#include "SceneCollisionState.h"
#include "CollisionBudget.h"
#include <cstdio>
#include <exception>

namespace octaryn::character_motion {
SceneCollisionResidency::State::~State() {
  canceled=true;
  if(job.task)octaryn_native_schedule_runtime_task_destroy(job.task);
  if(scheduler)octaryn_native_schedule_runtime_destroy(scheduler);
}
int SceneCollisionResidency::State::Job::execute(void* pointer) {
  auto& job=*static_cast<Job*>(pointer);
  try {
    const auto& part=job.owner->catalog.parts[job.part];assets::GltfTriangleWindow window;
    const auto& order=job.owner->catalog.orders[job.owner->catalog.part_orders[job.part]];
    if(order.path.empty()) {
      if(!job.owner->reader.read(part.mesh,part.primitive,part.first_triangle,std::uint32_t(part.triangle_count),window,job.error))return -1;
    }else {
      const auto unchanged=[&] {
        return order.stamp==std::filesystem::last_write_time(order.path) && order.bytes==std::filesystem::file_size(order.path);
      };
      if(!unchanged()) {job.error="collision triangle order changed after validation";return -1;}
      std::vector<std::uint64_t> triangles;
      if(!scene_geometry::read_spatial_triangle_order(order.path,order.config,part.first_triangle,
          std::uint32_t(part.triangle_count),triangles,job.error))return -1;
      if(!unchanged()) {job.error="collision triangle order changed during read";return -1;}
      if(!job.owner->reader.read(part.mesh,part.primitive,triangles,window,job.error))return -1;
    }
    if(!assets::transform_gltf_triangles(window,job.owner->catalog.instances[job.instance].transform,job.error))return -1;
    job.prepared=MeshCollisionScene::prepare_tile({window.positions.data(),window.positions.size(),window.indices.data(),window.indices.size()});
    if(!job.prepared) {job.error="scene collision BVH preparation failed";return -1;}
    job.triangles=window.indices.size()/3;job.success=!job.owner->canceled.load();return job.success?0:-1;
  }catch(const std::exception& failure) {job.error=failure.what();return -1;}
}
std::uint64_t SceneCollisionResidency::State::reservation(std::uint32_t part) const {
  return scene_geometry::collision_part_reservation(catalog.parts[part].triangle_count);
}
bool SceneCollisionResidency::State::poll() {
  if(!job.task || !octaryn_native_schedule_runtime_task_ready(job.task))return true;
  const auto result=octaryn_native_schedule_runtime_task_result(job.task,nullptr);
  octaryn_native_schedule_runtime_task_destroy(job.task);job.task=nullptr;
  if(result || !job.success) {error=job.error.empty()?"scene collision preparation failed":job.error;++statistics.failed;return false;}
  auto found=entries.find(job.key);
  if(found!=entries.end() && found->second.wanted>=Clock::now()) {
    if(!collision->set_tile(job.key,std::move(job.prepared))) {error="scene collision publication failed";++statistics.failed;return false;}
    found->second.resident=true;found->second.bytes=job.bytes;found->second.triangles=job.triangles;++statistics.loads;
    std::printf("scene_collision_published part=%u instance=%u triangles=%llu bytes=%llu\n",job.part,job.instance,
        static_cast<unsigned long long>(job.triangles),static_cast<unsigned long long>(job.bytes));
  }
  job.prepared.reset();return true;
}
bool SceneCollisionResidency::State::start(std::uint64_t key) {
  job.key=key;job.part=std::uint32_t(key>>32);job.instance=std::uint32_t(key);job.owner=this;
  job.success=false;job.error.clear();job.bytes=reservation(job.part);
  octaryn_native_schedule_runtime_job work{};work.job_id="scene_collision_prepare";work.context=&job;work.execute=Job::execute;
  job.task=octaryn_native_schedule_runtime_submit_worker(scheduler,&work,1);
  if(!job.task) {error="scene collision scheduling failed";++statistics.failed;return false;}
  return true;
}
void SceneCollisionResidency::State::refresh() {
  statistics.resident=0;statistics.resident_bytes=0;statistics.preparing=job.task?1u:0u;
  for(const auto& [key,entry]:entries)if(entry.resident) {++statistics.resident;statistics.resident_bytes+=entry.bytes;}
  statistics.reserved_bytes=statistics.resident_bytes+(job.task?job.bytes:0);
}
}
