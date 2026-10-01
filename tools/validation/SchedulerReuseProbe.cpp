#include "octaryn_native_schedule_runtime.h"
#include <atomic>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>
#include <cstdio>
#include <chrono>
#include <barrier>
#include <cstdlib>

struct Context {std::atomic<unsigned> calls{};int result{};std::barrier<>* gate{};};
static int execute(void* pointer) {
  auto& context=*static_cast<Context*>(pointer);
  ++context.calls;
  if(context.gate)context.gate->arrive_and_wait();
  std::this_thread::yield();
  return context.result;
}
static void run(void* runtime,Context& context,unsigned count) {
  const octaryn_native_schedule_runtime_job job{
    "probe.single",nullptr,0,nullptr,0,0,execute,&context};
  for(unsigned i=0;i<count;++i) {
    octaryn_native_schedule_runtime_report report{};
    const int result=octaryn_native_schedule_runtime_execute(runtime,&job,1,&report);
    if(result!=context.result || report.submitted_jobs!=1 || report.worker_jobs!=1 ||
       report.completed_jobs!=(result==0?1u:0u) || report.failed_job_index!=(result==0?-1:0))
      throw std::runtime_error("Cached scheduler changed execution or failure report");
  }
}
int main() {
  auto* runtime=octaryn_native_schedule_runtime_create(8,4);
  if(!runtime)return 1;
  Context success, failure;failure.result=-19;
  run(runtime,success,1000);run(runtime,failure,1000);run(runtime,success,1000);
  Context contexts[8];std::vector<std::future<void>> tasks;
  for(auto& context:contexts)tasks.push_back(std::async(std::launch::async,[&context,runtime] {run(runtime,context,1000);}));
  for(auto& task:tasks)task.get();
  for(auto& context:contexts)if(context.calls!=1000)throw std::runtime_error("Concurrent callback context was replaced");
  if(success.calls!=2000 || failure.calls!=1000)throw std::runtime_error("Sequential callback context was retained");
  octaryn_native_schedule_runtime_destroy(runtime);
  runtime=octaryn_native_schedule_runtime_create(2,2);
  Context nested[2];void* handles[2]{};std::barrier gate(2);
  for(unsigned index=0;index<2;++index) {
    nested[index].gate=&gate;
    const octaryn_native_schedule_runtime_job job{"probe.nested",nullptr,0,nullptr,0,0,execute,&nested[index]};
    handles[index]=octaryn_native_schedule_runtime_submit_worker(runtime,&job,1);
    if(!handles[index])throw std::runtime_error("Nested submit failed");
  }
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  for(void* handle:handles) {
    while(!octaryn_native_schedule_runtime_task_ready(handle)) {
      if(std::chrono::steady_clock::now()>deadline) {
        std::fputs("scheduler_reuse=failed nested_worker_deadlock\n",stderr);
        std::_Exit(1);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if(octaryn_native_schedule_runtime_task_result(handle,nullptr)!=0)throw std::runtime_error("Nested job failed");
    octaryn_native_schedule_runtime_task_destroy(handle);
  }
  for(const auto& context:nested)if(context.calls!=1)throw std::runtime_error("Nested job lost");
  octaryn_native_schedule_runtime_destroy(runtime);
  std::puts("scheduler_reuse=passed sequential=3000 concurrent=8000 failures=1000 nested_workers=2");
}
