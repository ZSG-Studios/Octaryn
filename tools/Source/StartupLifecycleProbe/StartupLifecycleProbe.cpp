#include "StartupWork.h"
#include "octaryn_native_schedule_runtime.h"
#include <chrono>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {
using octaryn::client::app::StartupWork;
enum class CancelAt { None, Device, SavedSettings };
struct State {
  StartupWork work;
  std::thread::id main_thread;
  bool fail_main{},fail_settings{},created{},released{},cancelled{},failed{};
  bool settings_started{},settings_completed{},published_ready{};
  std::exception_ptr failure;
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
    if(s.settings_started && !s.settings_completed)throw std::runtime_error("settings still own renderer");
    ++s.callbacks;
    s.released=true;
  }
  static int execute(void* user) noexcept {
    auto& s=*static_cast<State*>(user);
    try {
      if(std::this_thread::get_id()==s.main_thread)throw std::runtime_error("initialization on main");
      StartupWork::main_thread(create,&s,&s.work);
      StartupWork::progress("device operation",&s.work);
      // A bounded stand-in for an uninterruptible driver/compiler operation.
      std::this_thread::sleep_for(std::chrono::milliseconds(80));
      StartupWork::progress("saved graphics settings",&s.work);
      s.settings_started=true;
      // Saved GI/temporal preparation still owns the renderer after creation.
      std::this_thread::sleep_for(std::chrono::milliseconds(80));
      s.settings_completed=true;
      if(s.fail_settings)throw std::runtime_error("injected settings allocation failure");
      StartupWork::progress("graphics ready",&s.work);
      s.published_ready=true;
    } catch(const StartupWork::Cancelled&) {s.cancelled=true;
    } catch(...) {s.failed=true;s.failure=std::current_exception();}
    if(s.created)StartupWork::main_thread(destroy,&s,&s.work);
    return 0;
  }
};
bool run(void* runtime,CancelAt cancel_at,bool fail_main,bool fail_settings=false) {
  State state;
  state.main_thread=std::this_thread::get_id();state.fail_main=fail_main;state.fail_settings=fail_settings;
  octaryn_native_schedule_runtime_job job{};
  job.job_id="startup_lifecycle";job.execute=State::execute;job.context=&state;
  using Task=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_task_destroy)>;
  Task task(octaryn_native_schedule_runtime_submit_worker(runtime,&job,1),octaryn_native_schedule_runtime_task_destroy);
  if(!task)return false;
  const auto started=std::chrono::steady_clock::now();
  unsigned pumps{},settings_pumps{};
  bool cancel_requested{};
  while(!octaryn_native_schedule_runtime_task_ready(task.get())) {
    const auto stage=state.work.pump();++pumps;
    if(stage=="saved graphics settings")++settings_pumps;
    if((cancel_at==CancelAt::Device && stage=="device operation") ||
        (cancel_at==CancelAt::SavedSettings && stage=="saved graphics settings")) {
      state.work.cancel();cancel_requested=true;
    }
    if(std::chrono::steady_clock::now()-started>std::chrono::seconds(5)) {
      std::fputs("startup_lifecycle deadlock\n",stderr);std::terminate();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  octaryn_native_schedule_runtime_report report{};
  const auto result=octaryn_native_schedule_runtime_task_result(task.get(),&report);
  task.reset();
  bool failure_message=!fail_settings;
  if(fail_settings && state.failure)try {std::rethrow_exception(state.failure);}
    catch(const std::runtime_error& error) {
      failure_message=std::string_view(error.what())=="injected settings allocation failure";
    } catch(...) {}
  const bool settings_expected=!fail_main && cancel_at!=CancelAt::Device;
  const bool ready_expected=cancel_at==CancelAt::None && !fail_main && !fail_settings;
  const bool passed=result==0 && report.worker_jobs==1 && state.failed==(fail_main || fail_settings) &&
      state.cancelled==cancel_requested && state.created==!fail_main && failure_message &&
      state.released==state.created && state.callbacks==(fail_main?1u:2u) &&
      state.settings_started==settings_expected && state.settings_completed==settings_expected &&
      state.published_ready==ready_expected && (!settings_expected || settings_pumps>=3);
  std::printf("startup_lifecycle cancel=%u main_failure=%u settings_failure=%u settings_pumps=%u pumps=%u worker_jobs=%zu passed=%u\n",
      unsigned(cancel_at),fail_main?1u:0u,fail_settings?1u:0u,settings_pumps,pumps,report.worker_jobs,passed?1u:0u);
  return passed;
}
}
int main() {
  using Runtime=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_destroy)>;
  Runtime runtime(octaryn_native_schedule_runtime_create(4,2),octaryn_native_schedule_runtime_destroy);
  if(!runtime)return 1;
  return run(runtime.get(),CancelAt::None,false) && run(runtime.get(),CancelAt::Device,false) &&
      run(runtime.get(),CancelAt::None,true) && run(runtime.get(),CancelAt::None,false,true) &&
      run(runtime.get(),CancelAt::SavedSettings,false)?0:1;
}
