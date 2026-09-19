#include "StartupWork.h"
#include "octaryn_native_schedule_runtime.h"
#include <chrono>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
using octaryn::client::app::StartupWork;
struct State {
  StartupWork work;
  std::thread::id main_thread;
  bool fail_main{},cancel{},created{},released{},cancelled{},failed{};
  unsigned callbacks{};
  static void create(void* user) {
    auto& s=*static_cast<State*>(user);
    if(std::this_thread::get_id()!=s.main_thread)throw std::runtime_error("wrong window thread");
    ++s.callbacks;
    if(s.fail_main)throw std::runtime_error("injected surface failure");
    s.created=true;
  }
  static void destroy(void* user) {
    auto& s=*static_cast<State*>(user);
    if(std::this_thread::get_id()!=s.main_thread)throw std::runtime_error("wrong teardown thread");
    ++s.callbacks;
    s.released=true;
  }
  static int execute(void* user) noexcept {
    auto& s=*static_cast<State*>(user);
    try {
      if(std::this_thread::get_id()==s.main_thread)throw std::runtime_error("initialization on main");
      StartupWork::main_thread(create,&s,&s.work);
      // A bounded stand-in for an uninterruptible driver/compiler operation.
      std::this_thread::sleep_for(std::chrono::milliseconds(80));
      StartupWork::progress("next real initialization stage",&s.work);
    } catch(const StartupWork::Cancelled&) {s.cancelled=true;
    } catch(...) {s.failed=true;}
    if(s.created)StartupWork::main_thread(destroy,&s,&s.work);
    return 0;
  }
};
bool run(void* runtime,bool cancel,bool fail_main) {
  State state;
  state.main_thread=std::this_thread::get_id();state.cancel=cancel;state.fail_main=fail_main;
  octaryn_native_schedule_runtime_job job{};
  job.job_id="startup_lifecycle";job.execute=State::execute;job.context=&state;
  using Task=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_task_destroy)>;
  Task task(octaryn_native_schedule_runtime_submit_worker(runtime,&job,1),octaryn_native_schedule_runtime_task_destroy);
  if(!task)return false;
  const auto started=std::chrono::steady_clock::now();
  unsigned pumps{};
  while(!octaryn_native_schedule_runtime_task_ready(task.get())) {
    state.work.pump();++pumps;
    if(cancel && std::chrono::steady_clock::now()-started>std::chrono::milliseconds(15))state.work.cancel();
    if(std::chrono::steady_clock::now()-started>std::chrono::seconds(5)) {
      std::fputs("startup_lifecycle deadlock\n",stderr);std::terminate();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  octaryn_native_schedule_runtime_report report{};
  const auto result=octaryn_native_schedule_runtime_task_result(task.get(),&report);
  const bool passed=result==0 && report.worker_jobs==1 && state.failed==fail_main &&
      state.cancelled==(cancel && !fail_main) && state.created==!fail_main &&
      state.released==state.created && state.callbacks==(fail_main?1u:2u) && (fail_main || pumps>=3);
  std::printf("startup_lifecycle cancel=%u main_failure=%u pumps=%u worker_jobs=%zu passed=%u\n",
      cancel?1u:0u,fail_main?1u:0u,pumps,report.worker_jobs,passed?1u:0u);
  return passed;
}
}
int main() {
  using Runtime=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_destroy)>;
  Runtime runtime(octaryn_native_schedule_runtime_create(4,2),octaryn_native_schedule_runtime_destroy);
  if(!runtime)return 1;
  return run(runtime.get(),false,false) && run(runtime.get(),true,false) &&
      run(runtime.get(),false,true)?0:1;
}
