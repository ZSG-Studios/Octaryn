#include "SceneSessionInternal.h"
#include "SceneMemoryLedger.h"
#include "SceneGeometryPool.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include <algorithm>
#include <exception>

namespace octaryn::client::rendering {
int SceneSession::State::Job::execute(void* context) noexcept {
  auto& job=*static_cast<Job*>(context);
  try {
    if(job.cancelled)return 0;
    if(!job.expand)job.success=job.assets->prepare(job.part,job.prepared,job.error);
    else if(!job.exact)job.success=job.assets->expand(job.part,job.prepared,job.error,&job.cancelled);
    else {
      if(!job.detail) {
        job.detail=std::make_unique<virtual_geometry::SceneHierarchyDetail>();
        if(!job.detail->open(job.assets->hierarchy_path(),job.error,&job.cancelled)) {job.detail.reset();return 0;}
      }
      job.success=job.assets->prepare_detail(job.part,*job.detail,job.prepared,job.error,&job.cancelled);
    }
  }catch(const std::exception& failure){job.error=failure.what();}
  catch(...){job.error="scene geometry preparation exception";}
  return 0;
}
SceneSession::State::~State() {
  job.cancelled=true;
  if(job.task)octaryn_native_schedule_runtime_task_destroy(job.task);
  if(scheduler)octaryn_native_schedule_runtime_destroy(scheduler);
}
bool SceneSession::State::progress(WorldRenderer& renderer,std::span<const scene_geometry::Selection> selected) {
  const auto find=[&](std::uint32_t id) {return std::find_if(selected.begin(),selected.end(),[&](const auto& value){return value.part==id;});};
  if(job.task && !job.expand && find(job.part)==selected.end())job.cancelled=true;
  if(job.task && octaryn_native_schedule_runtime_task_ready(job.task)) {
    octaryn_native_schedule_runtime_report report{};
    const auto result=octaryn_native_schedule_runtime_task_result(job.task,&report);
    octaryn_native_schedule_runtime_task_destroy(job.task);job.task=nullptr;
    if(!job.cancelled) {
      if(result || !job.success) {error="scene geometry preparation failed: "+job.error;return false;}
      if(job.expand) {
        if(!assets.install_children(job.part,std::move(job.prepared),error))return false;
        pending=assets.render_part(job.part).children;
      }else if(const auto target=find(job.part);target!=selected.end()) {
        bool deferred{};auto map=assets.create(renderer,*target,std::move(job.prepared),error,&deferred);
        if(!map) {
          if(startup_committed && deferred) {
            retry_after[replacing]=frame+120;
            for(const auto id:pending)if(auto entry=entries.find(id);entry!=entries.end())retire(entry);
            pending.clear();removed.clear();replacing=virtual_geometry::invalid_id;error.clear();
          }else return false;
        }else entries.emplace(job.part,Entry{*target,std::move(map)});
      }
    }
    job.prepared={};job.success=false;job.error.clear();job.expand=false;job.exact=false;
  }
  if(job.task)return true;
  for(const auto id:pending)if(!entries.contains(id)) {
    job.part=id;job.assets=&assets;job.cancelled=false;job.success=false;job.expand=false;job.exact=false;job.error.clear();
    octaryn_native_schedule_runtime_job description{};description.job_id="scene_root_prepare";
    description.context=&job;description.execute=Job::execute;
    job.task=octaryn_native_schedule_runtime_submit_worker(scheduler,&description,1);
    if(!job.task) {error="scene geometry job submission failed";return false;}
    break;
  }
  return true;
}
}
