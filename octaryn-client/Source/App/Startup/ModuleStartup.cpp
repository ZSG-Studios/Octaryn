#include "ModuleStartup.h"
#include "StartupWork.h"
#include "WorldSession.h"
#include "LoadingScreen.h"
#include "Controls.h"
#include <algorithm>
#include "octaryn_native_schedule_runtime.h"
#include <SDL3/SDL.h>
#include <exception>
#include <memory>
#include <stdexcept>
#include <cstdio>

namespace octaryn::client::app {
namespace {
struct Activation {
  StartupWork work;
  host::ModuleHostHooks hooks;
  std::exception_ptr failure;
  int result{-1};
  static int execute(void* argument) noexcept {
    auto& state=*static_cast<Activation*>(argument);
    try {state.result=host::module_host_start_worker(state.hooks,state.work);}
    catch(...) {state.failure=std::current_exception();}
    return 0;
  }
};
}
int start_world_module(WorldSession& world,const host::ModuleHostHooks& hooks) {
  if(host::module_host_active())return host::module_host_rebind(hooks)?0:-1;
  Activation state;state.hooks=hooks;
  using Runtime=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_destroy)>;
  using Task=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_task_destroy)>;
  Runtime runtime(octaryn_native_schedule_runtime_create(2,1),octaryn_native_schedule_runtime_destroy);
  if(!runtime)throw std::runtime_error("Cannot create module activation scheduler");
  octaryn_native_schedule_runtime_job job{};job.job_id="client_module_activation";job.context=&state;job.execute=Activation::execute;
  Task task(octaryn_native_schedule_runtime_submit_worker(runtime.get(),&job,1),octaryn_native_schedule_runtime_task_destroy);
  if(!task)throw std::runtime_error("Cannot schedule module activation");
  const auto started=SDL_GetTicksNS();auto previous=started,last_draw=Uint64{};Uint64 max_gap{};unsigned pumps{};
  std::exception_ptr presentation_failure;
  while(!octaryn_native_schedule_runtime_task_ready(task.get())) {
    const auto now=SDL_GetTicksNS();max_gap=std::max(max_gap,now-previous);previous=now;++pumps;
    state.work.pump();
    if(!presentation_failure) {
      try {
        const bool draw=!last_draw || now-last_draw>=1000000000ull/60;
        if(!present_loading(world.window,world.renderer,*world.ui,*world.controls,
            "Loading game systems...","",false,draw))state.work.cancel();
        if(draw)last_draw=SDL_GetTicksNS();
      }catch(...) {presentation_failure=std::current_exception();state.work.cancel();}
    }
    if(!world.controls->running)state.work.cancel();
    SDL_Delay(1);
  }
  octaryn_native_schedule_runtime_report report{};
  const int scheduled=octaryn_native_schedule_runtime_task_result(task.get(),&report);task.reset();
  const auto completed=SDL_GetTicksNS();max_gap=std::max(max_gap,completed-previous);
  std::printf("module_boot worker_jobs=%zu event_pumps=%u max_event_gap_ms=%.3f elapsed_ms=%.3f result=%d\n",
      report.worker_jobs,pumps,double(max_gap)/1e6,double(completed-started)/1e6,state.result);std::fflush(stdout);
  if(state.failure)std::rethrow_exception(state.failure);
  if(presentation_failure)std::rethrow_exception(presentation_failure);
  if(scheduled!=0)throw std::runtime_error("Module activation worker failed");
  return state.result;
}
}
