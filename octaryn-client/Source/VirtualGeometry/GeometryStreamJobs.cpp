#include "GeometryStreamInternal.h"
#include <algorithm>
#include <exception>

namespace octaryn::client::rendering::virtual_geometry {
int GeometryStream::State::Job::execute(void* context) noexcept {
  auto& job=*static_cast<Job*>(context);
  try {
    if(job.cancelled.load())return 0;
    job.success=read_geometry_page(job.path,job.descriptor,job.decoded,job.error) && !job.cancelled.load();
  }catch(const std::exception& failure) {job.error=failure.what();}
  catch(...) {job.error="geometry worker decode exception";}
  return 0;
}
GeometryStream::State::~State() {
  for(auto& job:jobs)job->cancelled=true;
  for(auto& job:jobs)if(job->task)octaryn_native_schedule_runtime_task_destroy(job->task);
  if(config.scene_pool && scene_asset)config.scene_pool->release(scene_asset);
}
bool GeometryStream::State::poll_jobs() {
  for(auto& pointer:jobs) {
    auto& job=*pointer;
    if(!job.task || !octaryn_native_schedule_runtime_task_ready(job.task))continue;
    octaryn_native_schedule_runtime_report report{};
    const auto result=octaryn_native_schedule_runtime_task_result(job.task,&report);
    octaryn_native_schedule_runtime_task_destroy(job.task);job.task=nullptr;
    if(result!=0 || !job.success)return fail("geometry page decode failed: "+job.error);
    if(job.decoded.size()!=page_bytes)return fail("geometry worker returned wrong page size");
    job.ready=true;++stats.decoded_pages;
  }
  return true;
}
bool GeometryStream::State::start_jobs() {
  for(auto& pointer:jobs) {
    auto& job=*pointer;if(job.task || job.ready)continue;
    auto requests=residency->take_requests(1);if(requests.empty())break;
    const auto page=requests.front().page;
    auto handle=residency->reserve(page,page_bytes,pinned[page]);
    if(!handle && residency->evict_oldest())handle=residency->reserve(page,page_bytes,pinned[page]);
    if(!handle) {residency->feedback(requests);break;}
    job.page=page;job.handle=handle;job.path=path;job.descriptor=asset.pages[page];
    job.cancelled=false;job.success=false;job.ready=false;job.error.clear();job.decoded.clear();
    octaryn_native_schedule_runtime_job description{};
    description.job_id="virtual_geometry_page_decode";description.context=&job;description.execute=Job::execute;
    job.task=octaryn_native_schedule_runtime_submit_worker(scheduler.get(),&description,1);
    if(!job.task)return fail("geometry scheduler submission failed");
  }
  return true;
}
}
