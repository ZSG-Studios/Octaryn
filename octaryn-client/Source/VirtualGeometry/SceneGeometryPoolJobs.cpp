#include "SceneGeometryPoolInternal.h"
#include <algorithm>
#include <exception>

namespace octaryn::client::rendering::virtual_geometry {
int SceneGeometryPool::State::Job::execute(void* context) noexcept {
  auto& job=*static_cast<Job*>(context);
  try {
    if(job.cancelled.load())return 0;
    job.success=read_geometry_page(job.path,job.descriptor,job.decoded,job.error) && !job.cancelled.load();
  }catch(const std::exception& failure) {job.error=failure.what();}
  catch(...) {job.error="shared geometry decode exception";}
  return 0;
}
SceneGeometryPool::State::~State() {
  for(auto& job:jobs)job->cancelled=true;
  for(auto& job:jobs)if(job->task)octaryn_native_schedule_runtime_task_destroy(job->task);
  pool.setNull();allocation.reset();
}
void SceneGeometryPool::State::collect() {
  if(encoder)return;
  for(unsigned slot=0;slot<assets.size();++slot) {
    auto& asset=assets[slot];if(!asset.retiring)continue;
    const SceneGeometryHandle id{slot,asset.generation};bool busy=false;
    for(auto& job:jobs)if(job->asset==id) {
      job->cancelled=true;busy|=job->task!=nullptr;
      if(!job->task) {job->ready=false;job->decoded.clear();job->asset={};}
    }
    residency->discard_feedback(asset.pages);
    for(auto page:asset.pages) {
      if(owners[page].root) {
        busy|=std::max(owners[page].upload_signal,owners[page].consumer_signal)>completed;continue;
      }
      residency->pin(page,false);residency->evict(page);
      busy|=residency->valid(residency->handle(page));
    }
    if(busy || asset.last_signal>completed)continue;
    for(auto page:asset.pages) {
      auto& owner=owners[page];
      if(owner.root && !root_pages->release(owner.root,std::max(owner.upload_signal,owner.consumer_signal),completed)) {
        fail("packed root retirement generation mismatch");return;
      }
      owner={};free_pages.push_back(page);
    }
    hashes.erase(asset.hash);const auto generation=asset.generation;asset={};asset.generation=generation;
  }
}
bool SceneGeometryPool::State::poll() {
  if(fence && (SLANG_FAILED(fence->getCurrentValue(&completed)) || completed==UINT64_MAX))
    return fail("shared geometry fence failed");
  residency->complete({completed,completed,0,0});
  for(auto& pointer:jobs) {
    auto& job=*pointer;
    if(!job.task || !octaryn_native_schedule_runtime_task_ready(job.task))continue;
    octaryn_native_schedule_runtime_report report{};
    const auto result=octaryn_native_schedule_runtime_task_result(job.task,&report);
    octaryn_native_schedule_runtime_task_destroy(job.task);job.task=nullptr;
    const auto* asset=find(job.asset);
    if(!asset || asset->retiring || job.cancelled) {job.ready=false;job.decoded.clear();continue;}
    if(result!=0 || !job.success || job.decoded.size()!=page_bytes)
      return fail("shared geometry page decode failed: "+job.error);
    job.ready=true;++counters.decoded_pages;
  }
  collect();return true;
}
bool SceneGeometryPool::State::start_jobs() {
  for(auto& pointer:jobs) {
    auto& job=*pointer;if(job.task || job.ready)continue;
    auto requests=residency->take_requests(1);
    while(!requests.empty() && owners[requests.front().page].root &&
        (owners[requests.front().page].loading || owners[requests.front().page].upload_signal))requests=residency->take_requests(1);
    if(requests.empty())break;
    const auto page=requests.front().page;auto& owner=owners[page];auto* asset=find(owner.asset);
    if(!asset || asset->retiring)continue;
    PageHandle handle;
    if(!owner.root) {
      handle=residency->reserve(page,page_bytes);
      if(!handle && residency->evict_oldest())handle=residency->reserve(page,page_bytes);
      if(!handle) {residency->feedback(requests);break;}
    }
    owner.loading=true;job.asset=owner.asset;job.page=page;job.root=owner.root;
    job.handle=handle;job.path=asset->path;job.descriptor=asset->descriptors[owner.local];
    job.cancelled=false;job.success=false;job.ready=false;job.error.clear();job.decoded.clear();
    octaryn_native_schedule_runtime_job description{};
    description.job_id="scene_geometry_page_decode";description.context=&job;description.execute=Job::execute;
    job.task=octaryn_native_schedule_runtime_submit_worker(scheduler.get(),&description,1);
    if(!job.task)return fail("shared geometry scheduler submission failed");
  }
  return true;
}
}
